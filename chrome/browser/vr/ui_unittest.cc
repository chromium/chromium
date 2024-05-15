// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene_creator.h"

#include "base/numerics/ranges.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/vr/elements/indicator_spec.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/elements/vector_icon.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/test/ui_test.h"
#include "chrome/browser/vr/ui_renderer.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace vr {

namespace {
constexpr float kSmallDelaySeconds = 0.1f;

MATCHER_P2(SizeFsAreApproximatelyEqual, other, tolerance, "") {
  return base::IsApproximatelyEqual(arg.width(), other.width(), tolerance) &&
         base::IsApproximatelyEqual(arg.height(), other.height(), tolerance);
}

}  // namespace

TEST_F(UiTest, CaptureToasts) {
  auto browser_ui = ui_->GetBrowserUiWeakPtr();

  for (auto& spec : GetIndicatorSpecs()) {
    for (int i = 0; i < 3; ++i) {
#if !BUILDFLAG(IS_ANDROID)
      if (i == 1)  // Skip background tabs for non-Android platforms.
        continue;
#endif
      // Reinitialize the WebVR state to force the indicator to trigger each
      // time.
      model_->web_vr.has_received_permissions = false;
      model_->web_vr.state = kWebVrAwaitingFirstFrame;
      // Advance the frame to ensure that this state has propagated.
      AdvanceFrame();
      ui_->GetSchedulerUiPtr()->OnWebXrFrameAvailable();

      CapturingStateModel active_capturing;
      CapturingStateModel background_capturing;
      CapturingStateModel potential_capturing;
      active_capturing.*spec.signal = i == 0;
      // High accuracy location cannot be used in a background tab.
      // Also, capturing USB and Midi cannot be done in background tab.
      background_capturing.*spec.signal =
          i == 1 && spec.name != kLocationAccessIndicator &&
          spec.name != kUsbConnectedIndicator &&
          spec.name != kMidiConnectedIndicator;
      potential_capturing.*spec.signal =
          i == 2 && spec.name != kUsbConnectedIndicator;

      int string_id = 0;
      switch (i) {
        case 0:
          string_id = spec.resource_string;
          break;
        case 1:
          string_id = spec.background_resource_string;
          break;
        case 2:
          string_id = spec.potential_resource_string;
          break;
        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }

      browser_ui->SetCapturingState(active_capturing, background_capturing,
                                    potential_capturing);
      // Advance the frame to ensure that the capturing state has propagated.
      AdvanceFrame();
      EXPECT_TRUE(IsVisible(spec.webvr_name) == (string_id != 0));
      EXPECT_TRUE(RunForSeconds(kWindowsInitialIndicatorsTimeoutSeconds +
                                kSmallDelaySeconds));
    }
  }
}

TEST_F(UiTest, UiModeWebXr) {
  VerifyOnlyElementsVisible("WebXR", {kWebVrBackground});
}

TEST_F(UiTest, UiUpdatesForWebXR) {
  model_->active_capturing.audio_capture_enabled = true;
  model_->active_capturing.video_capture_enabled = true;
  model_->active_capturing.screen_capture_enabled = true;
  model_->active_capturing.location_access_enabled = true;
  model_->active_capturing.bluetooth_connected = true;
  model_->active_capturing.usb_connected = true;
  model_->active_capturing.midi_connected = true;

  VerifyOnlyElementsVisible("Elements hidden",
                            std::set<UiElementName>{kWebVrBackground});
}

TEST_F(UiTest, UiUpdateTransitionToWebVR) {
  model_->active_capturing.audio_capture_enabled = true;
  model_->active_capturing.video_capture_enabled = true;
  model_->active_capturing.screen_capture_enabled = true;
  model_->active_capturing.location_access_enabled = true;
  model_->active_capturing.bluetooth_connected = true;
  model_->active_capturing.usb_connected = true;
  model_->active_capturing.midi_connected = true;

  // Make a WebXr Frame available
  ui_->GetSchedulerUiPtr()->OnWebXrFrameAvailable();

  // All elements should be hidden.
  VerifyOnlyElementsVisible("Elements hidden", std::set<UiElementName>{});
}

TEST_F(UiTest, WebXrTimeout) {
  EXPECT_EQ(model_->web_vr.state, kWebVrAwaitingFirstFrame);

  RunForMs(500);
  VerifyVisibility(
      {kWebVrTimeoutSpinner, kWebVrTimeoutMessage, kWebVrTimeoutMessageLayout,
       kWebVrTimeoutMessageIcon, kWebVrTimeoutMessageText},
      false);
  VerifyVisibility(
      {
          kWebVrBackground,
      },
      true);

  model_->web_vr.state = kWebVrTimeoutImminent;
  RunForMs(500);
  VerifyVisibility({kWebVrTimeoutMessage, kWebVrTimeoutMessageLayout,
                    kWebVrTimeoutMessageIcon, kWebVrTimeoutMessageText},
                   false);
  VerifyVisibility(
      {
          kWebVrTimeoutSpinner, kWebVrBackground,
      },
      true);

  model_->web_vr.state = kWebVrTimedOut;
  RunForMs(500);
  VerifyVisibility(
      {
          kWebVrTimeoutSpinner,
      },
      false);
  VerifyVisibility(
      {kWebVrBackground, kWebVrTimeoutMessage, kWebVrTimeoutMessageLayout,
       kWebVrTimeoutMessageIcon, kWebVrTimeoutMessageText},
      true);
}

TEST_F(UiTest, SteadyState) {
  RunForSeconds(10.0f);
  // Should have reached steady state.
  EXPECT_FALSE(AdvanceFrame());
}

}  // namespace vr
