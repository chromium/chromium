// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene_creator.h"

#include "base/numerics/ranges.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/vr/elements/button.h"
#include "chrome/browser/vr/elements/content_element.h"
#include "chrome/browser/vr/elements/disc_button.h"
#include "chrome/browser/vr/elements/indicator_spec.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/repositioner.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/elements/vector_icon.h"
#include "chrome/browser/vr/model/assets.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/speech_recognizer.h"
#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/test/mock_ui_browser_interface.h"
#include "chrome/browser/vr/test/ui_test.h"
#include "chrome/browser/vr/ui_renderer.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace vr {

namespace {
const std::set<UiElementName> kFloorCeilingBackgroundElements = {
    kSolidBackground, kCeiling, kFloor,
};
const std::set<UiElementName> kElementsVisibleInBrowsing = {
    kSolidBackground,
    kCeiling,
    kFloor,
    kContentFrame,
    kContentFrameHitPlane,
    kContentQuad,
    kBackplane,
    kUrlBar,
    kUrlBarBackButton,
    kUrlBarLeftSeparator,
    kUrlBarOriginRegion,
    kUrlBarSecurityButton,
    kUrlBarUrlText,
    kUrlBarRightSeparator,
    kUrlBarOverflowButton,
    kController,
    kLaser,
    kControllerTouchpadButton,
    kControllerAppButton,
    kControllerHomeButton,
    kControllerBatteryDot0,
    kControllerBatteryDot1,
    kControllerBatteryDot2,
    kControllerBatteryDot3,
    kControllerBatteryDot4,
    kIndicatorBackplane,
};
const std::set<UiElementName> kElementsVisibleWithExitWarning = {
    kScreenDimmer, kExitWarningBackground, kExitWarningText};
const std::set<UiElementName> kElementsVisibleWithVoiceSearch = {
    kSpeechRecognitionListening, kSpeechRecognitionMicrophoneIcon,
    kSpeechRecognitionListeningCloseButton, kSpeechRecognitionCircle,
    kSpeechRecognitionListeningGrowingCircle};
const std::set<UiElementName> kElementsVisibleWithVoiceSearchResult = {
    kSpeechRecognitionResult, kSpeechRecognitionCircle,
    kSpeechRecognitionMicrophoneIcon, kSpeechRecognitionResultBackplane};

constexpr float kSmallDelaySeconds = 0.1f;

MATCHER_P2(SizeFsAreApproximatelyEqual, other, tolerance, "") {
  return base::IsApproximatelyEqual(arg.width(), other.width(), tolerance) &&
         base::IsApproximatelyEqual(arg.height(), other.height(), tolerance);
}

void VerifyNoHitTestableElementInSubtree(UiElement* element) {
  EXPECT_FALSE(element->IsHitTestable());
  for (auto& child : element->children())
    VerifyNoHitTestableElementInSubtree(child.get());
}

}  // namespace

TEST_F(UiTest, WebVrToastStateTransitions) {
  // Tests toast not showing when directly entering VR though WebVR
  // presentation.
  CreateScene(kInWebVr);
  EXPECT_FALSE(IsVisible(kWebVrExclusiveScreenToast));

  CreateScene(kNotInWebVr);
  EXPECT_FALSE(IsVisible(kWebVrExclusiveScreenToast));

  auto browser_ui = ui_->GetBrowserUiWeakPtr();
  browser_ui->SetWebVrMode(true);
  ui_->GetSchedulerUiPtr()->OnWebXrFrameAvailable();
  browser_ui->SetCapturingState(CapturingStateModel(), CapturingStateModel(),
                                CapturingStateModel());
  EXPECT_TRUE(IsVisible(kWebVrExclusiveScreenToast));

  browser_ui->SetWebVrMode(false);
  EXPECT_FALSE(IsVisible(kWebVrExclusiveScreenToast));

  browser_ui->SetWebVrMode(true);
  EXPECT_FALSE(IsVisible(kWebVrExclusiveScreenToast));

  browser_ui->SetWebVrMode(false);
  EXPECT_FALSE(IsVisible(kWebVrExclusiveScreenToast));
}

