// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/renderer_settings_creation.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/features.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_MACOSX)
#include "ui/base/cocoa/remote_layer_api.h"
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
#if defined(OS_WIN)
  renderer_settings.finish_rendering_on_resize = true;
#elif defined(OS_MACOSX)
  renderer_settings.release_overlay_resources_after_gpu_query = true;
  renderer_settings.auto_resize_output_surface = false;
#endif
  renderer_settings.tint_gl_composited_content =
      command_line->HasSwitch(switches::kTintGlCompositedContent);
  renderer_settings.show_overdraw_feedback =
      command_line->HasSwitch(switches::kShowOverdrawFeedback);
  renderer_settings.enable_draw_occlusion = features::IsDrawOcclusionEnabled();
  renderer_settings.allow_antialiasing =
      !command_line->HasSwitch(switches::kDisableCompositedAntialiasing);
  renderer_settings.use_skia_renderer = features::IsUsingSkiaRenderer();
  renderer_settings.use_skia_deferred_display_list =
      features::IsUsingSkiaDeferredDisplayList();
#if defined(OS_MACOSX)
  renderer_settings.allow_overlays =
      ui::RemoteLayerAPISupported() &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableMacOverlays);
#endif
  renderer_settings.record_sk_picture = features::IsRecordingSkPicture();

  if (command_line->HasSwitch(switches::kSlowDownCompositingScaleFactor)) {
    const int kMinSlowDownScaleFactor = 1;
    const int kMaxSlowDownScaleFactor = 1000;
    GetSwitchValueAsInt(command_line, switches::kSlowDownCompositingScaleFactor,
                        kMinSlowDownScaleFactor, kMaxSlowDownScaleFactor,
                        &renderer_settings.slow_down_compositing_scale_factor);
  }

  return renderer_settings;
}

}  // namespace viz
