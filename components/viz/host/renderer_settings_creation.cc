// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/renderer_settings_creation.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/display/overlay_strategy.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"
#include "ui/base/ui_base_switches.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace viz {

namespace {

bool GetSwitchValueAsInt(const base::CommandLine* command_line,
                         const std::string& switch_string,
                         int min_value,
                         int max_value,
                         int* result) {
  std::string string_value = command_line->GetSwitchValueASCII(switch_string);
  int int_value;
  if (base::StringToInt(string_value, &int_value) && int_value >= min_value &&
      int_value <= max_value) {
    *result = int_value;
    return true;
  } else {
    LOG(WARNING) << "Failed to parse switch " << switch_string << ": "
                 << string_value;
    return false;
  }
}

}  // namespace

RendererSettings CreateRendererSettings() {
  RendererSettings renderer_settings;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  renderer_settings.partial_swap_enabled =
      !command_line->HasSwitch(switches::kUIDisablePartialSwap);

#if BUILDFLAG(IS_APPLE)
  renderer_settings.release_overlay_resources_after_gpu_query = true;
  renderer_settings.auto_resize_output_surface = false;
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  renderer_settings.auto_resize_output_surface = false;
#endif
  renderer_settings.allow_antialiasing =
      !command_line->HasSwitch(switches::kDisableCompositedAntialiasing);

  if (command_line->HasSwitch(switches::kSlowDownCompositingScaleFactor)) {
    const int kMinSlowDownScaleFactor = 1;
    const int kMaxSlowDownScaleFactor = 1000;
    GetSwitchValueAsInt(command_line, switches::kSlowDownCompositingScaleFactor,
                        kMinSlowDownScaleFactor, kMaxSlowDownScaleFactor,
                        &renderer_settings.slow_down_compositing_scale_factor);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  renderer_settings.occlusion_culler_settings.quad_split_limit =
      features::DrawQuadSplitLimit();
#endif

#if BUILDFLAG(IS_OZONE)
  if (command_line->HasSwitch(switches::kEnableHardwareOverlays)) {
    renderer_settings.overlay_strategies = ParseOverlayStrategies(
        command_line->GetSwitchValueASCII(switches::kEnableHardwareOverlays));
  } else {
    auto& host_properties =
        ui::OzonePlatform::GetInstance()->GetPlatformRuntimeProperties();
    if (host_properties.supports_overlays) {
      renderer_settings.overlay_strategies = {OverlayStrategy::kFullscreen,
                                              OverlayStrategy::kSingleOnTop,
                                              OverlayStrategy::kUnderlay};
    }
  }
#endif

  return renderer_settings;
}

DebugRendererSettings CreateDefaultDebugRendererSettings() {
  DebugRendererSettings result;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  result.tint_composited_content =
      command_line->HasSwitch(switches::kTintCompositedContent);
  result.tint_composited_content_modulate =
      command_line->HasSwitch(switches::kTintCompositedContentModulate);
  result.show_overdraw_feedback =
      command_line->HasSwitch(switches::kShowOverdrawFeedback);
  result.show_dc_layer_debug_borders =
      command_line->HasSwitch(switches::kShowDCLayerDebugBorders);
  result.show_aggregated_damage =
      command_line->HasSwitch(switches::kShowAggregatedDamage);
  return result;
}

}  // namespace viz
