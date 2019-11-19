// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene_creator.h"

#include "base/macros.h"
#include "base/numerics/ranges.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
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

constexpr float kTolerance = 1e-5f;
constexpr float kSmallDelaySeconds = 0.1f;
constexpr float kAnimationTimeMs = 300;

MATCHER_P2(SizeFsAreApproximatelyEqual, other, tolerance, "") {
  return base::IsApproximatelyEqual(arg.width(), other.width(), tolerance) &&
         base::IsApproximatelyEqual(arg.height(), other.height(), tolerance);
}

void VerifyButtonColor(DiscButton* button,
                       SkColor foreground_color,
                       SkColor background_color,
                       const std::string& trace_name) {
  SCOPED_TRACE(trace_name);
  EXPECT_EQ(button->foreground()->GetColor(), foreground_color);
  EXPECT_EQ(button->background()->edge_color(), background_color);
  EXPECT_EQ(button->background()->center_color(), background_color);
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
  EXPECT_TRUE(
      RunForSeconds(kWmrInitialIndicatorsTimeoutSeconds + kSmallDelaySeconds));
  EXPECT_FALSE(IsVisible(kWebVrExclusiveScreenToast));

  browser_ui->SetWebVrMode(false);
  EXPECT_FALSE(IsVisible(kWebVrExclusiveScreenToast));
}

TEST_F(UiTest, PlatformToast) {
  CreateScene(kNotInWebVr);
  EXPECT_FALSE(IsVisible(kPlatformToast));
  auto browser_ui = ui_->GetBrowserUiWeakPtr();

  // show and hide toast after a timeout.
  browser_ui->ShowPlatformToast(base::UTF8ToUTF16("Downloading"));
  EXPECT_TRUE(IsVisible(kPlatformToast));
  EXPECT_TRUE(RunForSeconds(kToastTimeoutSeconds + kSmallDelaySeconds));
  EXPECT_FALSE(IsVisible(kPlatformToast));

  // toast can be cancelled.
  browser_ui->ShowPlatformToast(base::UTF8ToUTF16("Downloading"));
  EXPECT_TRUE(IsVisible(kPlatformToast));
  browser_ui->CancelPlatformToast();
  EXPECT_FALSE(IsVisible(kPlatformToast));

  // toast can refresh visible timeout.
  browser_ui->ShowPlatformToast(base::UTF8ToUTF16("Downloading"));
  EXPECT_TRUE(RunForSeconds(kSmallDelaySeconds));
  browser_ui->ShowPlatformToast(base::UTF8ToUTF16("Downloading"));
  EXPECT_TRUE(RunForSeconds(kToastTimeoutSeconds - kSmallDelaySeconds));
  EXPECT_TRUE(IsVisible(kPlatformToast));
  EXPECT_TRUE(RunForSeconds(kToastTimeoutSeconds + kSmallDelaySeconds));
  EXPECT_FALSE(IsVisible(kPlatformToast));
}

TEST_F(UiTest, CaptureToasts) {
  CreateScene(kNotInWebVr);
  auto browser_ui = ui_->GetBrowserUiWeakPtr();

  for (auto& spec : GetIndicatorSpecs()) {
    for (int i = 0; i < 3; ++i) {
#if !defined(OS_ANDROID)
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
      EXPECT_TRUE(RunForSeconds(kWmrInitialIndicatorsTimeoutSeconds +
                                kSmallDelaySeconds));
      EXPECT_FALSE(IsVisible(kWebVrExclusiveScreenToast));

      browser_ui->SetWebVrMode(false);
      EXPECT_FALSE(IsVisible(kWebVrExclusiveScreenToast));
      EXPECT_FALSE(IsVisible(spec.webvr_name));
    }
  }
}

