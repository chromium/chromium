// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_DISPLAY_RENDERER_SETTINGS_H_
#define COMPONENTS_VIZ_COMMON_DISPLAY_RENDERER_SETTINGS_H_

#include <stddef.h>

#include <vector>

#include "build/build_config.h"
#include "components/viz/common/display/overlay_strategy.h"
#include "components/viz/common/viz_common_export.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/color_space.h"
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
  bool should_clear_root_render_pass = true;
  bool release_overlay_resources_after_gpu_query = false;
  bool dont_round_texture_sizes_for_pixel_tests = false;
  int highp_threshold_min = 0;
  bool auto_resize_output_surface = true;
  bool requires_alpha_channel = false;
  bool disable_render_pass_bypassing = false;

  int slow_down_compositing_scale_factor = 1;

  struct VIZ_COMMON_EXPORT OcclusionCullerSettings {
    // The maximum number of occluding rects to track during occlusion culling.
    int maximum_occluder_complexity = 10;
    // The maximum number (exclusive) of quads one draw quad may be split into
    // during occlusion culling. e.g. an L-shaped visible region split into two
    // quads
    int quad_split_limit = 5;
    // The minimum number of fragments that would not be drawn if a quads was
    // split into multiple quads during occlusion culling.
    int minimum_fragments_reduced = 128 * 128;
  };

  OcclusionCullerSettings occlusion_culler_settings;

#if BUILDFLAG(IS_ANDROID)
  // The screen size at renderer creation time.
  gfx::Size initial_screen_size = gfx::Size(0, 0);

  gfx::ColorSpace color_space;
#endif

#if BUILDFLAG(IS_OZONE)
  // A list of overlay strategies that should be tried. If the list is empty
  // then overlays aren't supported.
  std::vector<OverlayStrategy> overlay_strategies;
#endif
#if BUILDFLAG(IS_MAC)
  // CGDirectDisplayID for the screen on which the browser is currently
  // displayed.
  int64_t display_id = display::kInvalidDisplayId;
#endif
};

// This is a set of debug flags that can be changed at runtime, so that we can
// trigger developer features. (The above RendererSettings cannot be changed in
// viz after initialization.) It has a single instance in viz (basically a
// singleton, owned by FrameSinkManagerImpl), while other objects keep a
// reference to it. On the host size the single instance is owned by
// HostFrameSinkManager.
struct VIZ_COMMON_EXPORT DebugRendererSettings {
  bool tint_composited_content = false;
  bool tint_composited_content_modulate = false;
  bool show_overdraw_feedback = false;
  bool show_dc_layer_debug_borders = false;
  bool show_aggregated_damage = false;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_DISPLAY_RENDERER_SETTINGS_H_
