// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_HDR_VIDEO_PASSTHROUGH_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_HDR_VIDEO_PASSTHROUGH_H_

#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/hdr_metadata.h"

namespace viz {

class DisplayResourceProvider;
class DrawQuad;

// Helpers for passing a single dominant HDR video's metadata through to the
// system compositor instead of tone mapping it in viz.
//
// When a full screen HDR10 video is the only HDR content on screen, the video
// can be composited into the primary plane without tone mapping, and the
// video's HDR metadata can be attached to the primary plane. A system
// compositor that tone maps using client metadata can then tone map the video
// using its real mastering display and content light level metadata, which
// matches what dedicated video players do.
//
// The rest of the primary plane (e.g. UI drawn over the video) is subject to
// the same tone mapping by the system compositor. Requiring that the video is
// full screen keeps the amount of such content small. Full screen video is
// detected the same way as for the Windows full screen video optimization
// (OverlayProcessorWin::TryPromoteFullScreenVideo), see
// full_screen_video_util.h.

// Returns the quad in the root render pass of `render_passes` whose HDR
// metadata should be passed through, or nullptr if passthrough isn't possible.
// Passthrough requires that:
//  * `output_color_space` uses the PQ transfer function.
//  * No non-root render pass has HDR content.
//  * Exactly one quad in the root render pass has an HDR resource, and it is
//    an opaque, unblended texture quad with a PQ color space, no AGTM
//    metadata and an unconstrained dynamic range limit.
//  * `is_page_fullscreen` is true, and the quad is a full screen video: its
//    transform is a positive scale and/or translation, it is scaled to fit and
//    centered in the root render pass's output rect (possibly letterboxed or
//    pillarboxed) and not clipped, and the quad directly below it is black or
//    tiled content covering the output from its origin (see
//    GetFullScreenVideoRect() and IsPossibleFullScreenLetterboxing()).
VIZ_SERVICE_EXPORT const DrawQuad* FindHdrVideoPassthroughQuad(
    DisplayResourceProvider* resource_provider,
    const AggregatedRenderPassList& render_passes,
    const gfx::ColorSpace& output_color_space,
    bool is_page_fullscreen);

// Returns the HDR metadata to attach to the primary plane when passing through
// content with `content_color_space` and `content_hdr_metadata`, composited
// into an output whose SDR white level is `output_sdr_white_level` nits.
// Luminance values in the content metadata are relative to the content's SDR
// white level. Compositing anchors the content's SDR white level to the
// output's, so the luminance values are scaled accordingly. Unspecified
// metadata is populated with defaults.
VIZ_SERVICE_EXPORT gfx::HDRMetadata ComputeHdrVideoPassthroughMetadata(
    const gfx::ColorSpace& content_color_space,
    const gfx::HDRMetadata& content_hdr_metadata,
    float output_sdr_white_level);

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_HDR_VIDEO_PASSTHROUGH_H_