TEST_F(UiTest, CloseButtonVisibleInFullscreen) {
  // Button should not be visible when not in fullscreen.
  CreateScene(kNotInWebVr);
  EXPECT_FALSE(IsVisible(kCloseButton));

  auto browser_ui = ui_->GetBrowserUiWeakPtr();
  // Button should be visible in fullscreen and hidden when leaving fullscreen.
  browser_ui->SetFullscreen(true);
  EXPECT_TRUE(IsVisible(kCloseButton));
  browser_ui->SetFullscreen(false);
  EXPECT_FALSE(IsVisible(kCloseButton));

  // Button should not be visible when in WebVR.
  CreateScene(kInWebVr);
  browser_ui = ui_->GetBrowserUiWeakPtr();
  EXPECT_FALSE(IsVisible(kCloseButton));
  browser_ui->SetWebVrMode(false);
  EXPECT_FALSE(IsVisible(kCloseButton));
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

TEST_F(UiTest, VoiceSearchHiddenInIncognito) {
  CreateScene(kNotInWebVr);

  model_->push_mode(kModeEditingOmnibox);
  EXPECT_TRUE(AdvanceFrame());
  EXPECT_TRUE(IsVisible(kOmniboxVoiceSearchButton));

  model_->incognito = true;
  EXPECT_TRUE(AdvanceFrame());
  EXPECT_FALSE(IsVisible(kOmniboxVoiceSearchButton));
}

TEST_F(UiTest, VoiceSearchHiddenWhenCantAskForPermission) {
  CreateScene(kNotInWebVr);

  model_->push_mode(kModeEditingOmnibox);
  model_->speech.has_or_can_request_record_audio_permission = true;
  EXPECT_TRUE(AdvanceFrame());
  EXPECT_TRUE(IsVisible(kOmniboxVoiceSearchButton));

  model_->speech.has_or_can_request_record_audio_permission = false;
  EXPECT_TRUE(AdvanceFrame());
  EXPECT_FALSE(IsVisible(kOmniboxVoiceSearchButton));
}

TEST_F(UiTest, VoiceSearchHiddenWhenContentCapturingAudio) {
  CreateScene(kNotInWebVr);

  model_->push_mode(kModeEditingOmnibox);
  model_->speech.has_or_can_request_record_audio_permission = true;
  model_->active_capturing.audio_capture_enabled = false;
  EXPECT_TRUE(AdvanceFrame());
  EXPECT_TRUE(IsVisible(kOmniboxVoiceSearchButton));

  model_->active_capturing.audio_capture_enabled = true;
  EXPECT_TRUE(AdvanceFrame());
  EXPECT_FALSE(IsVisible(kOmniboxVoiceSearchButton));
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

TEST_F(UiTest, UiModeOmniboxEditing) {
  CreateScene(kNotInWebVr);

  EXPECT_EQ(model_->ui_modes.size(), 1u);
  EXPECT_EQ(model_->ui_modes.back(), kModeBrowsing);
  EXPECT_EQ(NumVisibleInTree(kOmniboxRoot), 0);
  VerifyOnlyElementsVisible("Initial", kElementsVisibleInBrowsing);

  model_->push_mode(kModeEditingOmnibox);
  AdvanceFrame();
  EXPECT_EQ(model_->ui_modes.size(), 2u);
  EXPECT_EQ(model_->ui_modes[1], kModeEditingOmnibox);
  EXPECT_EQ(model_->ui_modes[0], kModeBrowsing);
  EXPECT_GT(NumVisibleInTree(kOmniboxRoot), 0);

  model_->pop_mode(kModeEditingOmnibox);
  AdvanceFrame();
  EXPECT_EQ(model_->ui_modes.size(), 1u);
  EXPECT_EQ(model_->ui_modes.back(), kModeBrowsing);
  VerifyOnlyElementsVisible("Browsing", kElementsVisibleInBrowsing);
}

TEST_F(UiTest, UiModeVoiceSearchFromOmnibox) {
  CreateScene(kNotInWebVr);
  auto browser_ui = ui_->GetBrowserUiWeakPtr();

  EXPECT_EQ(model_->ui_modes.size(), 1u);
  EXPECT_EQ(model_->ui_modes.back(), kModeBrowsing);
  EXPECT_TRUE(IsVisible(kContentQuad));
  EXPECT_FALSE(IsVisible(kOmniboxBackground));

  model_->push_mode(kModeEditingOmnibox);
  EXPECT_FALSE(IsVisible(kContentQuad));
  EXPECT_TRUE(IsVisible(kOmniboxBackground));
  EXPECT_EQ(model_->ui_modes.size(), 2u);
  EXPECT_EQ(model_->ui_modes[1], kModeEditingOmnibox);
  EXPECT_EQ(model_->ui_modes[0], kModeBrowsing);

  browser_ui->SetSpeechRecognitionEnabled(true);
  EXPECT_FALSE(IsVisible(kOmniboxBackground));
  EXPECT_EQ(model_->ui_modes.size(), 4u);
  EXPECT_EQ(model_->ui_modes[2], kModeVoiceSearch);
  EXPECT_EQ(model_->ui_modes[1], kModeEditingOmnibox);
  EXPECT_EQ(model_->ui_modes[0], kModeBrowsing);
  VerifyVisibility(kElementsVisibleWithVoiceSearch, true);

  browser_ui->SetSpeechRecognitionEnabled(false);
  EXPECT_TRUE(IsVisible(kOmniboxBackground));
  EXPECT_EQ(model_->ui_modes.size(), 2u);
  EXPECT_EQ(model_->ui_modes[1], kModeEditingOmnibox);
  EXPECT_EQ(model_->ui_modes[0], kModeBrowsing);

  model_->pop_mode(kModeEditingOmnibox);
  EXPECT_EQ(model_->ui_modes.size(), 1u);
  EXPECT_EQ(model_->ui_modes.back(), kModeBrowsing);
  AdvanceFrame();
  EXPECT_FALSE(IsVisible(kOmniboxBackground));
  EXPECT_TRUE(IsVisible(kContentQuad));
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

TEST_F(UiTest, UiUpdatesForFullscreenChanges) {
  auto visible_in_fullscreen = kFloorCeilingBackgroundElements;
  visible_in_fullscreen.insert(kContentQuad);
  visible_in_fullscreen.insert(kBackplane);
  visible_in_fullscreen.insert(kCloseButton);
  visible_in_fullscreen.insert(kController);
  visible_in_fullscreen.insert(kControllerTouchpadButton);
  visible_in_fullscreen.insert(kControllerAppButton);
  visible_in_fullscreen.insert(kControllerHomeButton);
  visible_in_fullscreen.insert(kControllerBatteryDot0);
  visible_in_fullscreen.insert(kControllerBatteryDot1);
  visible_in_fullscreen.insert(kControllerBatteryDot2);
  visible_in_fullscreen.insert(kControllerBatteryDot3);
  visible_in_fullscreen.insert(kControllerBatteryDot4);
  visible_in_fullscreen.insert(kLaser);
  visible_in_fullscreen.insert(kContentFrame);
  visible_in_fullscreen.insert(kContentFrameHitPlane);

  CreateScene(kNotInWebVr);

  auto browser_ui = ui_->GetBrowserUiWeakPtr();

  // Hold onto the background color to make sure it changes.
  SkColor initial_background = SK_ColorBLACK;
  GetBackgroundColor(&initial_background);
  VerifyOnlyElementsVisible("Initial", kElementsVisibleInBrowsing);
  UiElement* content_quad = scene_->GetUiElementByName(kContentQuad);
  UiElement* content_group =
      scene_->GetUiElementByName(k2dBrowsingContentGroup);
  gfx::SizeF initial_content_size = content_quad->size();
  gfx::Transform initial_position = content_group->LocalTransform();

  // In fullscreen mode, content elements should be visible, control elements
  // should be hidden.
  browser_ui->SetFullscreen(true);
  VerifyOnlyElementsVisible("In fullscreen", visible_in_fullscreen);
  // Make sure background has changed for fullscreen.
  SkColor fullscreen_background = SK_ColorBLACK;
  GetBackgroundColor(&fullscreen_background);
  EXPECT_EQ(ColorScheme::GetColorScheme(ColorScheme::kModeFullscreen)
                .world_background,
            fullscreen_background);
  // Should have started transition.
  EXPECT_TRUE(IsAnimating(content_quad, {BOUNDS}));
  EXPECT_TRUE(IsAnimating(content_group, {TRANSFORM}));
  // Finish the transition.
  EXPECT_TRUE(RunForMs(1000));
  EXPECT_FALSE(IsAnimating(content_quad, {BOUNDS}));
  EXPECT_FALSE(IsAnimating(content_group, {TRANSFORM}));
  EXPECT_NE(initial_content_size, content_quad->size());
  EXPECT_NE(initial_position, content_group->LocalTransform());

  // Everything should return to original state after leaving fullscreen.
  browser_ui->SetFullscreen(false);
  VerifyOnlyElementsVisible("Restore initial", kElementsVisibleInBrowsing);
  SkColor no_longer_fullscreen_background = SK_ColorBLACK;
  GetBackgroundColor(&no_longer_fullscreen_background);
  EXPECT_EQ(
      ColorScheme::GetColorScheme(ColorScheme::kModeNormal).world_background,
      no_longer_fullscreen_background);
  // Should have started transition.
  EXPECT_TRUE(IsAnimating(content_quad, {BOUNDS}));
  EXPECT_TRUE(IsAnimating(content_group, {TRANSFORM}));
  // Finish the transition.
  EXPECT_TRUE(RunForMs(1000));
  EXPECT_FALSE(IsAnimating(content_quad, {BOUNDS}));
  EXPECT_FALSE(IsAnimating(content_group, {TRANSFORM}));
  EXPECT_EQ(initial_content_size, content_quad->size());
  EXPECT_EQ(initial_position, content_group->LocalTransform());
}

TEST_F(UiTest, SecurityIconClickShouldShowPageInfo) {
  CreateScene(kNotInWebVr);

  // Initial state.
  VerifyOnlyElementsVisible("Initial", kElementsVisibleInBrowsing);

  EXPECT_CALL(*browser_, ShowPageInfo);
  auto* security_icon = scene_->GetUiElementByName(kUrlBarSecurityButton);
  ClickElement(security_icon);
}

TEST_F(UiTest, ClickingOmniboxTriggersUnsupportedMode) {
  UiInitialState state;
  state.needs_keyboard_update = true;
  CreateScene(state);
  VerifyOnlyElementsVisible("Initial", kElementsVisibleInBrowsing);

  // Clicking the omnibox should show the update prompt.
  auto* omnibox = scene_->GetUiElementByName(kUrlBarOriginRegion);
  EXPECT_CALL(*browser_,
              OnUnsupportedMode(UiUnsupportedMode::kNeedsKeyboardUpdate));
  ClickElement(omnibox);
  ui_->GetBrowserUiWeakPtr()->ShowExitVrPrompt(
      UiUnsupportedMode::kNeedsKeyboardUpdate);
  AdvanceFrame();
  EXPECT_EQ(model_->active_modal_prompt_type,
            ModalPromptType::kModalPromptTypeUpdateKeyboard);
  EXPECT_TRUE(scene_->GetUiElementByName(kExitPrompt)->IsVisible());
}

TEST_F(UiTest, WebInputEditingTriggersUnsupportedMode) {
  UiInitialState state;
  state.needs_keyboard_update = true;
  CreateScene(state);
  VerifyOnlyElementsVisible("Initial", kElementsVisibleInBrowsing);
  auto browser_ui = ui_->GetBrowserUiWeakPtr();

  // A call to show the keyboard should show the update prompt.
  EXPECT_CALL(*browser_,
              OnUnsupportedMode(UiUnsupportedMode::kNeedsKeyboardUpdate));
  browser_ui->ShowSoftInput(true);
  browser_ui->ShowExitVrPrompt(UiUnsupportedMode::kNeedsKeyboardUpdate);
  AdvanceFrame();
  EXPECT_EQ(model_->active_modal_prompt_type,
            ModalPromptType::kModalPromptTypeUpdateKeyboard);
  EXPECT_TRUE(scene_->GetUiElementByName(kExitPrompt)->IsVisible());
}

TEST_F(UiTest, ExitWebInputEditingOnMenuButtonClick) {
  CreateScene(kNotInWebVr);
  EXPECT_FALSE(scene_->GetUiElementByName(kKeyboard)->IsVisible());
  ui_->GetBrowserUiWeakPtr()->ShowSoftInput(true);
  AdvanceFrame();
  EXPECT_TRUE(scene_->GetUiElementByName(kKeyboard)->IsVisible());
  InputEventList events;
  events.push_back(
      std::make_unique<InputEvent>(InputEvent::kMenuButtonClicked));
  ui_->HandleMenuButtonEvents(&events);
  base::RunLoop().RunUntilIdle();
  AdvanceFrame();
  // Clicking menu button should hide the keyboard.
  EXPECT_FALSE(scene_->GetUiElementByName(kKeyboard)->IsVisible());
}

TEST_F(UiTest, ShowAndHideExitPrompt) {
  CreateScene(kNotInWebVr);

  model_->active_modal_prompt_type = kModalPromptTypeExitVRForSiteInfo;
  model_->push_mode(kModeModalPrompt);
  EXPECT_TRUE(IsVisible(kExitPrompt));
  EXPECT_TRUE(IsVisible(kContentQuad));

  model_->active_modal_prompt_type = kModalPromptTypeNone;
  model_->pop_mode(kModeModalPrompt);
  EXPECT_FALSE(IsVisible(kExitPrompt));
  EXPECT_TRUE(IsVisible(kContentQuad));
}

TEST_F(UiTest, PrimaryButtonClickTriggersOnExitPrompt) {
  CreateScene(kNotInWebVr);

  // Initial state.
  VerifyOnlyElementsVisible("Initial", kElementsVisibleInBrowsing);
  ui_->GetBrowserUiWeakPtr()->ShowExitVrPrompt(
      UiUnsupportedMode::kUnhandledPageInfo);
  AdvanceFrame();

  // Click on 'EXIT VR' should trigger UI browser interface and close prompt.
  EXPECT_CALL(*browser_,
              OnExitVrPromptResult(ExitVrPromptChoice::CHOICE_EXIT,
                                   UiUnsupportedMode::kUnhandledPageInfo));
  auto* prompt = scene_->GetUiElementByName(kExitPrompt);
  auto* button = prompt->GetDescendantByType(kTypePromptPrimaryButton);
  ClickElement(button);
  VerifyOnlyElementsVisible("Prompt cleared", kElementsVisibleInBrowsing);
}

TEST_F(UiTest, SecondaryButtonClickTriggersOnExitPrompt) {
  CreateScene(kNotInWebVr);

  // Initial state.
  VerifyOnlyElementsVisible("Initial", kElementsVisibleInBrowsing);
  model_->active_modal_prompt_type = kModalPromptTypeExitVRForSiteInfo;
  AdvanceFrame();

  // Click on 'BACK' should trigger UI browser interface and close prompt.
  EXPECT_CALL(*browser_,
              OnExitVrPromptResult(ExitVrPromptChoice::CHOICE_STAY,
                                   UiUnsupportedMode::kUnhandledPageInfo));

  auto* prompt = scene_->GetUiElementByName(kExitPrompt);
  auto* button = prompt->GetDescendantByType(kTypePromptSecondaryButton);
  ClickElement(button);
  VerifyOnlyElementsVisible("Prompt cleared", kElementsVisibleInBrowsing);
}

TEST_F(UiTest, ClickOnPromptBackgroundDoesNothing) {
  CreateScene(kNotInWebVr);

  ui_->GetBrowserUiWeakPtr()->ShowExitVrPrompt(
      UiUnsupportedMode::kUnhandledPageInfo);
  AdvanceFrame();

  EXPECT_CALL(*browser_, OnExitVrPromptResult(testing::_, testing::_)).Times(0);
  auto* prompt = scene_->GetUiElementByName(kExitPrompt);
  UiElement* target;
  // Target the inert icon and text to reliably miss the buttons.
  target = prompt->GetDescendantByType(kTypePromptIcon);
  ClickElement(target);
  target = prompt->GetDescendantByType(kTypePromptText);
  ClickElement(target);

  EXPECT_EQ(model_->get_mode(), kModeModalPrompt);
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

TEST_F(UiTest, CaptureIndicatorsVisibility) {
  const std::set<UiElementName> indicators = {
      kAudioCaptureIndicator,       kVideoCaptureIndicator,
      kScreenCaptureIndicator,      kLocationAccessIndicator,
      kBluetoothConnectedIndicator,
  };

  CreateScene(kNotInWebVr);
  auto browser_ui = ui_->GetBrowserUiWeakPtr();

  EXPECT_TRUE(VerifyVisibility(indicators, false));
  EXPECT_TRUE(VerifyRequiresLayout(indicators, false));

  model_->active_capturing.audio_capture_enabled = true;
  model_->active_capturing.video_capture_enabled = true;
  model_->active_capturing.screen_capture_enabled = true;
  model_->active_capturing.location_access_enabled = true;
  model_->active_capturing.bluetooth_connected = true;
  model_->active_capturing.usb_connected = true;
  model_->active_capturing.midi_connected = true;
  EXPECT_TRUE(VerifyVisibility(indicators, true));
  EXPECT_TRUE(VerifyRequiresLayout(indicators, true));

  // Go into non-browser modes and make sure all indicators are hidden.
  browser_ui->SetWebVrMode(true);
  EXPECT_TRUE(VerifyVisibility(indicators, false));
  browser_ui->SetWebVrMode(false);
  browser_ui->SetFullscreen(true);
  EXPECT_TRUE(VerifyVisibility(indicators, false));
  browser_ui->SetFullscreen(false);

  // Back to browser, make sure the indicators reappear.
  EXPECT_TRUE(VerifyVisibility(indicators, true));
  EXPECT_TRUE(VerifyRequiresLayout(indicators, true));

  // Ensure they can be turned off.
  model_->active_capturing.audio_capture_enabled = false;
  model_->active_capturing.video_capture_enabled = false;
  model_->active_capturing.screen_capture_enabled = false;
  model_->active_capturing.location_access_enabled = false;
  model_->active_capturing.bluetooth_connected = false;
  model_->active_capturing.usb_connected = false;
  model_->active_capturing.midi_connected = false;
  EXPECT_TRUE(VerifyRequiresLayout(indicators, false));
}

TEST_F(UiTest, PropagateContentBoundsOnStart) {
  CreateScene(kNotInWebVr);

  gfx::SizeF expected_bounds(0.495922f, 0.330614f);
  EXPECT_CALL(*browser_,
              OnContentScreenBoundsChanged(
                  SizeFsAreApproximatelyEqual(expected_bounds, kTolerance)));

  ui_->OnProjMatrixChanged(GetPixelDaydreamProjMatrix());
  AdvanceFrame();
}

TEST_F(UiTest, PropagateContentBoundsOnFullscreen) {
  CreateScene(kNotInWebVr);

  ui_->OnProjMatrixChanged(GetPixelDaydreamProjMatrix());
  ui_->GetBrowserUiWeakPtr()->SetFullscreen(true);

  gfx::SizeF expected_bounds(0.587874f, 0.330614f);
  EXPECT_CALL(*browser_,
              OnContentScreenBoundsChanged(
                  SizeFsAreApproximatelyEqual(expected_bounds, kTolerance)));

  ui_->OnProjMatrixChanged(GetPixelDaydreamProjMatrix());
  AdvanceFrame();
}

TEST_F(UiTest, DontPropagateContentBoundsOnNegligibleChange) {
  CreateScene(kNotInWebVr);

  EXPECT_FALSE(RunForMs(0));
  ui_->OnProjMatrixChanged(GetPixelDaydreamProjMatrix());

  UiElement* content_quad = scene_->GetUiElementByName(kContentQuad);
  gfx::SizeF content_quad_size = content_quad->size();
  content_quad_size.Scale(1.2f);
  content_quad->SetSize(content_quad_size.width(), content_quad_size.height());
  AdvanceFrame();

  EXPECT_CALL(*browser_, OnContentScreenBoundsChanged(testing::_)).Times(0);

  ui_->OnProjMatrixChanged(GetPixelDaydreamProjMatrix());
}

TEST_F(UiTest, LoadingIndicatorBindings) {
  CreateScene(kNotInWebVr);
  UiElement* background = scene_->GetUiElementByName(kLoadingIndicator);
  UiElement* foreground =
      scene_->GetUiElementByName(kLoadingIndicatorForeground);

  model_->loading = true;
  model_->load_progress = 0;
  RunForMs(100);
  EXPECT_TRUE(
      VerifyVisibility({kLoadingIndicator, kLoadingIndicatorForeground}, true));
  EXPECT_EQ(foreground->GetClipRect(), gfx::RectF(0, 0, 0, 1));
  EXPECT_EQ(foreground->size(), background->size());

  model_->load_progress = 0.5f;
  EXPECT_TRUE(
      VerifyVisibility({kLoadingIndicator, kLoadingIndicatorForeground}, true));
  EXPECT_EQ(foreground->GetClipRect(), gfx::RectF(0, 0, 0.5, 1));
  EXPECT_EQ(foreground->size(), background->size());

  model_->load_progress = 1.0f;
  EXPECT_TRUE(
      VerifyVisibility({kLoadingIndicator, kLoadingIndicatorForeground}, true));
  EXPECT_EQ(foreground->GetClipRect(), gfx::RectF(0, 0, 1, 1));
  EXPECT_EQ(foreground->size(), background->size());

  model_->loading = false;
  EXPECT_TRUE(VerifyVisibility({kLoadingIndicator, kLoadingIndicatorForeground},
                               false));
}

TEST_F(UiTest, WebVrTimeout) {
  CreateScene(kInWebVr);

  ui_->GetBrowserUiWeakPtr()->SetWebVrMode(true);
  model_->web_vr.state = kWebVrAwaitingFirstFrame;

  RunForMs(500);
  // On Windows, the timeout message button is not shown.
#if !defined(OS_WIN)
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
#endif  // OS_WIN
  VerifyVisibility(
      {
          kWebVrBackground,
      },
      true);

  model_->web_vr.state = kWebVrTimeoutImminent;
  RunForMs(500);
  // On Windows, the timeout message button is not shown.
#if !defined(OS_WIN)
  VerifyVisibility({kWebVrTimeoutMessage, kWebVrTimeoutMessageLayout,
                    kWebVrTimeoutMessageIcon, kWebVrTimeoutMessageText,
                    kWebVrTimeoutMessageButton, kWebVrTimeoutMessageButtonText},
                   false);
#else
  VerifyVisibility({kWebVrTimeoutMessage, kWebVrTimeoutMessageLayout,
                    kWebVrTimeoutMessageIcon, kWebVrTimeoutMessageText},
                   false);
#endif  // OS_WIN
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
#if !defined(OS_WIN)
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
#endif  // OS_WIN
}

TEST_F(UiTest, SpeechRecognitionUiVisibility) {
  CreateScene(kNotInWebVr);
  auto browser_ui = ui_->GetBrowserUiWeakPtr();

  browser_ui->SetSpeechRecognitionEnabled(true);

  // Start hiding browsing foreground and showing speech recognition listening
  // UI.
  EXPECT_TRUE(RunForMs(10));
  VerifyIsAnimating({k2dBrowsingForeground}, {OPACITY}, true);
  VerifyIsAnimating({kSpeechRecognitionListening, kSpeechRecognitionResult},
                    {OPACITY}, false);
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionListening},
                    {OPACITY}, true);

  EXPECT_TRUE(RunForMs(kSpeechRecognitionOpacityAnimationDurationMs));
  // All opacity animations should be finished at this point.
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionListening,
                     kSpeechRecognitionResult},
                    {OPACITY}, false);
  VerifyIsAnimating({kSpeechRecognitionListeningGrowingCircle}, {CIRCLE_GROW},
                    false);
  VerifyVisibility(kElementsVisibleWithVoiceSearch, true);
  VerifyVisibility({k2dBrowsingForeground, kSpeechRecognitionResult}, false);

  model_->speech.speech_recognition_state = SPEECH_RECOGNITION_READY;
  EXPECT_TRUE(RunForMs(10));
  VerifyIsAnimating({kSpeechRecognitionListeningGrowingCircle}, {CIRCLE_GROW},
                    true);

  // Mock received speech result.
  model_->speech.speech_recognition_state = SPEECH_RECOGNITION_END;
  browser_ui->SetRecognitionResult(base::ASCIIToUTF16("test"));
  browser_ui->SetSpeechRecognitionEnabled(false);

  EXPECT_TRUE(RunForMs(10));
  VerifyVisibility(kElementsVisibleWithVoiceSearchResult, true);
  // Speech result UI should show instantly while listening UI hide immediately.
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionListening,
                     kSpeechRecognitionResult},
                    {OPACITY}, false);
  VerifyIsAnimating({kSpeechRecognitionListeningGrowingCircle}, {CIRCLE_GROW},
                    false);
  VerifyVisibility({k2dBrowsingForeground, kSpeechRecognitionListening}, false);

  // The visibility of Speech Recognition UI should not change at this point,
  // but it will be animating.
  EXPECT_FALSE(RunForMs(10));

  EXPECT_TRUE(RunForMs(kSpeechRecognitionResultTimeoutMs));
  // Start hide speech recognition result and show browsing foreground.
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionResult},
                    {OPACITY}, true);
  VerifyIsAnimating({kSpeechRecognitionListening}, {OPACITY}, false);

  EXPECT_TRUE(RunForMs(kSpeechRecognitionOpacityAnimationDurationMs));
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionListening,
                     kSpeechRecognitionResult},
                    {OPACITY}, false);

  // Visibility is as expected.
  VerifyVisibility({kSpeechRecognitionListening, kSpeechRecognitionResult},
                   false);

  EXPECT_TRUE(IsVisible(k2dBrowsingForeground));
}

