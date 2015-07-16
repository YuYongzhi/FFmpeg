/*
 * Copyright (c) 2015 Yu Xiaolei <dreifachstein@gmail.com>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * convert straight alpha to premultiplied alpha
 */


#include <math.h>

#include "libavutil/libm.h"
#include "libavutil/pixfmt.h"
#include "avfilter.h"
#include "formats.h"
#include "internal.h"


static int query_formats(AVFilterContext *ctx)
{
    static const enum AVPixelFormat default_fmts[] = {
        AV_PIX_FMT_RGBA, AV_PIX_FMT_BGRA,
        AV_PIX_FMT_ARGB, AV_PIX_FMT_ABGR,
        AV_PIX_FMT_NONE
    };
    AVFilterFormats *default_formats = ff_make_format_list(default_fmts);
    ff_formats_ref(default_formats, &ctx->inputs[0]->out_formats);
    ff_formats_ref(default_formats, &ctx->outputs[0]->in_formats);
    return 0;
}

static int config_input_main(AVFilterLink *inlink)
{
    return 0;
}

static int config_output(AVFilterLink *outlink)
{
    AVFilterContext *ctx = outlink->src;
    AVFilterLink *inlink = ctx->inputs[0];

    outlink->w = inlink->w;
    outlink->h = inlink->h;
    outlink->time_base = inlink->time_base;
    outlink->sample_aspect_ratio = inlink->sample_aspect_ratio;
    outlink->frame_rate = inlink->frame_rate;
    return 0;
}

static void update_frame_xxxa(AVFrame *buf)
{
    int const stride = buf->linesize[0];
    int const w = buf->width;
    int const h = buf->height;

    uint8_t* data = buf->data[0];

    for (int y = 0; y < h; y ++) {
        for (int x = 0; x < w * 4; x += 4) {
            uint8_t alpha = data[x + 3];

            data[x + 0] = RSHIFT(data[x + 0] * alpha, 8);
            data[x + 1] = RSHIFT(data[x + 1] * alpha, 8);
            data[x + 2] = RSHIFT(data[x + 2] * alpha, 8);
        }

        data = &data[stride];
    }
}

static void update_frame_axxx(AVFrame *buf)
{
    int const stride = buf->linesize[0];
    int const w = buf->width;
    int const h = buf->height;

    uint8_t* data = buf->data[0];

    for (int y = 0; y < h; y ++) {
        for (int x = 0; x < w * 4; x += 4) {
            uint8_t alpha = data[x];

            data[x + 1] = RSHIFT(data[x + 1] * alpha, 8);
            data[x + 2] = RSHIFT(data[x + 2] * alpha, 8);
            data[x + 3] = RSHIFT(data[x + 3] * alpha, 8);
        }

        data = &data[stride];
    }
}

static int filter_frame(AVFilterLink *inlink, AVFrame *buf)
{
    AVFilterContext *ctx = inlink->dst;

    int rv = av_frame_make_writable(buf);

    if (rv != 0) {
        return rv;
    }

    switch (buf->format) {
    case AV_PIX_FMT_RGBA:
    case AV_PIX_FMT_BGRA:
        update_frame_xxxa(buf);
        break;
    case AV_PIX_FMT_ABGR:
    case AV_PIX_FMT_ARGB:
        update_frame_axxx(buf);
        break;
    default:
        av_frame_unref(buf);
        return AVERROR_BUG;
    }

    return ff_filter_frame(ctx->outputs[0], buf);
}

static int request_frame(AVFilterLink *outlink)
{
    AVFilterContext *ctx = outlink->src;
    return ff_request_frame(ctx->inputs[0]);;
}

static const AVFilterPad alphamult_inputs[] = {
    {
        .name             = "default",
        .type             = AVMEDIA_TYPE_VIDEO,
        .config_props     = config_input_main,
        .filter_frame     = filter_frame,
        .needs_writable   = 1,
    },
    { NULL }
};

static const AVFilterPad alphamult_outputs[] = {
    {
        .name          = "default",
        .type          = AVMEDIA_TYPE_VIDEO,
        .config_props  = config_output,
        .request_frame = request_frame,
    },
    { NULL }
};

AVFilter ff_vf_alphamult = {
    .name           = "alphamult",
    .description    = NULL_IF_CONFIG_SMALL("Convert straight alpha to premultiplied alpha"),
    .query_formats  = query_formats,
    .inputs         = alphamult_inputs,
    .outputs        = alphamult_outputs,
};