TEST_F(UiTest, WebVrToastTransience) {
  CreateScene(kNotInWebVr);

  auto browser_ui = ui_->GetBrowserUiWeakPtr();
  browser_ui->SetWebVrMode(true);
  ui_->GetSchedulerUiPtr()->OnWebXrFrameAvailable();
  browser_ui->SetCapturingState(CapturingStateModel(), CapturingStateModel(),
                                CapturingStateModel());
  EXPECT_TRUE(IsVisible(kWebVrExclusiveScreenToast));
  EXPECT_TRUE(RunForSeconds(kWindowsInitialIndicatorsTimeoutSeconds +
                            kSmallDelaySeconds));
  EXPECT_FALSE(IsVisible(kWebVrExclusiveScreenToast));

  browser_ui->SetWebVrMode(false);
  EXPECT_FALSE(IsVisible(kWebVrExclusiveScreenToast));
}

TEST_F(UiTest, CaptureToasts) {
  CreateScene(kNotInWebVr);
  auto browser_ui = ui_->GetBrowserUiWeakPtr();

  for (auto& spec : GetIndicatorSpecs()) {
    for (int i = 0; i < 3; ++i) {
#if !BUILDFLAG(IS_ANDROID)
      if (i == 1)  // Skip background tabs for non-Android platforms.
        continue;
#endif
      browser_ui->SetWebVrMode(true);
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
          NOTREACHED();
          break;
      }

      browser_ui->SetCapturingState(active_capturing, background_capturing,
                                    potential_capturing);
      EXPECT_TRUE(IsVisible(kWebVrExclusiveScreenToast));
      EXPECT_TRUE(IsVisible(spec.webvr_name) == (string_id != 0));
      EXPECT_TRUE(RunForSeconds(kWindowsInitialIndicatorsTimeoutSeconds +
                                kSmallDelaySeconds));
      EXPECT_FALSE(IsVisible(kWebVrExclusiveScreenToast));

      browser_ui->SetWebVrMode(false);
      EXPECT_FALSE(IsVisible(kWebVrExclusiveScreenToast));
      EXPECT_FALSE(IsVisible(spec.webvr_name));
    }
  }
}

TEST_F(UiTest, UiUpdatesForIncognito) {
  CreateScene(kNotInWebVr);
  auto browser_ui = ui_->GetBrowserUiWeakPtr();

  // Hold onto the background color to make sure it changes.
  SkColor initial_background = SK_ColorBLACK;
  GetBackgroundColor(&initial_background);
  EXPECT_EQ(
      ColorScheme::GetColorScheme(ColorScheme::kModeNormal).world_background,
      initial_background);
  browser_ui->SetFullscreen(true);

  // Make sure background has changed for fullscreen.
  SkColor fullscreen_background = SK_ColorBLACK;
  GetBackgroundColor(&fullscreen_background);
  EXPECT_EQ(ColorScheme::GetColorScheme(ColorScheme::kModeFullscreen)
                .world_background,
            fullscreen_background);

  model_->incognito = true;
  // Make sure background remains fullscreen colored.
  SkColor incognito_background = SK_ColorBLACK;
  GetBackgroundColor(&incognito_background);
  EXPECT_EQ(ColorScheme::GetColorScheme(ColorScheme::kModeFullscreen)
                .world_background,
            incognito_background);

  model_->incognito = false;
  SkColor no_longer_incognito_background = SK_ColorBLACK;
  GetBackgroundColor(&no_longer_incognito_background);
  EXPECT_EQ(fullscreen_background, no_longer_incognito_background);

  browser_ui->SetFullscreen(false);
  SkColor no_longer_fullscreen_background = SK_ColorBLACK;
  GetBackgroundColor(&no_longer_fullscreen_background);
  EXPECT_EQ(initial_background, no_longer_fullscreen_background);

  // Incognito, but not fullscreen, should show incognito colors.
  model_->incognito = true;
  SkColor incognito_again_background = SK_ColorBLACK;
  GetBackgroundColor(&incognito_again_background);
  EXPECT_EQ(
      ColorScheme::GetColorScheme(ColorScheme::kModeIncognito).world_background,
      incognito_again_background);

  model_->incognito = false;
  SkColor no_longer_incognito_again_background = SK_ColorBLACK;
  GetBackgroundColor(&no_longer_incognito_again_background);
  EXPECT_EQ(initial_background, no_longer_incognito_again_background);
}