TEST_F(UiTest, SpeechRecognitionUiVisibilityNoResult) {
  CreateScene(kNotInWebVr);
  auto browser_ui = ui_->GetBrowserUiWeakPtr();

  browser_ui->SetSpeechRecognitionEnabled(true);
  EXPECT_TRUE(RunForMs(kSpeechRecognitionOpacityAnimationDurationMs));
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionListening},
                    {OPACITY}, false);

  // Mock exit without a recognition result
  browser_ui->SetSpeechRecognitionEnabled(false);
  model_->speech.recognition_result.clear();
  model_->speech.speech_recognition_state = SPEECH_RECOGNITION_END;

  EXPECT_TRUE(RunForMs(10));
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionListening},
                    {OPACITY}, true);
  VerifyIsAnimating({kSpeechRecognitionResult}, {OPACITY}, false);

  EXPECT_TRUE(RunForMs(kSpeechRecognitionOpacityAnimationDurationMs));
  VerifyIsAnimating({k2dBrowsingForeground, kSpeechRecognitionListening,
                     kSpeechRecognitionResult},
                    {OPACITY}, false);

  // Visibility is as expected.
  VerifyVisibility({kSpeechRecognitionListening, kSpeechRecognitionResult},
                   false);
  EXPECT_TRUE(IsVisible(k2dBrowsingForeground));
}

