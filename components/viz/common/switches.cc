// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/switches.h"

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "components/viz/common/constants.h"

namespace switches {

// The default number of the BeginFrames to wait to activate a surface with
// dependencies.
const char kDeadlineToSynchronizeSurfaces[] =
    "deadline-to-synchronize-surfaces";

// Force the use of a Delegated Ink renderer as specified by
// the command line argument, rather than using system details. Acceptable
// values are: skia, system, none. Default to skia.
const char kDelegatedInkRenderer[] = "delegated-ink-renderer";

// Disables reporting of frame timing via ADPF, even if supported on the device.
const char kDisableAdpf[] = "disable-adpf";

// Disables begin frame limiting in both cc scheduler and display scheduler.
// Also implies --disable-gpu-vsync (see //ui/gl/gl_switches.h).
// TODO(ananta/jonross/sunnyps)
// http://crbug.com/346931323
// We should remove or change this once VRR support is implemented for
// Windows and other platforms potentially.
const char kDisableFrameRateLimit[] = "disable-frame-rate-limit";

// Sets the number of max pending frames in the GL buffer queue to 1.
const char kDoubleBufferCompositing[] = "double-buffer-compositing";

// Enable compositing individual elements via hardware overlays when
// permitted by device.
// Setting the flag to "single-fullscreen" will try to promote a single
// fullscreen overlay and use it as main framebuffer where possible.
const char kEnableHardwareOverlays[] = "enable-hardware-overlays";

// Effectively disables pipelining of compositor frame production stages by
// waiting for each stage to finish before completing a frame.
const char kRunAllCompositorStagesBeforeDraw[] =
    "run-all-compositor-stages-before-draw";

// Adds a DebugBorderDrawQuad to the top of the root RenderPass showing the
// damage rect after surface aggregation. Note that when enabled this feature
// sets the entire output rect as damaged after adding the quad to highlight the
// real damage rect, which could hide damage rect problems.
const char kShowAggregatedDamage[] = "show-aggregated-damage";

// Modulates the debug compositor tint color so that damage and page flip
// updates are made clearly visible. This feature was useful in determining the
// root cause of https://b.corp.google.com/issues/183260320 . The tinting flag
// "tint-composited-content" must also be enabled for this flag to used.
const char kTintCompositedContentModulate[] =
    "tint-composited-content-modulate";

// Show debug borders for DC layers - red for overlays and blue for underlays.
// The debug borders are offset from the layer rect by a few pixels for clarity.
const char kShowDCLayerDebugBorders[] = "show-dc-layer-debug-borders";

std::optional<uint32_t> GetDeadlineToSynchronizeSurfaces() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kRunAllCompositorStagesBeforeDraw)) {
    // In full-pipeline mode, surface deadlines should always be unlimited.
    return std::nullopt;
  }
  std::string deadline_to_synchronize_surfaces_string =
      command_line->GetSwitchValueASCII(
          switches::kDeadlineToSynchronizeSurfaces);
  if (deadline_to_synchronize_surfaces_string.empty())
    return viz::kDefaultActivationDeadlineInFrames;

  uint32_t activation_deadline_in_frames;
  if (!base::StringToUint(deadline_to_synchronize_surfaces_string,
                          &activation_deadline_in_frames)) {
    return std::nullopt;
  }
  return activation_deadline_in_frames;
}

std::optional<DelegatedInkRendererMode> GetDelegatedInkRendererMode() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kDelegatedInkRenderer)) {
    return std::nullopt;
  }
  std::string mode =
      command_line->GetSwitchValueASCII(switches::kDelegatedInkRenderer);
  if (mode == "system") {
    return DelegatedInkRendererMode::kSystem;
  }
  if (mode == "none") {
    return DelegatedInkRendererMode::kNone;
  }
  if (mode == "skia") {
    return DelegatedInkRendererMode::kSkia;
  }
  // Default to system.
  return DelegatedInkRendererMode::kSystem;
}

}  // namespace switches