TEST_F(UiTest, UiModeWebVr) {
  CreateScene(kNotInWebVr);
  auto browser_ui = ui_->GetBrowserUiWeakPtr();

  EXPECT_EQ(model_->ui_modes.size(), 1u);
  EXPECT_EQ(model_->ui_modes.back(), kModeBrowsing);
  VerifyOnlyElementsVisible("Initial", kElementsVisibleInBrowsing);

  browser_ui->SetWebVrMode(true);
  EXPECT_EQ(model_->ui_modes.size(), 2u);
  EXPECT_EQ(model_->ui_modes[1], kModeWebVr);
  EXPECT_EQ(model_->ui_modes[0], kModeBrowsing);
  VerifyOnlyElementsVisible("WebVR", {kWebVrBackground});

  browser_ui->SetWebVrMode(false);
  EXPECT_EQ(model_->ui_modes.size(), 1u);
  EXPECT_EQ(model_->ui_modes.back(), kModeBrowsing);
  VerifyOnlyElementsVisible("Browsing after WebVR", kElementsVisibleInBrowsing);
}

TEST_F(UiTest, HostedUiInWebVr) {
  CreateScene(kInWebVr);
  VerifyVisibility({kWebVrHostedUi, kWebVrFloor}, false);

  ui_->SetAlertDialogEnabled(true, nullptr, 0, 0);
  AdvanceFrame();
  VerifyVisibility({kWebVrHostedUi, kWebVrBackground, kWebVrFloor}, true);

  ui_->SetAlertDialogEnabled(false, nullptr, 0, 0);
  AdvanceFrame();
  VerifyVisibility({kWebVrHostedUi, kWebVrFloor}, false);
}