TEST_F(UiTest, OmniboxSuggestionBindings) {
  CreateScene(kNotInWebVr);
  UiElement* container = scene_->GetUiElementByName(kOmniboxSuggestions);
  ASSERT_NE(container, nullptr);

  AdvanceFrame();
  EXPECT_EQ(container->children().size(), 0u);

  model_->push_mode(kModeEditingOmnibox);
  EXPECT_EQ(container->children().size(), 0u);
  EXPECT_EQ(NumVisibleInTree(kOmniboxSuggestions), 0);

  model_->omnibox_suggestions.emplace_back(
      OmniboxSuggestion(base::string16(), base::string16(),
                        ACMatchClassifications(), ACMatchClassifications(),
                        nullptr, GURL(), base::string16(), base::string16()));
  AdvanceFrame();
  EXPECT_EQ(container->children().size(), 1u);
  EXPECT_GT(NumVisibleInTree(kOmniboxSuggestions), 1);

  model_->omnibox_suggestions.clear();
  AdvanceFrame();
  EXPECT_EQ(container->children().size(), 0u);
  EXPECT_EQ(NumVisibleInTree(kOmniboxSuggestions), 0);
}

TEST_F(UiTest, OmniboxSuggestionNavigates) {
  CreateScene(kNotInWebVr);
  GURL gurl("http://test.com/");
  model_->push_mode(kModeEditingOmnibox);
  model_->omnibox_suggestions.emplace_back(
      OmniboxSuggestion(base::string16(), base::string16(),
                        ACMatchClassifications(), ACMatchClassifications(),
                        nullptr, gurl, base::string16(), base::string16()));
  AdvanceFrame();

  // Let the omnibox fade in.
  RunForMs(200);

  UiElement* suggestions = scene_->GetUiElementByName(kOmniboxSuggestions);
  ASSERT_NE(suggestions, nullptr);
  UiElement* suggestion = suggestions->children().front().get();
  ASSERT_NE(suggestion, nullptr);
  EXPECT_CALL(*browser_,
              Navigate(gurl, NavigationMethod::kOmniboxSuggestionSelected))
      .Times(1);
  ClickElement(suggestion);
}

