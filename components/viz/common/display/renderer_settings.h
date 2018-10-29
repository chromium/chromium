// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_DISPLAY_RENDERER_SETTINGS_H_
#define COMPONENTS_VIZ_COMMON_DISPLAY_RENDERER_SETTINGS_H_

#include <stddef.h>

#include "build/build_config.h"
#include "components/viz/common/viz_common_export.h"
#include "ui/gfx/geometry/size.h"

namespace viz {

class VIZ_COMMON_EXPORT RendererSettings {
 public:
  RendererSettings();
  RendererSettings(const RendererSettings& other);
  ~RendererSettings();

  bool allow_antialiasing = true;
  bool force_antialiasing = false;
  bool force_blending_with_shaders = false;
  bool partial_swap_enabled = false;
  bool finish_rendering_on_resize = false;
  bool should_clear_root_render_pass = true;
  bool release_overlay_resources_after_gpu_query = false;
  bool tint_gl_composited_content = false;
  bool show_overdraw_feedback = false;
  bool enable_draw_occlusion = false;
  bool use_skia_renderer = false;
  bool use_skia_deferred_display_list = false;
  bool allow_overlays = true;
  bool dont_round_texture_sizes_for_pixel_tests = false;
  int highp_threshold_min = 0;
  bool auto_resize_output_surface = true;
  bool requires_alpha_channel = false;
  bool record_sk_picture = false;

  int slow_down_compositing_scale_factor = 1;

  // The required minimum size for DrawQuad to apply Draw Occlusion on.
  gfx::Size kMinimumDrawOcclusionSize = gfx::Size(60, 60);

#if defined(OS_ANDROID)
  // The screen size at renderer creation time.
  gfx::Size initial_screen_size = gfx::Size(0, 0);
#endif
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_DISPLAY_RENDERER_SETTINGS_H_