TEST_F(UiTest, UiUpdatesForWebVR) {
  CreateScene(kInWebVr);

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

// This test verifies that we ignore the WebVR frame when we're not expecting
// WebVR presentation. You can get an unexpected frame when for example, the
// user hits the menu button to exit WebVR mode, but the site continues to pump
// frames. If the frame is not ignored, our UI will think we're in WebVR mode.
TEST_F(UiTest, WebVrFramesIgnoredWhenUnexpected) {
  CreateScene(kInWebVr);

  ui_->GetSchedulerUiPtr()->OnWebXrFrameAvailable();
  VerifyOnlyElementsVisible("Elements hidden", std::set<UiElementName>{});
  // Disable WebVR mode.
  ui_->GetBrowserUiWeakPtr()->SetWebVrMode(false);

  // New frame available after exiting WebVR mode.
  ui_->GetSchedulerUiPtr()->OnWebXrFrameAvailable();
  VerifyOnlyElementsVisible("Browser visible", kElementsVisibleInBrowsing);
}

TEST_F(UiTest, UiUpdateTransitionToWebVR) {
  CreateScene(kNotInWebVr);
  model_->active_capturing.audio_capture_enabled = true;
  model_->active_capturing.video_capture_enabled = true;
  model_->active_capturing.screen_capture_enabled = true;
  model_->active_capturing.location_access_enabled = true;
  model_->active_capturing.bluetooth_connected = true;
  model_->active_capturing.usb_connected = true;
  model_->active_capturing.midi_connected = true;

  // Transition to WebVR mode
  ui_->GetBrowserUiWeakPtr()->SetWebVrMode(true);
  ui_->GetSchedulerUiPtr()->OnWebXrFrameAvailable();

  // All elements should be hidden.
  VerifyOnlyElementsVisible("Elements hidden", std::set<UiElementName>{});
}

TEST_F(UiTest, WebVrTimeout) {
  CreateScene(kInWebVr);

  ui_->GetBrowserUiWeakPtr()->SetWebVrMode(true);
  model_->web_vr.state = kWebVrAwaitingFirstFrame;

  RunForMs(500);
  // On Windows, the timeout message button is not shown.
#if !BUILDFLAG(IS_WIN)
  VerifyVisibility(
      {kWebVrTimeoutSpinner, kWebVrTimeoutMessage, kWebVrTimeoutMessageLayout,
       kWebVrTimeoutMessageIcon, kWebVrTimeoutMessageText,
       kWebVrTimeoutMessageButton, kWebVrTimeoutMessageButtonText},
      false);
#else
  VerifyVisibility(
      {kWebVrTimeoutSpinner, kWebVrTimeoutMessage, kWebVrTimeoutMessageLayout,
       kWebVrTimeoutMessageIcon, kWebVrTimeoutMessageText},
      false);
#endif  // !BUILDFLAG(IS_WIN)
  VerifyVisibility(
      {
          kWebVrBackground,
      },
      true);

  model_->web_vr.state = kWebVrTimeoutImminent;
  RunForMs(500);
  // On Windows, the timeout message button is not shown.
#if !BUILDFLAG(IS_WIN)
  VerifyVisibility({kWebVrTimeoutMessage, kWebVrTimeoutMessageLayout,
                    kWebVrTimeoutMessageIcon, kWebVrTimeoutMessageText,
                    kWebVrTimeoutMessageButton, kWebVrTimeoutMessageButtonText},
                   false);
#else
  VerifyVisibility({kWebVrTimeoutMessage, kWebVrTimeoutMessageLayout,
                    kWebVrTimeoutMessageIcon, kWebVrTimeoutMessageText},
                   false);
#endif  // !BUILDFLAG(IS_WIN)
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
// On Windows, the timeout message button is not shown.
#if !BUILDFLAG(IS_WIN)
  VerifyVisibility(
      {kWebVrBackground, kWebVrTimeoutMessage, kWebVrTimeoutMessageLayout,
       kWebVrTimeoutMessageIcon, kWebVrTimeoutMessageText,
       kWebVrTimeoutMessageButton, kWebVrTimeoutMessageButtonText},
      true);
#else
  VerifyVisibility(
      {kWebVrBackground, kWebVrTimeoutMessage, kWebVrTimeoutMessageLayout,
       kWebVrTimeoutMessageIcon, kWebVrTimeoutMessageText},
      true);
#endif  // !BUILDFLAG(IS_WIN)
}

TEST_F(UiTest, ExitPresentAndFullscreenOnMenuButtonClick) {
  CreateScene(kNotInWebVr);
  ui_->GetBrowserUiWeakPtr()->SetWebVrMode(true);
  // Clicking menu button should trigger to exit presentation.
  EXPECT_CALL(*browser_, ExitPresent());
  InputEventList events;
  events.push_back(
      std::make_unique<InputEvent>(InputEvent::kMenuButtonClicked));
  ui_->HandleMenuButtonEvents(&events);
  base::RunLoop().RunUntilIdle();
}

TEST_F(UiTest, ResetRepositioner) {
  CreateScene(kNotInWebVr);

  Repositioner* repositioner = static_cast<Repositioner*>(
      scene_->GetUiElementByName(k2dBrowsingRepositioner));

  AdvanceFrame();
  gfx::Transform original = repositioner->world_space_transform();

  repositioner->set_laser_direction(kForwardVector);
  repositioner->SetEnabled(true);
  repositioner->set_laser_direction({1, 0, 0});
  AdvanceFrame();

  EXPECT_NE(original, repositioner->world_space_transform());
  repositioner->SetEnabled(false);

  model_->mutable_primary_controller().recentered = true;

  AdvanceFrame();
  EXPECT_TRANSFORM_NEAR(original, repositioner->world_space_transform(), 1e-20);
}

TEST_F(UiTest, RepositionHostedUi) {
  CreateScene(kNotInWebVr);

  Repositioner* repositioner = static_cast<Repositioner*>(
      scene_->GetUiElementByName(k2dBrowsingRepositioner));
  UiElement* hosted_ui = scene_->GetUiElementByName(k2dBrowsingHostedUi);

  model_->hosted_platform_ui.hosted_ui_enabled = true;
  AdvanceFrame();
  gfx::Transform original = hosted_ui->world_space_transform();

  repositioner->set_laser_direction(kForwardVector);
  repositioner->SetEnabled(true);
  repositioner->set_laser_direction({0, 1, 0});
  AdvanceFrame();

  EXPECT_NE(original, hosted_ui->world_space_transform());
  repositioner->SetEnabled(false);
}

// Ensures that permissions do not appear after showing hosted UI.
TEST_F(UiTest, DoNotShowIndicatorsAfterHostedUi) {
#if !BUILDFLAG(IS_WIN)
  CreateScene(kInWebVr);
  auto browser_ui = ui_->GetBrowserUiWeakPtr();
  browser_ui->SetWebVrMode(true);
  EXPECT_FALSE(IsVisible(kWebVrExclusiveScreenToast));
  ui_->GetSchedulerUiPtr()->OnWebXrFrameAvailable();
  browser_ui->SetCapturingState(CapturingStateModel(), CapturingStateModel(),
                                CapturingStateModel());
  AdvanceFrame();
  EXPECT_TRUE(IsVisible(kWebVrExclusiveScreenToast));
  RunForSeconds(8);
  EXPECT_FALSE(IsVisible(kWebVrExclusiveScreenToast));
  model_->web_vr.showing_hosted_ui = true;
  AdvanceFrame();
  model_->web_vr.showing_hosted_ui = false;
  AdvanceFrame();
  EXPECT_FALSE(IsVisible(kWebVrExclusiveScreenToast));
#endif
}

// Ensures that permissions appear on long press, and that when the menu button
// is released that we do not show the exclusive screen toast. Distinguishing
// these cases requires knowledge of the previous state.
TEST_F(UiTest, LongPressMenuButtonInWebVrMode) {
#if !BUILDFLAG(IS_WIN)
  CreateScene(kInWebVr);
  auto browser_ui = ui_->GetBrowserUiWeakPtr();
  browser_ui->SetWebVrMode(true);
  EXPECT_FALSE(IsVisible(kWebVrExclusiveScreenToast));
  ui_->GetSchedulerUiPtr()->OnWebXrFrameAvailable();
  browser_ui->SetCapturingState(CapturingStateModel(), CapturingStateModel(),
                                CapturingStateModel());
  AdvanceFrame();
  EXPECT_TRUE(IsVisible(kWebVrExclusiveScreenToast));
  RunForSeconds(8);
  EXPECT_FALSE(IsVisible(kWebVrExclusiveScreenToast));
  model_->active_capturing.audio_capture_enabled = true;
  EXPECT_FALSE(model_->menu_button_long_pressed);
  InputEventList events;
  events.push_back(
      std::make_unique<InputEvent>(InputEvent::kMenuButtonLongPressStart));
  ui_->HandleMenuButtonEvents(&events);
  AdvanceFrame();
  EXPECT_TRUE(model_->menu_button_long_pressed);
  EXPECT_FALSE(IsVisible(kWebVrExclusiveScreenToast));
  EXPECT_TRUE(IsVisible(kWebVrAudioCaptureIndicator));
  RunForSeconds(8);
  AdvanceFrame();
  EXPECT_TRUE(model_->menu_button_long_pressed);
  EXPECT_FALSE(IsVisible(kWebVrAudioCaptureIndicator));
  EXPECT_FALSE(IsVisible(kWebVrAudioCaptureIndicator));
  EXPECT_FALSE(IsVisible(kWebVrExclusiveScreenToast));
  events.push_back(
      std::make_unique<InputEvent>(InputEvent::kMenuButtonLongPressEnd));
  ui_->HandleMenuButtonEvents(&events);
  EXPECT_FALSE(model_->menu_button_long_pressed);
#endif
}

TEST_F(UiTest, SteadyState) {
  CreateScene(kNotInWebVr);
  RunForSeconds(10.0f);
  // Should have reached steady state.
  EXPECT_FALSE(AdvanceFrame());
}

}  // namespace vr