TEST_F(UiTest, CloseButtonColorBindings) {
  CreateScene(kNotInWebVr);
  auto browser_ui = ui_->GetBrowserUiWeakPtr();
  browser_ui->SetFullscreen(true);
  EXPECT_TRUE(IsVisible(kCloseButton));
  DiscButton* button =
      static_cast<DiscButton*>(scene_->GetUiElementByName(kCloseButton));
  for (int i = 0; i < ColorScheme::kNumModes; i++) {
    ColorScheme::Mode mode = static_cast<ColorScheme::Mode>(i);
    SCOPED_TRACE(mode);
    if (mode == ColorScheme::kModeIncognito) {
      browser_ui->SetIncognito(true);
    } else if (mode == ColorScheme::kModeFullscreen) {
      browser_ui->SetIncognito(false);
      browser_ui->SetFullscreen(true);
    }
    ColorScheme scheme = model_->color_scheme();
    RunForMs(kAnimationTimeMs);
    VerifyButtonColor(button, scheme.disc_button_colors.foreground,
                      scheme.disc_button_colors.background, "normal");
    button->hit_plane()->OnHoverEnter(gfx::PointF(0.5f, 0.5f),
                                      base::TimeTicks());
    RunForMs(kAnimationTimeMs);
    VerifyButtonColor(button, scheme.disc_button_colors.foreground,
                      scheme.disc_button_colors.background_hover, "hover");
    button->hit_plane()->OnButtonDown(gfx::PointF(0.5f, 0.5f),
                                      base::TimeTicks());
    RunForMs(kAnimationTimeMs);
    VerifyButtonColor(button, scheme.disc_button_colors.foreground,
                      scheme.disc_button_colors.background_down, "down");
    button->hit_plane()->OnHoverMove(gfx::PointF(), base::TimeTicks());
    RunForMs(kAnimationTimeMs);
    VerifyButtonColor(button, scheme.disc_button_colors.foreground,
                      scheme.disc_button_colors.background, "move");
    button->hit_plane()->OnButtonUp(gfx::PointF(), base::TimeTicks());
    RunForMs(kAnimationTimeMs);
    VerifyButtonColor(button, scheme.disc_button_colors.foreground,
                      scheme.disc_button_colors.background, "up");
  }
}

TEST_F(UiTest, ExitPresentAndFullscreenOnMenuButtonClick) {
  CreateScene(kNotInWebVr);
  ui_->GetBrowserUiWeakPtr()->SetWebVrMode(true);
  // Clicking menu button should trigger to exit presentation.
  EXPECT_CALL(*browser_, ExitPresent());
  // And also trigger exit fullscreen.
  EXPECT_CALL(*browser_, ExitFullscreen());
  InputEventList events;
  events.push_back(
      std::make_unique<InputEvent>(InputEvent::kMenuButtonClicked));
  ui_->HandleMenuButtonEvents(&events);
  base::RunLoop().RunUntilIdle();
}


TEST_F(UiTest, DefaultBackgroundWhenNoAssetAvailable) {
  UiInitialState state;
  state.assets_supported = false;
  CreateScene(state);

  EXPECT_FALSE(IsVisible(k2dBrowsingTexturedBackground));
  EXPECT_TRUE(IsVisible(k2dBrowsingDefaultBackground));
  EXPECT_TRUE(IsVisible(kContentQuad));
}

TEST_F(UiTest, TextureBackgroundAfterAssetLoaded) {
  UiInitialState state;
  state.assets_supported = true;
  CreateScene(state);

  EXPECT_FALSE(IsVisible(k2dBrowsingTexturedBackground));
  EXPECT_FALSE(IsVisible(k2dBrowsingDefaultBackground));
  EXPECT_FALSE(IsVisible(kContentQuad));

  auto assets = std::make_unique<Assets>();
  ui_->GetBrowserUiWeakPtr()->OnAssetsLoaded(
      AssetsLoadStatus::kSuccess, std::move(assets), base::Version("1.0"));

  EXPECT_TRUE(IsVisible(k2dBrowsingTexturedBackground));
  EXPECT_TRUE(IsVisible(kContentQuad));
  EXPECT_FALSE(IsVisible(k2dBrowsingDefaultBackground));
}

TEST_F(UiTest, ControllerLabels) {
  CreateScene(kNotInWebVr);

  EXPECT_FALSE(IsVisible(kControllerTrackpadLabel));
  EXPECT_FALSE(IsVisible(kControllerTrackpadRepositionLabel));
  EXPECT_FALSE(IsVisible(kControllerExitButtonLabel));
  EXPECT_FALSE(IsVisible(kControllerBackButtonLabel));

  model_->mutable_primary_controller().resting_in_viewport = true;
  EXPECT_TRUE(IsVisible(kControllerTrackpadLabel));
  EXPECT_FALSE(IsVisible(kControllerTrackpadRepositionLabel));
  EXPECT_FALSE(IsVisible(kControllerExitButtonLabel));
  EXPECT_FALSE(IsVisible(kControllerBackButtonLabel));

  model_->push_mode(kModeFullscreen);
  EXPECT_TRUE(IsVisible(kControllerTrackpadLabel));
  EXPECT_FALSE(IsVisible(kControllerTrackpadRepositionLabel));
  EXPECT_TRUE(IsVisible(kControllerExitButtonLabel));
  EXPECT_FALSE(IsVisible(kControllerBackButtonLabel));

  model_->pop_mode(kModeFullscreen);
  EXPECT_TRUE(IsVisible(kControllerTrackpadLabel));
  EXPECT_FALSE(IsVisible(kControllerTrackpadRepositionLabel));
  EXPECT_FALSE(IsVisible(kControllerExitButtonLabel));
  EXPECT_FALSE(IsVisible(kControllerBackButtonLabel));

  model_->push_mode(kModeEditingOmnibox);
  EXPECT_TRUE(IsVisible(kControllerTrackpadLabel));
  EXPECT_FALSE(IsVisible(kControllerTrackpadRepositionLabel));
  EXPECT_FALSE(IsVisible(kControllerExitButtonLabel));
  EXPECT_TRUE(IsVisible(kControllerBackButtonLabel));

  model_->pop_mode(kModeEditingOmnibox);
  EXPECT_TRUE(IsVisible(kControllerTrackpadLabel));
  EXPECT_FALSE(IsVisible(kControllerTrackpadRepositionLabel));
  EXPECT_FALSE(IsVisible(kControllerExitButtonLabel));
  EXPECT_FALSE(IsVisible(kControllerBackButtonLabel));

  model_->push_mode(kModeVoiceSearch);
  EXPECT_TRUE(IsVisible(kControllerTrackpadLabel));
  EXPECT_FALSE(IsVisible(kControllerTrackpadRepositionLabel));
  EXPECT_FALSE(IsVisible(kControllerExitButtonLabel));
  EXPECT_TRUE(IsVisible(kControllerBackButtonLabel));

  model_->pop_mode(kModeVoiceSearch);
  EXPECT_TRUE(IsVisible(kControllerTrackpadLabel));
  EXPECT_FALSE(IsVisible(kControllerTrackpadRepositionLabel));
  EXPECT_FALSE(IsVisible(kControllerExitButtonLabel));
  EXPECT_FALSE(IsVisible(kControllerBackButtonLabel));

  model_->push_mode(kModeRepositionWindow);
  model_->mutable_primary_controller().laser_direction = kForwardVector;
  EXPECT_FALSE(IsVisible(kControllerTrackpadLabel));
  EXPECT_TRUE(IsVisible(kControllerTrackpadRepositionLabel));
  EXPECT_FALSE(IsVisible(kControllerExitButtonLabel));
  EXPECT_TRUE(IsVisible(kControllerRepositionFinishLabel));

  model_->mutable_primary_controller().resting_in_viewport = false;
  EXPECT_FALSE(IsVisible(kControllerTrackpadRepositionLabel));
  EXPECT_FALSE(IsVisible(kControllerTrackpadLabel));
  EXPECT_FALSE(IsVisible(kControllerExitButtonLabel));
  EXPECT_FALSE(IsVisible(kControllerBackButtonLabel));
}

TEST_F(UiTest, ControllerBatteryIndicator) {
  CreateScene(kNotInWebVr);

  Rect* dot_0 =
      static_cast<Rect*>(scene_->GetUiElementByName(kControllerBatteryDot0));
  Rect* dot_1 =
      static_cast<Rect*>(scene_->GetUiElementByName(kControllerBatteryDot1));
  Rect* dot_2 =
      static_cast<Rect*>(scene_->GetUiElementByName(kControllerBatteryDot2));
  Rect* dot_3 =
      static_cast<Rect*>(scene_->GetUiElementByName(kControllerBatteryDot3));
  Rect* dot_4 =
      static_cast<Rect*>(scene_->GetUiElementByName(kControllerBatteryDot4));

  ColorScheme scheme = model_->color_scheme();

  model_->mutable_primary_controller().battery_level = 5;
  RunForMs(kAnimationTimeMs);
  EXPECT_EQ(dot_0->center_color(), scheme.controller_battery_full);
  EXPECT_EQ(dot_1->center_color(), scheme.controller_battery_full);
  EXPECT_EQ(dot_2->center_color(), scheme.controller_battery_full);
  EXPECT_EQ(dot_3->center_color(), scheme.controller_battery_full);
  EXPECT_EQ(dot_4->center_color(), scheme.controller_battery_full);

  model_->mutable_primary_controller().battery_level = 4;
  RunForMs(kAnimationTimeMs);
  EXPECT_EQ(dot_0->center_color(), scheme.controller_battery_full);
  EXPECT_EQ(dot_1->center_color(), scheme.controller_battery_full);
  EXPECT_EQ(dot_2->center_color(), scheme.controller_battery_full);
  EXPECT_EQ(dot_3->center_color(), scheme.controller_battery_full);
  EXPECT_EQ(dot_4->center_color(), scheme.controller_battery_empty);

  model_->mutable_primary_controller().battery_level = 3;
  RunForMs(kAnimationTimeMs);
  EXPECT_EQ(dot_0->center_color(), scheme.controller_battery_full);
  EXPECT_EQ(dot_1->center_color(), scheme.controller_battery_full);
  EXPECT_EQ(dot_2->center_color(), scheme.controller_battery_full);
  EXPECT_EQ(dot_3->center_color(), scheme.controller_battery_empty);
  EXPECT_EQ(dot_4->center_color(), scheme.controller_battery_empty);

  model_->mutable_primary_controller().battery_level = 2;
  RunForMs(kAnimationTimeMs);
  EXPECT_EQ(dot_0->center_color(), scheme.controller_battery_full);
  EXPECT_EQ(dot_1->center_color(), scheme.controller_battery_full);
  EXPECT_EQ(dot_2->center_color(), scheme.controller_battery_empty);
  EXPECT_EQ(dot_3->center_color(), scheme.controller_battery_empty);
  EXPECT_EQ(dot_4->center_color(), scheme.controller_battery_empty);

  model_->mutable_primary_controller().battery_level = 1;
  RunForMs(kAnimationTimeMs);
  EXPECT_EQ(dot_0->center_color(), scheme.controller_battery_full);
  EXPECT_EQ(dot_1->center_color(), scheme.controller_battery_empty);
  EXPECT_EQ(dot_2->center_color(), scheme.controller_battery_empty);
  EXPECT_EQ(dot_3->center_color(), scheme.controller_battery_empty);
  EXPECT_EQ(dot_4->center_color(), scheme.controller_battery_empty);

  model_->mutable_primary_controller().battery_level = 0;
  RunForMs(kAnimationTimeMs);
  EXPECT_EQ(dot_0->center_color(), scheme.controller_battery_empty);
  EXPECT_EQ(dot_1->center_color(), scheme.controller_battery_empty);
  EXPECT_EQ(dot_2->center_color(), scheme.controller_battery_empty);
  EXPECT_EQ(dot_3->center_color(), scheme.controller_battery_empty);
  EXPECT_EQ(dot_4->center_color(), scheme.controller_battery_empty);
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
  EXPECT_EQ(original, repositioner->world_space_transform());
}

// No element in the controller root's subtree should be hit testable.
TEST_F(UiTest, ControllerHitTest) {
  CreateScene(kNotInWebVr);
  auto* controller = scene_->GetUiElementByName(kControllerRoot);
  VerifyNoHitTestableElementInSubtree(controller);
}

TEST_F(UiTest, BrowsingRootBounds) {
  CreateScene(kNotInWebVr);
  auto* elem = scene_->GetUiElementByName(k2dBrowsingContentGroup);
  auto* root = scene_->GetUiElementByName(k2dBrowsingRepositioner);
  for (; elem; elem = elem->parent()) {
    int num_bounds_contributors = 0;
    for (auto& child : elem->children()) {
      if (child->contributes_to_parent_bounds())
        num_bounds_contributors++;
    }
    EXPECT_EQ(1, num_bounds_contributors);
    if (elem == root)
      break;
  }
}

TEST_F(UiTest, DisableResizeWhenEditing) {
  CreateScene(kNotInWebVr);
  UiElement* hit_plane = scene_->GetUiElementByName(kContentFrameHitPlane);
  EXPECT_TRUE(hit_plane->hit_testable());
  model_->editing_web_input = true;
  AdvanceFrame();
  EXPECT_FALSE(hit_plane->hit_testable());
  model_->editing_web_input = false;
  AdvanceFrame();
  EXPECT_TRUE(hit_plane->hit_testable());

  model_->editing_input = true;
  AdvanceFrame();
  EXPECT_FALSE(hit_plane->hit_testable());
  model_->editing_input = false;
  AdvanceFrame();
  EXPECT_TRUE(hit_plane->hit_testable());

  model_->hosted_platform_ui.hosted_ui_enabled = true;
  AdvanceFrame();
  EXPECT_FALSE(hit_plane->hit_testable());
  model_->hosted_platform_ui.hosted_ui_enabled = false;
  AdvanceFrame();
  EXPECT_TRUE(hit_plane->hit_testable());

  model_->active_modal_prompt_type =
      kModalPromptTypeExitVRForVoiceSearchRecordAudioOsPermission;
  AdvanceFrame();
  EXPECT_FALSE(hit_plane->hit_testable());
  model_->active_modal_prompt_type = kModalPromptTypeNone;
  AdvanceFrame();
  EXPECT_TRUE(hit_plane->hit_testable());
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
#if !defined(OS_WIN)
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
#if !defined(OS_WIN)
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

TEST_F(UiTest, MenuItems) {
  CreateScene(kNotInWebVr);
  model_->overflow_menu_enabled = true;

  EXPECT_EQ(IsVisible(kOverflowMenuNewIncognitoTabItem), true);
  EXPECT_EQ(IsVisible(kOverflowMenuCloseAllIncognitoTabsItem), false);
  EXPECT_EQ(IsVisible(kOverflowMenuPreferencesItem), false);

  model_->incognito_tabs_open = true;
  AdvanceFrame();
  EXPECT_EQ(IsVisible(kOverflowMenuNewIncognitoTabItem), true);
  EXPECT_EQ(IsVisible(kOverflowMenuCloseAllIncognitoTabsItem), true);

  model_->incognito = true;
  AdvanceFrame();
  EXPECT_EQ(IsVisible(kOverflowMenuNewIncognitoTabItem), false);
  EXPECT_EQ(IsVisible(kOverflowMenuCloseAllIncognitoTabsItem), true);

  model_->standalone_vr_device = true;
  AdvanceFrame();
  EXPECT_EQ(IsVisible(kOverflowMenuPreferencesItem), true);
}

TEST_F(UiTest, SteadyState) {
  CreateScene(kNotInWebVr);
  RunForSeconds(10.0f);
  // Should have reached steady state.
  EXPECT_FALSE(AdvanceFrame());
}

}  // namespace vr
