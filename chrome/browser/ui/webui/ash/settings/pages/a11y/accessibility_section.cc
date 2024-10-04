// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/a11y/accessibility_section.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ash/color_enhancement/color_enhancement_controller.h"
#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_observer_chromeos.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_observer_chromeos_factory.h"
#include "chrome/browser/ui/webui/ash/settings/pages/a11y/accessibility_handler.h"
#include "chrome/browser/ui/webui/ash/settings/pages/a11y/facegaze_settings_handler.h"
#include "chrome/browser/ui/webui/ash/settings/pages/a11y/select_to_speak_handler.h"
#include "chrome/browser/ui/webui/ash/settings/pages/a11y/switch_access_handler.h"
#include "chrome/browser/ui/webui/ash/settings/pages/a11y/tts_handler.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/settings/accessibility_main_handler.h"
#include "chrome/browser/ui/webui/settings/captions_handler.h"
#include "chrome/browser/ui/webui/settings/font_handler.h"
#include "chrome/browser/ui/webui/settings/shared_settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/live_caption/caption_util.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "extensions/browser/extension_system.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/events/ash/keyboard_layout_util.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kAccessibilitySectionPath;
using ::chromeos::settings::mojom::kAudioAndCaptionsSubpagePath;
using ::chromeos::settings::mojom::kChromeVoxSubpagePath;
using ::chromeos::settings::mojom::kCursorAndTouchpadSubpagePath;
using ::chromeos::settings::mojom::kDisplayAndMagnificationSubpagePath;
using ::chromeos::settings::mojom::kFaceGazeSettingsSubpagePath;
using ::chromeos::settings::mojom::kKeyboardAndTextInputSubpagePath;
using ::chromeos::settings::mojom::kManageAccessibilitySubpagePath;
using ::chromeos::settings::mojom::kSelectToSpeakSubpagePath;
using ::chromeos::settings::mojom::kSwitchAccessOptionsSubpagePath;
using ::chromeos::settings::mojom::kTextToSpeechPagePath;
using ::chromeos::settings::mojom::kTextToSpeechSubpagePath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

const std::vector<SearchConcept>& GetA11ySearchConcepts() {
  const bool kIsRevampEnabled =
      ash::features::IsOsSettingsRevampWayfindingEnabled();

  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_A11Y_ALWAYS_SHOW_OPTIONS,
       mojom::kAccessibilitySectionPath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kA11yQuickSettings},
       {IDS_OS_SETTINGS_TAG_A11Y_ALWAYS_SHOW_OPTIONS_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_TEXT_TO_SPEECH_PAGE,
       mojom::kTextToSpeechPagePath,
       kIsRevampEnabled ? mojom::SearchResultIcon::kTextToSpeech
                        : mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kTextToSpeechPage},
       {IDS_OS_SETTINGS_TAG_A11Y_TEXT_TO_SPEECH_PAGE_ALT1,
        IDS_OS_SETTINGS_TAG_A11Y_TEXT_TO_SPEECH_PAGE_ALT2,
        IDS_OS_SETTINGS_TAG_A11Y_TEXT_TO_SPEECH_PAGE_ALT3,
        IDS_OS_SETTINGS_TAG_A11Y_TEXT_TO_SPEECH_PAGE_ALT4,
        IDS_OS_SETTINGS_TAG_A11Y_TEXT_TO_SPEECH_PAGE_ALT5}},
      {IDS_OS_SETTINGS_TAG_A11Y_DISPLAY_AND_MAGNIFICATION_PAGE,
       mojom::kDisplayAndMagnificationSubpagePath,
       kIsRevampEnabled ? mojom::SearchResultIcon::kZoomIn
                        : mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kDisplayAndMagnification},
       {IDS_OS_SETTINGS_TAG_A11Y_DISPLAY_AND_MAGNIFICATION_PAGE_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_KEYBOARD_AND_TEXT_INPUT_PAGE,
       mojom::kKeyboardAndTextInputSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kKeyboardAndTextInput}},
      {IDS_OS_SETTINGS_TAG_A11Y_CURSOR_AND_TOUCHPAD_PAGE,
       mojom::kCursorAndTouchpadSubpagePath,
       kIsRevampEnabled ? mojom::SearchResultIcon::kCursorClick
                        : mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kCursorAndTouchpad},
       {IDS_OS_SETTINGS_TAG_A11Y_CURSOR_AND_TOUCHPAD_PAGE_ALT1,
        IDS_OS_SETTINGS_TAG_A11Y_CURSOR_AND_TOUCHPAD_PAGE_ALT2,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_AUDIO_AND_CAPTIONS_PAGE,
       mojom::kAudioAndCaptionsSubpagePath,
       kIsRevampEnabled ? mojom::SearchResultIcon::kHearing
                        : mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kAudioAndCaptions},
       {IDS_OS_SETTINGS_TAG_A11Y_AUDIO_AND_CAPTIONS_PAGE_ALT1,
        IDS_OS_SETTINGS_TAG_A11Y_AUDIO_AND_CAPTIONS_PAGE_ALT2,
        IDS_OS_SETTINGS_TAG_A11Y_AUDIO_AND_CAPTIONS_PAGE_ALT3,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_STICKY_KEYS,
       mojom::kKeyboardAndTextInputSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kStickyKeys}},
      {IDS_OS_SETTINGS_TAG_A11Y_LARGE_CURSOR,
       mojom::kCursorAndTouchpadSubpagePath,
       kIsRevampEnabled ? mojom::SearchResultIcon::kCursorClick
                        : mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kLargeCursor},
       {IDS_OS_SETTINGS_TAG_A11Y_LARGE_CURSOR_ALT1,
        IDS_OS_SETTINGS_TAG_A11Y_LARGE_CURSOR_ALT2,
        IDS_OS_SETTINGS_TAG_A11Y_LARGE_CURSOR_ALT3,
        IDS_OS_SETTINGS_TAG_A11Y_LARGE_CURSOR_ALT4, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y,
       mojom::kAccessibilitySectionPath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kHigh,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kAccessibility},
       {IDS_OS_SETTINGS_TAG_A11Y_ALT1, IDS_OS_SETTINGS_TAG_A11Y_ALT2,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_DOCKED_MAGNIFIER,
       mojom::kDisplayAndMagnificationSubpagePath,
       mojom::SearchResultIcon::kDockedMagnifier,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kDockedMagnifier},
       {IDS_OS_SETTINGS_TAG_A11Y_DOCKED_MAGNIFIER_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11y_CHROMEVOX,
       mojom::kTextToSpeechPagePath,
       kIsRevampEnabled ? mojom::SearchResultIcon::kTextToSpeech
                        : mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kChromeVox},
       {IDS_OS_SETTINGS_TAG_A11y_CHROMEVOX_ALT1,
        IDS_OS_SETTINGS_TAG_A11y_CHROMEVOX_ALT2, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_MONO_AUDIO,
       mojom::kAudioAndCaptionsSubpagePath,
       kIsRevampEnabled ? mojom::SearchResultIcon::kHearing
                        : mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kMonoAudio},
       {IDS_OS_SETTINGS_TAG_A11Y_MONO_AUDIO_ALT1, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_TEXT_TO_SPEECH,
       mojom::kTextToSpeechSubpagePath,
       kIsRevampEnabled ? mojom::SearchResultIcon::kTextToSpeech
                        : mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kTextToSpeech},
       {IDS_OS_SETTINGS_TAG_A11Y_TEXT_TO_SPEECH_ALT1,
        IDS_OS_SETTINGS_TAG_A11Y_TEXT_TO_SPEECH_ALT2,
        IDS_OS_SETTINGS_TAG_A11Y_TEXT_TO_SPEECH_ALT3,
        IDS_OS_SETTINGS_TAG_A11Y_TEXT_TO_SPEECH_ALT4}},
      {IDS_OS_SETTINGS_TAG_A11Y_CAPTIONS,
       mojom::kAudioAndCaptionsSubpagePath,
       kIsRevampEnabled ? mojom::SearchResultIcon::kHearing
                        : mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kAudioAndCaptions}},
      {IDS_OS_SETTINGS_TAG_A11Y_HIGHLIGHT_CURSOR,
       mojom::kCursorAndTouchpadSubpagePath,
       kIsRevampEnabled ? mojom::SearchResultIcon::kCursorClick
                        : mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kHighlightCursorWhileMoving},
       {IDS_OS_SETTINGS_TAG_A11Y_HIGHLIGHT_CURSOR_ALT1,
        IDS_OS_SETTINGS_TAG_A11Y_HIGHLIGHT_CURSOR_ALT2,
        IDS_OS_SETTINGS_TAG_A11Y_HIGHLIGHT_CURSOR_ALT3,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_MANAGE,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kManageAccessibility},
       {IDS_OS_SETTINGS_TAG_A11Y_MANAGE_ALT1, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_ON_SCREEN_KEYBOARD,
       mojom::kKeyboardAndTextInputSubpagePath,
       mojom::SearchResultIcon::kOnScreenKeyboard,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kOnScreenKeyboard}},
      {IDS_OS_SETTINGS_TAG_A11Y_HIGHLIGHT_TEXT_CARET,
       mojom::kKeyboardAndTextInputSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kHighlightTextCaret},
       {IDS_OS_SETTINGS_TAG_A11Y_HIGHLIGHT_TEXT_CARET_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_CARET_BROWSING,
       mojom::kKeyboardAndTextInputSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCaretBrowsing},
       {IDS_OS_SETTINGS_TAG_A11Y_CARET_BROWSING_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_DICTATION,
       mojom::kKeyboardAndTextInputSubpagePath,
       mojom::SearchResultIcon::kDictation,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kDictation},
       {IDS_OS_SETTINGS_TAG_A11Y_DICTATION_ALT1,
        IDS_OS_SETTINGS_TAG_A11Y_DICTATION_ALT2,
        IDS_OS_SETTINGS_TAG_A11Y_DICTATION_ALT3,
        IDS_OS_SETTINGS_TAG_A11Y_DICTATION_ALT4, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_HIGH_CONTRAST,
       mojom::kDisplayAndMagnificationSubpagePath,
       mojom::SearchResultIcon::kContrast,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kHighContrastMode},
       {IDS_OS_SETTINGS_TAG_A11Y_HIGH_CONTRAST_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_HIGHLIGHT_KEYBOARD_FOCUS,
       mojom::kKeyboardAndTextInputSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kHighlightKeyboardFocus},
       {IDS_OS_SETTINGS_TAG_A11Y_HIGHLIGHT_KEYBOARD_FOCUS_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_STARTUP_SOUND,
       mojom::kAudioAndCaptionsSubpagePath,
       kIsRevampEnabled ? mojom::SearchResultIcon::kHearing
                        : mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kStartupSound},
       {IDS_OS_SETTINGS_TAG_A11Y_STARTUP_SOUND_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_AUTOMATICALLY_CLICK,
       mojom::kCursorAndTouchpadSubpagePath,
       mojom::SearchResultIcon::kAutoclick,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAutoClickWhenCursorStops},
       {IDS_OS_SETTINGS_TAG_A11Y_AUTOMATICALLY_CLICK_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_SELECT_TO_SPEAK,
       mojom::kTextToSpeechPagePath,
       mojom::SearchResultIcon::kSelectToSpeak,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSelectToSpeak}},
      {IDS_OS_SETTINGS_TAG_A11Y_SPEECH_PITCH,
       mojom::kTextToSpeechSubpagePath,
       kIsRevampEnabled ? mojom::SearchResultIcon::kTextToSpeech
                        : mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTextToSpeechPitch}},
      {IDS_OS_SETTINGS_TAG_A11Y_SPEECH_RATE,
       mojom::kTextToSpeechSubpagePath,
       kIsRevampEnabled ? mojom::SearchResultIcon::kTextToSpeech
                        : mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTextToSpeechRate},
       {IDS_OS_SETTINGS_TAG_A11Y_SPEECH_RATE_ALT1, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_SPEECH_VOLUME,
       mojom::kTextToSpeechSubpagePath,
       kIsRevampEnabled ? mojom::SearchResultIcon::kTextToSpeech
                        : mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTextToSpeechVolume}},
      {IDS_OS_SETTINGS_TAG_A11Y_FULLSCREEN_MAGNIFIER,
       mojom::kDisplayAndMagnificationSubpagePath,
       mojom::SearchResultIcon::kFullscreenMagnifier,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kFullscreenMagnifier},
       {IDS_OS_SETTINGS_TAG_A11Y_FULLSCREEN_MAGNIFIER_ALT1,
        IDS_OS_SETTINGS_TAG_A11Y_FULLSCREEN_MAGNIFIER_ALT2,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_ENABLE_SWITCH_ACCESS,
       mojom::kKeyboardAndTextInputSubpagePath,
       mojom::SearchResultIcon::kSwitchAccess,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kEnableSwitchAccess}},
      {IDS_OS_SETTINGS_TAG_A11Y_CURSOR_COLOR,
       mojom::kCursorAndTouchpadSubpagePath,
       kIsRevampEnabled ? mojom::SearchResultIcon::kCursorClick
                        : mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kEnableCursorColor},
       {IDS_OS_SETTINGS_TAG_A11Y_CURSOR_COLOR_ALT1,
        IDS_OS_SETTINGS_TAG_A11Y_CURSOR_COLOR_ALT2, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_REDUCED_ANIMATIONS,
       mojom::kDisplayAndMagnificationSubpagePath,
       mojom::SearchResultIcon::kReducedAnimations,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kReducedAnimationsEnabled},
       {IDS_OS_SETTINGS_TAG_A11Y_REDUCED_ANIMATIONS_ALT1,
        IDS_OS_SETTINGS_TAG_A11Y_REDUCED_ANIMATIONS_ALT2,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetTextToSpeechVoiceSearchConcepts() {
  const bool kIsRevampEnabled =
      ash::features::IsOsSettingsRevampWayfindingEnabled();

  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_A11Y_SPEECH_VOICE_PREVIEW,
       mojom::kTextToSpeechSubpagePath,
       kIsRevampEnabled ? mojom::SearchResultIcon::kTextToSpeech
                        : mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTextToSpeechVoice}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetTextToSpeechEnginesSearchConcepts() {
  const bool kIsRevampEnabled =
      ash::features::IsOsSettingsRevampWayfindingEnabled();

  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_A11Y_SPEECH_ENGINES,
       mojom::kTextToSpeechSubpagePath,
       kIsRevampEnabled ? mojom::SearchResultIcon::kTextToSpeech
                        : mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTextToSpeechEngines}},
  });
  return *tags;
}

const std::vector<SearchConcept>&
GetA11yTabletNavigationButtonSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_A11Y_TABLET_NAVIGATION_BUTTONS,
       mojom::kCursorAndTouchpadSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTabletNavigationButtons}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetA11ySwitchAccessOnSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_A11Y_SWITCH_ACCESS_ASSIGNMENT,
       mojom::kSwitchAccessOptionsSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSwitchActionAssignment}},
      {IDS_OS_SETTINGS_TAG_A11Y_SWITCH_ACCESS,
       mojom::kSwitchAccessOptionsSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kSwitchAccessOptions}},
      {IDS_OS_SETTINGS_TAG_A11Y_SWITCH_ACCESS_AUTO_SCAN,
       mojom::kSwitchAccessOptionsSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSwitchActionAutoScan}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetA11yOverscrollSettingSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_A11Y_OVERSCROLL_ENABLED,
       mojom::kCursorAndTouchpadSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kOverscrollEnabled},
       {IDS_OS_SETTINGS_TAG_A11Y_OVERSCROLL_ENABLED_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetA11yFlashNotificationsSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_A11Y_FLASH_NOTIFICATIONS,
       mojom::kAudioAndCaptionsSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kFlashNotifications},
       {IDS_OS_SETTINGS_TAG_A11Y_FLASH_NOTIFICATIONS_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetA11ySwitchAccessKeyboardSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_A11Y_SWITCH_ACCESS_AUTO_SCAN_KEYBOARD,
       mojom::kSwitchAccessOptionsSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSwitchActionAutoScanKeyboard}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetA11yLabelsSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_A11Y_LABELS_FROM_GOOGLE,
       mojom::kAccessibilitySectionPath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kGetImageDescriptionsFromGoogle}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetA11yLiveCaptionSearchConcepts() {
  const bool kIsRevampEnabled =
      ash::features::IsOsSettingsRevampWayfindingEnabled();

  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_A11Y_LIVE_CAPTION,
       mojom::kAudioAndCaptionsSubpagePath,
       kIsRevampEnabled ? mojom::SearchResultIcon::kHearing
                        : mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kLiveCaption},
       {IDS_OS_SETTINGS_TAG_A11Y_LIVE_CAPTION_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>&
GetA11yFullscreenMagnifierFocusFollowingSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_A11Y_FULLSCREEN_MAGNIFIER_FOCUS_FOLLOWING,
       mojom::kDisplayAndMagnificationSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kFullscreenMagnifierFocusFollowing}},
  });
  return *tags;
}

const std::vector<SearchConcept>&
GetA11yFullscreenMagnifierSelectToSpeakFocusFollowingSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_A11Y_MAGNIFIER_SELECT_TO_SPEAK_FOLLOWING,
       mojom::kDisplayAndMagnificationSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAccessibilityMagnifierFollowsSts}},
  });
  return *tags;
}

const std::vector<SearchConcept>&
GetA11yMagnifierChromeVoxFocusFollowingSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_A11Y_MAGNIFIER_CHROMEVOX_FOLLOWING,
       mojom::kDisplayAndMagnificationSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kMagnifierFollowsChromeVox}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetA11yColorCorrectionSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_A11Y_COLOR_CORRECTION,
       mojom::kDisplayAndMagnificationSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kColorCorrectionEnabled},
       {IDS_OS_SETTINGS_TAG_A11Y_COLOR_CORRECTION_ALT1,
        IDS_OS_SETTINGS_TAG_A11Y_COLOR_CORRECTION_ALT2,
        IDS_OS_SETTINGS_TAG_A11Y_COLOR_CORRECTION_ALT3,
        IDS_OS_SETTINGS_TAG_A11Y_COLOR_CORRECTION_ALT4,
        IDS_OS_SETTINGS_TAG_A11Y_COLOR_CORRECTION_ALT5}},
  });
  return *tags;
}

bool IsLiveCaptionEnabled() {
  return captions::IsLiveCaptionFeatureSupported();
}

bool IsSwitchAccessTextAllowed() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableExperimentalAccessibilitySwitchAccessText);
}

bool AreTabletNavigationButtonsAllowed() {
  return features::IsHideShelfControlsInTabletModeEnabled() &&
         TabletMode::IsBoardTypeMarkedAsTabletCapable();
}

int GetDisplayAndMangificationLinkDescriptionResourceId() {
  return IDS_SETTINGS_ACCESSIBILITY_DISPLAY_AND_MAGNIFICATION_LINK_NEW_DESCRIPTION;
}

bool IsAccessibilityReducedAnimationsEnabled() {
  return ::features::IsAccessibilityReducedAnimationsEnabled();
}

bool IsAccessibilityMagnifierFollowsChromeVoxEnabled() {
  return ::features::IsAccessibilityMagnifierFollowsChromeVoxEnabled();
}

bool IsAccessibilityMagnifierFollowsStsEnabled() {
  return ::features::IsAccessibilityMagnifierFollowsStsEnabled();
}

bool IsAccessibilityFaceGazeEnabled() {
  return ::features::IsAccessibilityFaceGazeEnabled();
}

bool IsAccessibilityMouseKeysEnabled() {
  return ::features::IsAccessibilityMouseKeysEnabled();
}

bool IsAccessibilityDisableTrackpadEnabled() {
  return ::features::IsAccessibilityDisableTrackpadEnabled();
}

bool IsAccessibilityOverscrollSettingFeatureEnabled() {
  return ::features::IsAccessibilityOverscrollSettingFeatureEnabled();
}

bool IsAccessibilityFlashNotificationFeatureEnabled() {
  return ::features::IsAccessibilityFlashScreenFeatureEnabled();
}

}  // namespace

AccessibilitySection::AccessibilitySection(
    Profile* profile,
    SearchTagRegistry* search_tag_registry,
    PrefService* pref_service)
    : OsSettingsSection(profile, search_tag_registry),
      pref_service_(pref_service) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.AddSearchTags(GetA11ySearchConcepts());

  if (AreTabletNavigationButtonsAllowed()) {
    updater.AddSearchTags(GetA11yTabletNavigationButtonSearchConcepts());
  }

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kAccessibilitySwitchAccessEnabled,
      base::BindRepeating(&AccessibilitySection::UpdateSearchTags,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kAccessibilitySwitchAccessAutoScanEnabled,
      base::BindRepeating(&AccessibilitySection::UpdateSearchTags,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kAccessibilityScreenMagnifierEnabled,
      base::BindRepeating(&AccessibilitySection::UpdateSearchTags,
                          base::Unretained(this)));

  UpdateSearchTags();

  // ExtensionService can be null for tests.
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!extension_service) {
    return;
  }
  content::TtsController::GetInstance()->AddVoicesChangedDelegate(this);
  extension_registry_ = extensions::ExtensionRegistry::Get(profile);
  extension_registry_->AddObserver(this);

  UpdateTextToSpeechVoiceSearchTags();
  UpdateTextToSpeechEnginesSearchTags();
}

AccessibilitySection::~AccessibilitySection() {
  content::TtsController::GetInstance()->RemoveVoicesChangedDelegate(this);
  if (extension_registry_) {
    extension_registry_->RemoveObserver(this);
  }
}

void AccessibilitySection::AddLoadTimeData(
    content::WebUIDataSource* html_source) {
  const bool kIsRevampEnabled =
      ash::features::IsOsSettingsRevampWayfindingEnabled();

  webui::LocalizedString kLocalizedStrings[] = {
      {"a11yExplanation", IDS_SETTINGS_ACCESSIBILITY_EXPLANATION},
      {"a11yPageTitle", IDS_SETTINGS_ACCESSIBILITY},
      {"a11yMenuItemDescription",
       IDS_OS_SETTINGS_ACCESSIBILITY_MENU_ITEM_DESCRIPTION},
      {"a11yWebStore", IDS_SETTINGS_ACCESSIBILITY_WEB_STORE},
      {"accessibleImageLabelsSubtitle",
       IDS_SETTINGS_ACCESSIBLE_IMAGE_LABELS_SUBTITLE},
      {"accessibilityFaceGazeLabel",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_LABEL},
      {"accessibilityFaceGazeDescription",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_DESCRIPTION},
      {"accessibleImageLabelsTitle",
       IDS_SETTINGS_ACCESSIBLE_IMAGE_LABELS_TITLE},
      {"additionalFeaturesTitle",
       IDS_SETTINGS_ACCESSIBILITY_ADDITIONAL_FEATURES_TITLE},
      {"appearanceSettingsDescription",
       IDS_SETTINGS_ACCESSIBILITY_APPEARANCE_SETTINGS_DESCRIPTION},
      {"appearanceSettingsTitle",
       IDS_SETTINGS_ACCESSIBILITY_APPEARANCE_SETTINGS_TITLE},
      {"assignNextSwitchLabel", IDS_SETTINGS_ASSIGN_NEXT_SWITCH_LABEL},
      {"assignPreviousSwitchLabel", IDS_SETTINGS_ASSIGN_PREVIOUS_SWITCH_LABEL},
      {"assignSelectSwitchLabel", IDS_SETTINGS_ASSIGN_SELECT_SWITCH_LABEL},
      {"assignSwitchSubLabel0Switches",
       IDS_SETTINGS_ASSIGN_SWITCH_SUB_LABEL_0_SWITCHES},
      {"assignSwitchSubLabel1Switch",
       IDS_SETTINGS_ASSIGN_SWITCH_SUB_LABEL_1_SWITCH},
      {"assignSwitchSubLabel2Switches",
       IDS_SETTINGS_ASSIGN_SWITCH_SUB_LABEL_2_SWITCHES},
      {"assignSwitchSubLabel3Switches",
       IDS_SETTINGS_ASSIGN_SWITCH_SUB_LABEL_3_SWITCHES},
      {"assignSwitchSubLabel4Switches",
       IDS_SETTINGS_ASSIGN_SWITCH_SUB_LABEL_4_SWITCHES},
      {"assignSwitchSubLabel5OrMoreSwitches",
       IDS_SETTINGS_ASSIGN_SWITCH_SUB_LABEL_5_OR_MORE_SWITCHES},
      {"audioAndCaptionsHeading",
       IDS_SETTINGS_ACCESSIBILITY_AUDIO_AND_CAPTIONS_HEADING},
      {"audioAndCaptionsLinkDescription",
       IDS_SETTINGS_ACCESSIBILITY_AUDIO_AND_CAPTIONS_LINK_DESCRIPTION},
      {"audioAndCaptionsLinkTitle",
       IDS_SETTINGS_ACCESSIBILITY_AUDIO_AND_CAPTIONS_LINK_TITLE},
      {"autoclickMovementThresholdDefault",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_DEFAULT},
      {"autoclickMovementThresholdExtraLarge",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_EXTRA_LARGE},
      {"autoclickMovementThresholdExtraSmall",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_EXTRA_SMALL},
      {"autoclickMovementThresholdLabel",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_LABEL},
      {"autoclickMovementThresholdLarge",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_LARGE},
      {"autoclickMovementThresholdSmall",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_SMALL},
      {"autoclickRevertToLeftClick",
       IDS_SETTINGS_AUTOCLICK_REVERT_TO_LEFT_CLICK},
      {"autoclickStabilizeCursorPosition",
       IDS_SETTINGS_AUTOCLICK_STABILIZE_CURSOR_POSITION},
      {"mouseKeysLabel", IDS_OS_SETTINGS_ACCESSIBILITY_MOUSE_KEYS_LABEL},
      {"mouseKeysDescription",
       IDS_OS_SETTINGS_ACCESSIBILITY_MOUSE_KEYS_DESCRIPTION},
      {"mouseKeysAcceleration",
       IDS_OS_SETTINGS_ACCESSIBILITY_MOUSE_KEYS_ACCELERATION},
      {"mouseKeysAccelerationMinLabel",
       IDS_OS_SETTINGS_ACCESSIBILITY_MOUSE_KEYS_ACCELERATION_MIN_LABEL},
      {"mouseKeysAccelerationMaxLabel",
       IDS_OS_SETTINGS_ACCESSIBILITY_MOUSE_KEYS_ACCELERATION_MAX_LABEL},
      {"mouseKeysMaxSpeed", IDS_OS_SETTINGS_ACCESSIBILITY_MOUSE_KEYS_MAX_SPEED},
      {"mouseKeysMaxSpeedMinLabel",
       IDS_OS_SETTINGS_ACCESSIBILITY_MOUSE_KEYS_MAX_SPEED_MIN_LABEL},
      {"mouseKeysMaxSpeedMaxLabel",
       IDS_OS_SETTINGS_ACCESSIBILITY_MOUSE_KEYS_MAX_SPEED_MAX_LABEL},
      {"mouseKeysUsePrimaryKeys",
       IDS_OS_SETTINGS_ACCESSIBILITY_MOUSE_KEYS_USE_PRIMARY_KEYS},
      {"mouseKeysDominantHand",
       IDS_OS_SETTINGS_ACCESSIBILITY_MOUSE_KEYS_DOMINANT_HAND},
      {"mouseKeysRightHand",
       IDS_OS_SETTINGS_ACCESSIBILITY_MOUSE_KEYS_RIGHT_HAND},
      {"mouseKeysLeftHand", IDS_OS_SETTINGS_ACCESSIBILITY_MOUSE_KEYS_LEFT_HAND},
      {"cancel", IDS_CANCEL},
      {"caretBrowsingLabel",
       IDS_SETTINGS_ACCESSIBILITY_CARET_BROWSING_DESCRIPTION},
      {"caretBrowsingLabelSubtext",
       IDS_SETTINGS_ACCESSIBILITY_CARET_BROWSING_DESCRIPTION_SUBTEXT},
      {"caretBrowsingSubtitle", IDS_SETTINGS_ENABLE_CARET_BROWSING_SUBTITLE},
      {"caretBrowsingTitle", IDS_SETTINGS_ENABLE_CARET_BROWSING_TITLE},
      {"caretHighlightLabel",
       IDS_SETTINGS_ACCESSIBILITY_CARET_HIGHLIGHT_DESCRIPTION},
      {"caretHighlightLabelSubtext",
       IDS_SETTINGS_ACCESSIBILITY_CARET_HIGHLIGHT_DESCRIPTION_SUBTEXT},
      {"chromeVoxDescriptionOff", IDS_SETTINGS_CHROMEVOX_DESCRIPTION_OFF},
      {"chromeVoxDescriptionOn", IDS_SETTINGS_CHROMEVOX_DESCRIPTION_ON},
      {"chromeVoxLabel", IDS_SETTINGS_CHROMEVOX_LABEL},
      {"chromeVoxOptionsLabel", IDS_SETTINGS_CHROMEVOX_OPTIONS_LABEL},
      {"chromeVoxGeneralLabel", IDS_SETTINGS_CHROMEVOX_GENERAL_LABEL},
      {"chromeVoxVoicesLabel", IDS_SETTINGS_CHROMEVOX_VOICES_LABEL},
      {"chromeVoxBrailleLabel", IDS_SETTINGS_CHROMEVOX_BRAILLE_LABEL},
      {"chromeVoxDeveloperOptionsLabel",
       IDS_SETTINGS_CHROMEVOX_DEVELOPER_OPTIONS_LABEL},
      {"chromeVoxUseVerboseMode", IDS_SETTINGS_CHROMEVOX_USE_VERBOSE_MODE},
      {"chromeVoxAutoRead", IDS_SETTINGS_CHROMEVOX_AUTO_READ},
      {"chromeVoxSpeakTextUnderMouse",
       IDS_SETTINGS_CHROMEVOX_SPEAK_TEXT_UNDER_MOUSE},
      {"chromeVoxUsePitchChanges", IDS_SETTINGS_CHROMEVOX_USE_PITCH_CHANGES},
      {"chromeVoxAnnounceRichTextAttributes",
       IDS_SETTINGS_CHROMEVOX_ANNOUNCE_RICH_TEXT_ATTRIBUTES},
      {"chromeVoxCapitalStrategy", IDS_SETTINGS_CHROMEVOX_CAPITAL_STRATEGY},
      {"chromeVoxAnnounceCapitals", IDS_SETTINGS_CHROMEVOX_ANNOUNCE_CAPITALS},
      {"chromeVoxIncreasePitch", IDS_SETTINGS_CHROMEVOX_INCREASE_PITCH},
      {"chromeVoxNumberReadingStyle",
       IDS_SETTINGS_CHROMEVOX_NUMBER_READING_STYLE},
      {"chromeVoxAsWords", IDS_SETTINGS_CHROMEVOX_NUMBER_READING_STYLE_WORDS},
      {"chromeVoxAsDigits", IDS_SETTINGS_CHROMEVOX_NUMBER_READING_STYLE_DIGITS},
      {"chromeVoxPunctuationEcho", IDS_SETTINGS_CHROMEVOX_PUNCTUATION_ECHO},
      {"chromeVoxNone", IDS_SETTINGS_CHROMEVOX_PUNCTUATION_ECHO_NONE},
      {"chromeVoxSome", IDS_SETTINGS_CHROMEVOX_PUNCTUATION_ECHO_SOME},
      {"chromeVoxAll", IDS_SETTINGS_CHROMEVOX_PUNCTUATION_ECHO_ALL},
      {"chromeVoxAnnounceDownloadNotifications",
       IDS_SETTINGS_CHROMEVOX_ANNOUNCE_DOWNLOAD_NOTIFICATIONS},
      {"chromeVoxSmartStickyMode", IDS_SETTINGS_CHROMEVOX_SMART_STICKY_MODE},
      {"chromeVoxAudioStrategy", IDS_SETTINGS_CHROMEVOX_AUDIO_STRATEGY},
      {"chromeVoxAudioNormal", IDS_SETTINGS_CHROMEVOX_AUDIO_NORMAL},
      {"chromeVoxAudioDuck", IDS_SETTINGS_CHROMEVOX_AUDIO_DUCK},
      {"chromeVoxAudioSuspend", IDS_SETTINGS_CHROMEVOX_AUDIO_SUSPEND},
      {"chromeVoxVoice", IDS_SETTINGS_CHROMEVOX_VOICE},
      {"chromeVoxSystemVoice", IDS_SETTINGS_CHROMEVOX_SYSTEM_VOICE},
      {"chromeVoxLanguageSwitching", IDS_SETTINGS_CHROMEVOX_LANGUAGE_SWITCHING},
      {"chromeVoxTtsSettingsLink", IDS_SETTINGS_CHROMEVOX_TTS_SETTINGS_LINK},
      {"chromeVoxTtsSettingsDescription",
       IDS_SETTINGS_CHROMEVOX_TTS_SETTINGS_DESCRIPTION},
      {"chromeVoxBrailleWordWrap", IDS_SETTINGS_CHROMEVOX_BRAILLE_WORD_WRAP},
      {"chromeVoxMenuBrailleCommands",
       IDS_SETTINGS_CHROMEVOX_MENU_BRAILLE_COMMANDS},
      {"chromeVoxBluetoothBrailleDisplayConnect",
       IDS_SETTINGS_CHROMEVOX_BLUETOOTH_BRAILLE_DISPLAY_CONNECT},
      {"chromeVoxBluetoothBrailleDisplayDisconnect",
       IDS_SETTINGS_CHROMEVOX_BLUETOOTH_BRAILLE_DISPLAY_DISCONNECT},
      {"chromeVoxBluetoothBrailleDisplayConnecting",
       IDS_SETTINGS_CHROMEVOX_BLUETOOTH_BRAILLE_DISPLAY_CONNECTING},
      {"chromeVoxBluetoothBrailleDisplayForget",
       IDS_SETTINGS_CHROMEVOX_BLUETOOTH_BRAILLE_DISPLAY_FORGET},
      {"chromeVoxBluetoothBrailleDisplayPincodeLabel",
       IDS_SETTINGS_CHROMEVOX_BLUETOOTH_BRAILLE_DISPLAY_PINCODE_LABEL},
      {"chromeVoxBluetoothBrailleDisplaySelectLabel",
       IDS_SETTINGS_CHROMEVOX_BLUETOOTH_BRAILLE_DISPLAY_SELECT_LABEL},
      {"chromeVoxVirtualBrailleDisplay",
       IDS_SETTINGS_CHROMEVOX_VIRTUAL_BRAILLE_DISPLAY},
      {"chromeVoxVirtualBrailleDisplayDetails",
       IDS_SETTINGS_CHROMEVOX_VIRTUAL_BRAILLE_DISPLAY_DETAILS},
      {"chromeVoxVirtualBrailleDisplayRows",
       IDS_SETTINGS_CHROMEVOX_VIRTUAL_BRAILLE_DISPLAY_ROWS},
      {"chromeVoxVirtualBrailleDisplayColumns",
       IDS_SETTINGS_CHROMEVOX_VIRTUAL_BRAILLE_DISPLAY_COLUMNS},
      {"chromeVoxVirtualBrailleDisplayStyleLabel",
       IDS_SETTINGS_CHROMEVOX_VIRTUAL_BRAILLE_DISPLAY_STYLE_LABEL},
      {"chromeVoxVirtualBrailleDisplayStyleInterleave",
       IDS_SETTINGS_CHROMEVOX_VIRTUAL_BRAILLE_DISPLAY_STYLE_INTERLEAVE},
      {"chromeVoxVirtualBrailleDisplayStyleSideBySide",
       IDS_SETTINGS_CHROMEVOX_VIRTUAL_BRAILLE_DISPLAY_STYLE_SIDE_BY_SIDE},
      {"chromeVoxEventLogLink", IDS_SETTINGS_CHROMEVOX_EVENT_LOG_LINK},
      {"chromeVoxEventLogDescription",
       IDS_SETTINGS_CHROMEVOX_EVENT_LOG_DESCRIPTION},
      {"chromeVoxEnableSpeechLogging",
       IDS_SETTINGS_CHROMEVOX_MENU_ENABLE_SPEECH_LOGGING},
      {"chromeVoxEnableEarconLogging",
       IDS_SETTINGS_CHROMEVOX_MENU_ENABLE_EARCON_LOGGING},
      {"chromeVoxEnableBrailleLogging",
       IDS_SETTINGS_CHROMEVOX_MENU_ENABLE_BRAILLE_LOGGING},
      {"chromeVoxEnableEventStreamLogging",
       IDS_SETTINGS_CHROMEVOX_MENU_ENABLE_EVENT_STREAM_LOGGING},
      {"chromeVoxBrailleTableDescription",
       IDS_SETTINGS_CHROMEVOX_BRAILLE_TABLE_DESCRIPTION},
      {"chromeVoxBrailleTable6Dot", IDS_SETTINGS_CHROMEVOX_BRAILLE_TABLE_6_DOT},
      {"chromeVoxBrailleTable8Dot", IDS_SETTINGS_CHROMEVOX_BRAILLE_TABLE_8_DOT},
      {"chromeVoxBrailleTableNameWithGrade",
       IDS_SETTINGS_CHROMEVOX_BRAILLE_TABLE_NAME_WITH_GRADE},
      {"chromeVoxBrailleTableNameWithVariant",
       IDS_SETTINGS_CHROMEVOX_BRAILLE_TABLE_NAME_WITH_VARIANT},
      {"chromeVoxBrailleTableNameWithVariantAndGrade",
       IDS_SETTINGS_CHROMEVOX_BRAILLE_TABLE_NAME_WITH_VARIANT_AND_GRADE},
      {"chromeVoxTutorialLabel", IDS_SETTINGS_CHROMEVOX_TUTORIAL_LABEL},
      {"clickOnStopDescription", IDS_SETTINGS_CLICK_ON_STOP_DESCRIPTION},
      {"clickOnStopLabel", IDS_SETTINGS_CLICK_ON_STOP_LABEL},
      {"colorFilterMaxLabel", IDS_SETTINGS_COLOR_FILTER_MAXIMUM_LABEL},
      {"colorFilterMinLabel", IDS_SETTINGS_COLOR_FILTER_MINIMUM_LABEL},
      {"cursorAndTouchpadLinkDescription",
       IDS_SETTINGS_ACCESSIBILITY_CURSOR_AND_TOUCHPAD_LINK_DESCRIPTION},
      {"cursorAndTouchpadLinkTitle",
       IDS_SETTINGS_ACCESSIBILITY_CURSOR_AND_TOUCHPAD_LINK_TITLE},
      {"cursorColorBlack", IDS_SETTINGS_CURSOR_COLOR_BLACK},
      {"cursorColorBlue", IDS_SETTINGS_CURSOR_COLOR_BLUE},
      {"cursorColorCyan", IDS_SETTINGS_CURSOR_COLOR_CYAN},
      {"cursorColorGreen", IDS_SETTINGS_CURSOR_COLOR_GREEN},
      {"cursorColorMagenta", IDS_SETTINGS_CURSOR_COLOR_MAGENTA},
      {"cursorColorOptionsLabel", IDS_SETTINGS_CURSOR_COLOR_OPTIONS_LABEL},
      {"cursorColorPink", IDS_SETTINGS_CURSOR_COLOR_PINK},
      {"cursorColorRed", IDS_SETTINGS_CURSOR_COLOR_RED},
      {"cursorColorYellow", IDS_SETTINGS_CURSOR_COLOR_YELLOW},
      {"cursorHighlightLabel",
       IDS_SETTINGS_ACCESSIBILITY_CURSOR_HIGHLIGHT_DESCRIPTION},
      {"defaultPercentage", IDS_SETTINGS_DEFAULT_PERCENTAGE},
      {"delayBeforeClickExtremelyShort",
       IDS_SETTINGS_DELAY_BEFORE_CLICK_EXTREMELY_SHORT},
      {"delayBeforeClickLabel", IDS_SETTINGS_DELAY_BEFORE_CLICK_LABEL},
      {"delayBeforeClickLong", IDS_SETTINGS_DELAY_BEFORE_CLICK_LONG},
      {"delayBeforeClickShort", IDS_SETTINGS_DELAY_BEFORE_CLICK_SHORT},
      {"delayBeforeClickVeryLong", IDS_SETTINGS_DELAY_BEFORE_CLICK_VERY_LONG},
      {"delayBeforeClickVeryShort", IDS_SETTINGS_DELAY_BEFORE_CLICK_VERY_SHORT},
      {"dictationChangeLanguageButton",
       IDS_SETTINGS_ACCESSIBILITY_DICTATION_CHANGE_LANGUAGE_BUTTON},
      {"dictationChangeLanguageDialogAll",
       IDS_SETTINGS_ACCESSIBILITY_DICTATION_LANGUAGE_DIALOG_ALL},
      {"dictationChangeLanguageDialogCancelButton",
       IDS_SETTINGS_ACCESSIBILITY_DICTATION_LANGUAGE_DIALOG_CANCEL_BUTTON},
      {"dictationChangeLanguageDialogNoResults",
       IDS_SETTINGS_ACCESSIBILITY_DICTATION_LANGUAGE_DIALOG_NO_RESULTS},
      {"dictationChangeLanguageDialogNotSelectedDescription",
       IDS_SETTINGS_ACCESSIBILITY_DICTATION_LANGUAGE_DIALOG_NOT_SELECTED_DESCRIPTION},
      {"dictationChangeLanguageDialogOfflineDescription",
       IDS_SETTINGS_ACCESSIBILITY_DICTATION_LANGUAGE_DIALOG_OFFLINE_DESCRIPTION},
      {"dictationChangeLanguageDialogRecommended",
       IDS_SETTINGS_ACCESSIBILITY_DICTATION_LANGUAGE_DIALOG_RECOMMENDED},
      {"dictationChangeLanguageDialogSearchClear",
       IDS_SETTINGS_ACCESSIBILITY_DICTATION_LANGUAGE_DIALOG_SEARCH_CLEAR},
      {"dictationChangeLanguageDialogSearchHint",
       IDS_SETTINGS_ACCESSIBILITY_DICTATION_LANGUAGE_DIALOG_SEARCH_HINT},
      {"dictationChangeLanguageDialogSelectedDescription",
       IDS_SETTINGS_ACCESSIBILITY_DICTATION_LANGUAGE_DIALOG_SELECTED_DESCRIPTION},
      {"dictationChangeLanguageDialogTitle",
       IDS_SETTINGS_ACCESSIBILITY_DICTATION_CHANGE_LANGUAGE_DIALOG_TITLE},
      {"dictationChangeLanguageDialogUpdateButton",
       IDS_SETTINGS_ACCESSIBILITY_DICTATION_LANGUAGE_DIALOG_UPDATE_BUTTON},
      {"dictationDescription",
       IDS_SETTINGS_ACCESSIBILITY_DICTATION_NEW_DESCRIPTION},
      {"dictationLabel", IDS_SETTINGS_ACCESSIBILITY_DICTATION_LABEL},
      {"dictationLocaleMenuLabel",
       IDS_SETTINGS_ACCESSIBILITY_DICTATION_LOCALE_MENU_LABEL},
      {"dictationLocaleOfflineSubtitle",
       IDS_SETTINGS_ACCESSIBILITY_DICTATION_LANGUAGE_DIALOG_OFFLINE_SUBTITLE},
      {"dictationLocaleSubLabelNetwork",
       IDS_SETTINGS_ACCESSIBILITY_DICTATION_LOCALE_SUB_LABEL_NETWORK},
      // For temporary network label, we can use the string that's shown when a
      // SODA download fails.
      {"dictationLocaleSubLabelNetworkTemporarily",
       IDS_SETTINGS_ACCESSIBILITY_DICTATION_SUBTITLE_SODA_DOWNLOAD_ERROR},
      {"dictationLocaleSubLabelOffline",
       IDS_SETTINGS_ACCESSIBILITY_DICTATION_LOCALE_SUB_LABEL_OFFLINE},
      {"disableTrackpadLabel", IDS_SETTINGS_DISABLE_TRACKPAD_LABEL},
      {"disableTrackpadAlways", IDS_SETTINGS_DISABLE_TRACKPAD_ALWAYS},
      {"disableTrackpadMouseConnected",
       IDS_SETTINGS_DISABLE_TRACKPAD_MOUSE_CONNECTED},
      {"disableTrackpadNever", IDS_SETTINGS_DISABLE_TRACKPAD_NEVER},
      {"displayAndMagnificationLinkTitle",
       IDS_SETTINGS_ACCESSIBILITY_DISPLAY_AND_MAGNIFICATION_LINK_TITLE},
      {"displayHeading", IDS_SETTINGS_ACCESSIBILITY_DISPLAY_HEADING},
      {"displaySettingsDescription",
       IDS_SETTINGS_ACCESSIBILITY_DISPLAY_SETTINGS_DESCRIPTION},
      {"displaySettingsTitle",
       IDS_SETTINGS_ACCESSIBILITY_DISPLAY_SETTINGS_TITLE},
      {"dockedMagnifierDescription", IDS_SETTINGS_DOCKED_MAGNIFIER_DESCRIPTION},
      {"dockedMagnifierLabel", IDS_SETTINGS_DOCKED_MAGNIFIER_LABEL},
      {"dockedMagnifierZoomLabel", IDS_SETTINGS_DOCKED_MAGNIFIER_ZOOM_LABEL},
      {"durationInSeconds", IDS_SETTINGS_DURATION_IN_SECONDS},
      {"reEnableTrackpadLabel", IDS_SETTINGS_RE_ENABLE_TRACKPAD},
      {"focusHighlightLabel",
       IDS_SETTINGS_ACCESSIBILITY_FOCUS_HIGHLIGHT_DESCRIPTION},
      {"focusHighlightLabelSubtext",
       IDS_SETTINGS_ACCESSIBILITY_FOCUS_HIGHLIGHT_DESCRIPTION_SUBTEXT},
      {"focusHighlightDisabledByChromevoxTooltip",
       IDS_SETTINGS_FOCUS_HIGHLIGHT_DISABLED_BY_CHROMEVOX_TOOLTIP},
      {"greyscaleLabel", IDS_SETTINGS_GREYSCALE_LABEL},
      {"highContrastDescription", IDS_SETTINGS_HIGH_CONTRAST_DESCRIPTION},
      {"highContrastLabel", IDS_SETTINGS_HIGH_CONTRAST_LABEL},
      {"protanomalyFilter", IDS_SETTINGS_PROTANOMALY_FILTER},
      {"tritanomalyFilter", IDS_SETTINGS_TRITANOMALY_FILTER},
      {"deuteranomalyFilter", IDS_SETTINGS_DEUTERANOMALY_FILTER},
      {"colorFilteringLabel", IDS_SETTINGS_COLOR_FILTERING_LABEL},
      {"colorFilteringDescription", IDS_SETTINGS_COLOR_FILTERING_DESCRIPTION},
      {"colorFilteringPreviewInstructions",
       IDS_SETTINGS_COLOR_FILTERING_PREVIEW_INSTRUCTIONS},
      {"colorFilteringPreviewColorRed",
       IDS_SETTINGS_COLOR_FILTERING_PREVIEW_COLOR_RED},
      {"colorFilteringPreviewColorOrange",
       IDS_SETTINGS_COLOR_FILTERING_PREVIEW_COLOR_ORANGE},
      {"colorFilteringPreviewColorYellow",
       IDS_SETTINGS_COLOR_FILTERING_PREVIEW_COLOR_YELLOW},
      {"colorFilteringPreviewColorGreen",
       IDS_SETTINGS_COLOR_FILTERING_PREVIEW_COLOR_GREEN},
      {"colorFilteringPreviewColorCyan",
       IDS_SETTINGS_COLOR_FILTERING_PREVIEW_COLOR_CYAN},
      {"colorFilteringPreviewColorBlue",
       IDS_SETTINGS_COLOR_FILTERING_PREVIEW_COLOR_BLUE},
      {"colorFilteringPreviewColorPurple",
       IDS_SETTINGS_COLOR_FILTERING_PREVIEW_COLOR_PURPLE},
      {"colorFilteringPreviewColorGray",
       IDS_SETTINGS_COLOR_FILTERING_PREVIEW_COLOR_GRAY},
      {"colorVisionDeficiencyTypeLabel",
       IDS_SETTINGS_COLOR_VISION_DEFICIENCY_TYPE_LABEL},
      {"colorVisionFilterIntensityLabel",
       IDS_SETTINGS_COLOR_VISION_FILTER_INTENSITY_LABEL},
      {"reducedAnimationsLabel",
       IDS_SETTINGS_ACCESSIBILITY_REDUCED_ANIMATIONS_LABEL},
      {"reducedAnimationsDescription",
       IDS_SETTINGS_ACCESSIBILITY_REDUCED_ANIMATIONS_DESCRIPTION},
      {"caretBlinkIntervalLabel", IDS_SETTINGS_CARET_BLINK_INTERVAL_LABEL},
      {"caretBlinkIntervalOff", IDS_SETTINGS_CARET_BLINK_INTERVAL_OFF},
      {"caretBlinkIntervalFast", IDS_SETTINGS_CARET_BLINK_INTERVAL_FAST},
      {"faceGazePrevious", IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_PREVIOUS},
      {"faceGazeActionsSectionTitle",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_ACTIONS_SECTION_TITLE},
      {"faceGazeActionsAddAction",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_ACTIONS_ADD_ACTION},
      {"faceGazeActionsAssignGestureLabel",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_ACTIONS_ASSIGN_GESTURE_LABEL},
      {"faceGazeActionsDialogTitle",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_ACTIONS_DIALOG_TITLE},
      {"faceGazeActionsDialogKeyCombinationTitle",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_ACTIONS_DIALOG_KEY_COMBINATION_TITLE},
      {"faceGazeActionsDialogKeyCombinationLabel",
       IDS_SETTINGS_CUSTOMIZE_BUTTONS_DIALOG_DESCRIPTION},
      {"faceGazeActionsDialogSelectGestureTitle",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_ACTIONS_DIALOG_SELECT_GESTURE_TITLE},
      {"faceGazeActionsDialogSelectGestureSubtitle",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_ACTIONS_DIALOG_SELECT_GESTURE_SUBTITLE},
      {"faceGazeActionsDialogGestureThresholdTitle",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_ACTIONS_DIALOG_GESTURE_THRESHOLD_TITLE},
      {"faceGazeActionsDialogGestureThresholdSubtitle",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_ACTIONS_DIALOG_GESTURE_THRESHOLD_SUBTITLE},
      {"faceGazeActionsDialogGestureNotDetectedLabel",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_ACTIONS_DIALOG_GESTURE_NOT_DETECTED_LABEL},
      {"faceGazeActionsDialogGestureDetectedCountOneLabel",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_ACTIONS_DIALOG_GESTURE_DETECTED_COUNT_ONE_LABEL},
      {"faceGazeActionsDialogGestureDetectedCountLabel",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_ACTIONS_DIALOG_GESTURE_DETECTED_COUNT_LABEL},
      {"faceGazeActionsDialogGestureThresholdKnobLabel",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_ACTIONS_DIALOG_GESTURE_THRESHOLD_KNOB_LABEL},
      {"faceGazeMacroLabelToggleFaceGaze",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_MACRO_LABEL_TOGGLE_FACEGAZE},
      {"faceGazeMacroLabelClickLeft",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_MACRO_LABEL_CLICK_LEFT},
      {"faceGazeMacroLabelClickLeftDouble",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_MACRO_LABEL_CLICK_LEFT_DOUBLE},
      {"faceGazeMacroLabelClickRight",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_MACRO_LABEL_CLICK_RIGHT},
      {"faceGazeMacroLabelLongClickLeft",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_MACRO_LABEL_LONG_CLICK_LEFT},
      {"faceGazeMacroLabelResetCursor",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_MACRO_LABEL_RESET_CURSOR},
      {"faceGazeMacroLabelToggleDictation",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_MACRO_LABEL_TOGGLE_DICTATION},
      {"faceGazeMacroLabelToggleOverview",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_MACRO_LABEL_TOGGLE_OVERVIEW},
      {"faceGazeMacroLabelMediaPlayPause",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_MACRO_LABEL_MEDIA_PLAY_PAUSE},
      {"faceGazeMacroLabelToggleScrollMode",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_MACRO_LABEL_TOGGLE_SCROLL_MODE},
      {"faceGazeMacroLabelToggleVirtualKeyboard",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_MACRO_LABEL_TOGGLE_VIRTUAL_KEYBOARD},
      {"faceGazeMacroLabelCustomKeyCombo",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_MACRO_LABEL_CUSTOM_KEY_COMBO},
      {"faceGazeMacroLabelScreenshot",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_MACRO_LABEL_SCREENSHOT},
      {"faceGazeMacroSubLabelToggleScrollMode",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_MACRO_SUB_LABEL_TOGGLE_SCROLL_MODE},
      {"faceGazeKeyboardLabelOneModifier",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_KEYBOARD_LABEL_ONE_MODIFIER},
      {"faceGazeKeyboardLabelTwoModifiers",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_KEYBOARD_LABEL_TWO_MODIFIERS},
      {"faceGazeKeyboardLabelThreeModifiers",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_KEYBOARD_LABEL_THREE_MODIFIERS},
      {"faceGazeKeyboardLabelFourModifiers",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_KEYBOARD_LABEL_FOUR_MODIFIERS},
      {"faceGazeKeyboardKeyCtrl",
       IDS_SETTINGS_PER_DEVICE_KEYBOARD_KEY_LEFT_CTRL},
      {"faceGazeKeyboardKeyAlt", IDS_SETTINGS_PER_DEVICE_KEYBOARD_KEY_LEFT_ALT},
      {"faceGazeKeyboardKeyShift",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_KEYBOARD_KEY_SHIFT},
      {"faceGazeKeyboardKeySearch",
       IDS_SETTINGS_PER_DEVICE_KEYBOARD_KEY_SEARCH},
      {"faceGazeGestureLabelBrowInnerUp",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_GESTURE_LABEL_BROW_INNER_UP},
      {"faceGazeGestureLabelBrowsDown",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_GESTURE_LABEL_BROWS_DOWN},
      {"faceGazeGestureLabelEyeSquintLeft",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_GESTURE_LABEL_EYE_SQUINT_LEFT},
      {"faceGazeGestureLabelEyeSquintRight",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_GESTURE_LABEL_EYE_SQUINT_RIGHT},
      {"faceGazeGestureLabelEyesBlink",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_GESTURE_LABEL_EYES_BLINK},
      {"faceGazeGestureLabelEyesLookDown",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_GESTURE_LABEL_EYES_LOOK_DOWN},
      {"faceGazeGestureLabelEyesLookLeft",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_GESTURE_LABEL_EYES_LOOK_LEFT},
      {"faceGazeGestureLabelEyesLookRight",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_GESTURE_LABEL_EYES_LOOK_RIGHT},
      {"faceGazeGestureLabelEyesLookUp",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_GESTURE_LABEL_EYES_LOOK_UP},
      {"faceGazeGestureLabelJawLeft",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_GESTURE_LABEL_JAW_LEFT},
      {"faceGazeGestureLabelJawOpen",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_GESTURE_LABEL_JAW_OPEN},
      {"faceGazeGestureLabelJawRight",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_GESTURE_LABEL_JAW_RIGHT},
      {"faceGazeGestureLabelMouthFunnel",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_GESTURE_LABEL_MOUTH_FUNNEL},
      {"faceGazeGestureLabelMouthLeft",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_GESTURE_LABEL_MOUTH_LEFT},
      {"faceGazeGestureLabelMouthPucker",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_GESTURE_LABEL_MOUTH_PUCKER},
      {"faceGazeGestureLabelMouthRight",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_GESTURE_LABEL_MOUTH_RIGHT},
      {"faceGazeGestureLabelMouthSmile",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_GESTURE_LABEL_MOUTH_SMILE},
      {"faceGazeGestureLabelMouthUpperUp",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_GESTURE_LABEL_MOUTH_UPPER_UP},
      {"faceGazeCursorAccelerationLabel",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_CURSOR_ACCELERATION_LABEL},
      {"faceGazeCursorAccelerationDescription",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_CURSOR_ACCELERATION_DESCRIPTION},
      {"faceGazeCursorSmoothingLabel",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_CURSOR_SMOOTHING_LABEL},
      {"faceGazeCursorSmoothingDescription",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_CURSOR_SMOOTHING_DESCRIPTION},
      {"faceGazeCursorDownSpeedLabel",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_CURSOR_SPEED_DOWN_LABEL},
      {"faceGazeCursorLeftSpeedLabel",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_CURSOR_SPEED_LEFT_LABEL},
      {"faceGazeCursorRightSpeedLabel",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_CURSOR_SPEED_RIGHT_LABEL},
      {"faceGazeCursorUpSpeedLabel",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_CURSOR_SPEED_UP_LABEL},
      {"faceGazeActionsEnabledLabel",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_ACTIONS_ENABLED_LABEL},
      {"faceGazeCursorControlEnabledLabel",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_CURSOR_CONTROL_ENABLED_LABEL},
      {"faceGazeCursorAdjustSeparatelyLabel",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_ADJUST_SEPARATELY_LABEL},
      {"faceGazeCursorSpeedLabel",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_CURSOR_SPEED_LABEL},
      {"faceGazeCursorSpeedSlow",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_CURSOR_SPEED_SLOW},
      {"faceGazeCursorSpeedFast",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_CURSOR_SPEED_FAST},
      {"faceGazeCursorSliderLabelResponsive",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_CURSOR_SLIDER_LABEL_RESPONSIVE},
      {"faceGazeCursorSliderLabelSmooth",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_CURSOR_SLIDER_LABEL_SMOOTH},
      {"faceGazeCursorSpeedSectionName",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_CURSOR_SPEED_SECTION_NAME},
      {"faceGazeCursorSettingsReset",
       IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_CURSOR_SETTINGS_RESET},
      {"flashNotificationsLabel", IDS_SETTINGS_FLASH_NOTIFICATIONS_LABEL},
      {"flashNotificationsDescription",
       IDS_SETTINGS_FLASH_NOTIFICATIONS_DESCRIPTION},
      {"flashNotificationsColorOptionsLabel",
       IDS_SETTINGS_FLASH_NOTIFICATIONS_COLOR_OPTIONS_LABEL},
      {"flashNotificationsPreviewButton",
       IDS_SETTINGS_FLASH_NOTIFICATIONS_PREVIEW_BUTTON},
      {"flashNotificationsPreviewButtonLabel",
       IDS_SETTINGS_FLASH_NOTIFICATIONS_PREVIEW_BUTTON_LABEL},
      {"keyboardAndTextInputHeading",
       IDS_SETTINGS_ACCESSIBILITY_KEYBOARD_AND_TEXT_INPUT_HEADING},
      {"keyboardAndTextInputLinkDescription",
       IDS_SETTINGS_ACCESSIBILITY_KEYBOARD_AND_TEXT_INPUT_LINK_DESCRIPTION},
      {"keyboardAndTextInputLinkTitle",
       IDS_SETTINGS_ACCESSIBILITY_KEYBOARD_AND_TEXT_INPUT_LINK_TITLE},
      {"keyboardSettingsDescription",
       IDS_SETTINGS_ACCESSIBILITY_KEYBOARD_SETTINGS_DESCRIPTION},
      {"keyboardSettingsTitle",
       IDS_SETTINGS_ACCESSIBILITY_KEYBOARD_SETTINGS_TITLE},
      {"largeMouseCursorLabel", IDS_SETTINGS_LARGE_MOUSE_CURSOR_LABEL},
      {"largeMouseCursorSizeDefaultLabel",
       IDS_SETTINGS_LARGE_MOUSE_CURSOR_SIZE_DEFAULT_LABEL},
      {"largeMouseCursorSizeLabel", IDS_SETTINGS_LARGE_MOUSE_CURSOR_SIZE_LABEL},
      {"largeMouseCursorSizeLargeLabel",
       IDS_SETTINGS_LARGE_MOUSE_CURSOR_SIZE_LARGE_LABEL},
      {"manageAccessibilityFeatures",
       IDS_SETTINGS_ACCESSIBILITY_MANAGE_ACCESSIBILITY_FEATURES},
      {"manageSwitchAccessSettings",
       IDS_SETTINGS_MANAGE_SWITCH_ACCESS_SETTINGS},
      {"manageTtsSettings", IDS_SETTINGS_MANAGE_TTS_SETTINGS},
      {"monoAudioDescription", IDS_SETTINGS_MONO_AUDIO_DESCRIPTION},
      {"monoAudioLabel", IDS_SETTINGS_MONO_AUDIO_LABEL},
      {"moreFeaturesLinkDescription",
       IDS_SETTINGS_MORE_FEATURES_LINK_DESCRIPTION},
      {"mouseAndTouchpadHeading",
       IDS_SETTINGS_ACCESSIBILITY_MOUSE_AND_TOUCHPAD_HEADING},
      {"mouseSettingsTitle", IDS_SETTINGS_ACCESSIBILITY_MOUSE_SETTINGS_TITLE},
      {"noSwitchesAssigned", IDS_SETTINGS_NO_SWITCHES_ASSIGNED},
      {"noSwitchesAssignedSetupGuide",
       IDS_SETTINGS_NO_SWITCHES_ASSIGNED_SETUP_GUIDE},
      {"onScreenKeyboardDescription",
       IDS_SETTINGS_ON_SCREEN_KEYBOARD_DESCRIPTION},
      {"onScreenKeyboardLabel", IDS_SETTINGS_ON_SCREEN_KEYBOARD_LABEL},
      {"optionsInMenuDescription", IDS_SETTINGS_OPTIONS_IN_MENU_DESCRIPTION},
      {"optionsInMenuLabel", IDS_SETTINGS_OPTIONS_IN_MENU_LABEL},
      {"percentage", IDS_SETTINGS_PERCENTAGE},
      {"screenMagnifierDescriptionOff",
       IDS_SETTINGS_SCREEN_MAGNIFIER_DESCRIPTION_OFF},
      {"screenMagnifierDescriptionOn",
       IDS_SETTINGS_SCREEN_MAGNIFIER_DESCRIPTION_ON},
      {"screenMagnifierFocusFollowingLabel",
       IDS_SETTINGS_SCREEN_MAGNIFIER_FOCUS_FOLLOWING_LABEL},
      {"screenMagnifierSelectToSpeakFocusFollowingLabel",
       IDS_SETTINGS_SCREEN_MAGNIFIER_SELECT_TO_SPEAK_FOCUS_FOLLOWING_LABEL},
      {"screenMagnifierChromeVoxFocusFollowingLabel",
       IDS_SETTINGS_SCREEN_MAGNIFIER_CHROMEVOX_FOCUS_FOLLOWING_LABEL},
      {"screenMagnifierLabel", IDS_SETTINGS_SCREEN_MAGNIFIER_LABEL},
      {"screenMagnifierMouseFollowingModeCentered",
       IDS_SETTINGS_SCREEN_MANIFIER_MOUSE_FOLLOWING_MODE_CENTERED},
      {"screenMagnifierMouseFollowingModeContinuous",
       IDS_SETTINGS_SCREEN_MANIFIER_MOUSE_FOLLOWING_MODE_CONTINUOUS},
      {"screenMagnifierMouseFollowingModeEdge",
       IDS_SETTINGS_SCREEN_MANIFIER_MOUSE_FOLLOWING_MODE_EDGE},
      {"screenMagnifierZoom10x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_10_X},
      {"screenMagnifierZoom12x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_12_X},
      {"screenMagnifierZoom14x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_14_X},
      {"screenMagnifierZoom16x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_16_X},
      {"screenMagnifierZoom18x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_18_X},
      {"screenMagnifierZoom20x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_20_X},
      {"screenMagnifierZoom2x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_2_X},
      {"screenMagnifierZoom4x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_4_X},
      {"screenMagnifierZoom6x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_6_X},
      {"screenMagnifierZoom8x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_8_X},
      {"screenMagnifierZoomHintLabel",
       IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_HINT_LABEL},
      {"screenMagnifierZoomLabel", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_LABEL},
      {"selectToSpeakDescription",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_DESCRIPTION},
      {"selectToSpeakDescriptionWithoutKeyboard",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_DESCRIPTION_WITHOUT_KEYBOARD},
      {"selectToSpeakDisabledDescription",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_DISABLED_DESCRIPTION},
      {"selectToSpeakLinkTitle",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_LINK_TITLE},
      {"selectToSpeakOptionsLanguagesFilterDescription",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_LANGUAGES_FILTER_DESCRIPTION},
      {"selectToSpeakOptionsVoiceDescription",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_VOICE_DESCRIPTION},
      {"selectToSpeakOptionsVoiceSwitchingDescription",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_VOICE_SWITCHING_DESCRIPTION},
      {"selectToSpeakOptionsEnhancedNetworkVoicesDescription",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_ENHANCED_NETWORK_VOICES_DESCRIPTION},
      {"selectToSpeakOptionsEnhancedNetworkVoicesSubtitle",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_ENHANCED_NETWORK_VOICES_SUBTITLE},
      {"selectToSpeakOptionsEnhancedNetworkVoice",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_ENHANCED_NETWORK_VOICE},
      {"selectToSpeakOptionsNaturalVoiceName",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_NATURAL_VOICE_NAME},
      {"selectToSpeakOptionsHighlightDescription",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_HIGHLIGHT_DESCRIPTION},
      {"selectToSpeakOptionsHighlightColorDescription",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_HIGHLIGHT_COLOR_DESCRIPTION},
      {"selectToSpeakOptionsHighlightColorBlue",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_HIGHLIGHT_COLOR_BLUE},
      {"selectToSpeakOptionsHighlightColorOrange",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_HIGHLIGHT_COLOR_ORANGE},
      {"selectToSpeakOptionsHighlightColorYellow",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_HIGHLIGHT_COLOR_YELLOW},
      {"selectToSpeakOptionsHighlightColorGreen",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_HIGHLIGHT_COLOR_GREEN},
      {"selectToSpeakOptionsHighlightColorPink",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_HIGHLIGHT_COLOR_PINK},
      {"selectToSpeakOptionsHighlightDark",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_HIGHLIGHT_DARK},
      {"selectToSpeakOptionsHighlightLight",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_HIGHLIGHT_LIGHT},
      {"selectToSpeakOptionsBackgroundShadingDescription",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_BACKGROUND_SHADING_DESCRIPTION},
      {"selectToSpeakOptionsSampleText",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_SAMPLE_TEXT},
      {"selectToSpeakOptionsNavigationControlsDescription",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_NAVIGATION_CONTROLS_DESCRIPTION},
      {"selectToSpeakOptionsNavigationControlsSubtitle",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_NAVIGATION_CONTROLS_SUBTITLE},
      {"selectToSpeakTextToSpeechSettingsLink",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_TEXT_TO_SPEECH_SETTINGS_LINK},
      {"selectToSpeakOptionsHighlight",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_HIGHLIGHT},
      {"selectToSpeakOptionsSpeech",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_SPEECH},
      {"selectToSpeakOptionsDeviceLanguage",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_DEVICE_LANGUAGE},
      {"selectToSpeakOptionsSystemVoice",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_SYSTEM_VOICE},
      {"selectToSpeakOptionsVoicePreview",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_VOICE_PREVIEW},
      {"selectToSpeakOptionsDefaultNetworkVoice",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_DEFAULT_NETWORK_VOICE},
      {"selectToSpeakOptionsNaturalVoicePreview",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_NATURAL_VOICE_PREVIEW},
      {"selectToSpeakOptionsLabel",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_OPTIONS_LABEL},
      {"selectToSpeakTitle", IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_TITLE},
      {"settingsSliderRoleDescription",
       IDS_SETTINGS_SLIDER_MIN_MAX_ARIA_ROLE_DESCRIPTION},
      {"sliderLabel1", IDS_OS_SETTINGS_SLIDER_LABEL_1},
      {"sliderLabel100", IDS_OS_SETTINGS_SLIDER_LABEL_100},
      {"startupSoundLabel", IDS_SETTINGS_STARTUP_SOUND_LABEL},
      {"stickyKeysDescription", IDS_SETTINGS_STICKY_KEYS_DESCRIPTION},
      {"stickyKeysLabel", IDS_SETTINGS_STICKY_KEYS_LABEL},
      {"stickyKeysDisabledByChromevoxTooltip",
       IDS_SETTINGS_STICKY_KEYS_DISABLED_BY_CHROMEVOX_TOOLTIP},
      {"switchAccessActionAssignmentAddAssignmentIconLabel",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_ADD_ASSIGNMENT_ICON_LABEL},
      {"switchAccessActionAssignmentAssignedIconLabel",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_ASSIGNED_ICON_LABEL},
      {"switchAccessActionAssignmentContinueResponse",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_CONTINUE_RESPONSE},
      {"switchAccessActionAssignmentDialogTitle",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_DIALOG_TITLE},
      {"switchAccessActionAssignmentErrorIconLabel",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_ERROR_ICON_LABEL},
      {"switchAccessActionAssignmentExitResponse",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_EXIT_RESPONSE},
      {"switchAccessActionAssignmentRemoveAssignmentIconLabel",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_REMOVE_ASSIGNMENT_ICON_LABEL},
      {"switchAccessActionAssignmentTryAgainResponse",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_TRY_AGAIN_RESPONSE},
      {"switchAccessActionAssignmentWaitForConfirmationPrompt",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_WAIT_FOR_CONFIRMATION_PROMPT},
      {"switchAccessActionAssignmentWaitForConfirmationRemovalPrompt",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_WAIT_FOR_CONFIRMATION_REMOVAL_PROMPT},
      {"switchAccessActionAssignmentWaitForKeyPromptAtLeastOneSwitch",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_WAIT_FOR_KEY_PROMPT_AT_LEAST_ONE_SWITCH},
      {"switchAccessActionAssignmentWaitForKeyPromptNoSwitches",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_WAIT_FOR_KEY_PROMPT_NO_SWITCHES},
      {"switchAccessActionAssignmentWaitForKeyPromptNoSwitchesSetupGuide",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_WAIT_FOR_KEY_PROMPT_NO_SWITCHES_SETUP_GUIDE},
      {"switchAccessActionAssignmentWarnAlreadyAssignedActionPrompt",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_WARN_ALREADY_ASSIGNED_ACTION_PROMPT},
      {"switchAccessActionAssignmentWarnCannotRemoveLastSelectSwitch",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_WARN_CANNOT_REMOVE_LAST_SELECT_SWITCH},
      {"switchAccessActionAssignmentWarnNotConfirmedPrompt",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_WARN_NOT_CONFIRMED_PROMPT},
      {"switchAccessActionAssignmentWarnUnrecognizedKeyPrompt",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_WARN_UNRECOGNIZED_KEY_PROMPT},
      {"switchAccessAutoScanHeading",
       IDS_SETTINGS_SWITCH_ACCESS_AUTO_SCAN_HEADING},
      {"switchAccessAutoScanKeyboardSpeedLabel",
       IDS_SETTINGS_SWITCH_ACCESS_AUTO_SCAN_KEYBOARD_SPEED_LABEL},
      {"switchAccessAutoScanLabel", IDS_SETTINGS_SWITCH_ACCESS_AUTO_SCAN_LABEL},
      {"switchAccessAutoScanSpeedLabel",
       IDS_SETTINGS_SWITCH_ACCESS_AUTO_SCAN_SPEED_LABEL},
      {"switchAccessBluetoothDeviceTypeLabel",
       IDS_SETTINGS_SWITCH_ACCESS_BLUETOOTH_DEVICE_TYPE_LABEL},
      {"switchAccessDialogExit", IDS_SETTINGS_SWITCH_ACCESS_DIALOG_EXIT},
      {"switchAccessInternalDeviceTypeLabel",
       IDS_SETTINGS_SWITCH_ACCESS_INTERNAL_DEVICE_TYPE_LABEL},
      {"switchAccessLabel",
       IDS_SETTINGS_ACCESSIBILITY_SWITCH_ACCESS_DESCRIPTION},
      {"switchAccessLabelSubtext",
       IDS_SETTINGS_ACCESSIBILITY_SWITCH_ACCESS_DESCRIPTION_SUBTEXT},
      {"switchAccessOptionsLabel",
       IDS_SETTINGS_ACCESSIBILITY_SWITCH_ACCESS_OPTIONS_LABEL},
      {"switchAccessPointScanSpeedLabel",
       IDS_SETTINGS_SWITCH_ACCESS_POINT_SCAN_SPEED_LABEL},
      {"switchAccessSetupAssignNextTitle",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_ASSIGN_NEXT_TITLE},
      {"switchAccessSetupAssignPreviousTitle",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_ASSIGN_PREVIOUS_TITLE},
      {"switchAccessSetupAssignSelectTitle",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_ASSIGN_SELECT_TITLE},
      {"switchAccessSetupAutoScanEnabledDirections",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_AUTO_SCAN_ENABLED_DIRECTIONS},
      {"switchAccessSetupAutoScanEnabledExplanation",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_AUTO_SCAN_ENABLED_EXPLANATION},
      {"switchAccessSetupAutoScanEnabledTitle",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_AUTO_SCAN_ENABLED_TITLE},
      {"switchAccessSetupAutoScanFaster",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_AUTO_SCAN_FASTER},
      {"switchAccessSetupAutoScanSlower",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_AUTO_SCAN_SLOWER},
      {"switchAccessSetupAutoScanSpeedDescription",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_AUTO_SCAN_SPEED_DESCRIPTION},
      {"switchAccessSetupAutoScanSpeedTitle",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_AUTO_SCAN_SPEED_TITLE},
      {"switchAccessSetupChoose1Switch",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_CHOOSE_1_SWITCH},
      {"switchAccessSetupChoose2Switches",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_CHOOSE_2_SWITCHES},
      {"switchAccessSetupChoose2SwitchesDescription",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_CHOOSE_2_SWITCHES_DESCRIPTION},
      {"switchAccessSetupChoose3Switches",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_CHOOSE_3_SWITCHES},
      {"switchAccessSetupChoose3SwitchesDescription",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_CHOOSE_3_SWITCHES_DESCRIPTION},
      {"switchAccessSetupChooseSwitchCountTitle",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_CHOOSE_SWITCH_COUNT_TITLE},
      {"switchAccessSetupClosingInfo",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_CLOSING_INFO},
      {"switchAccessSetupClosingManualScanInstructions",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_CLOSING_MANUAL_SCAN_INSTRUCTIONS},
      {"switchAccessSetupClosingTitle",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_CLOSING_TITLE},
      {"switchAccessSetupDone", IDS_SETTINGS_SWITCH_ACCESS_SETUP_DONE},
      {"switchAccessSetupGuideLabel",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_GUIDE_LABEL},
      {"switchAccessSetupGuideWarningDialogMessage",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_GUIDE_WARNING_DIALOG_MESSAGE},
      {"switchAccessSetupGuideWarningDialogTitle",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_GUIDE_WARNING_DIALOG_TITLE},
      {"switchAccessSetupIntroBody",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_INTRO_BODY},
      {"switchAccessSetupIntroTitle",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_INTRO_TITLE},
      {"switchAccessSetupNext", IDS_SETTINGS_SWITCH_ACCESS_SETUP_NEXT},
      {"switchAccessSetupPairBluetooth",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_PAIR_BLUETOOTH},
      {"switchAccessSetupPrevious", IDS_SETTINGS_SWITCH_ACCESS_SETUP_PREVIOUS},
      {"switchAccessSetupStartOver",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_START_OVER},
      {"switchAccessUnknownDeviceTypeLabel",
       IDS_SETTINGS_SWITCH_ACCESS_UNKNOWN_DEVICE_TYPE_LABEL},
      {"switchAccessUsbDeviceTypeLabel",
       IDS_SETTINGS_SWITCH_ACCESS_USB_DEVICE_TYPE_LABEL},
      {"switchAndDeviceType", IDS_SETTINGS_SWITCH_AND_DEVICE_TYPE},
      {"switchAssignmentHeading", IDS_SETTINGS_SWITCH_ASSIGNMENT_HEADING},
      {"tabletModeShelfNavigationButtonsSettingDescription",
       IDS_SETTINGS_A11Y_TABLET_MODE_SHELF_BUTTONS_DESCRIPTION},
      {"tabletModeShelfNavigationButtonsSettingLabel",
       IDS_SETTINGS_A11Y_TABLET_MODE_SHELF_BUTTONS_LABEL},
      {"tapDraggingLabel", kIsRevampEnabled
                               ? IDS_OS_SETTINGS_REVAMP_TAP_DRAGGING_LABEL
                               : IDS_SETTINGS_TAP_DRAGGING_LABEL},
      {"tapDraggingDescription",
       IDS_OS_SETTINGS_REVAMP_TAP_DRAGGING_DESCRIPTION},
      {"textToSpeechEngines", IDS_SETTINGS_TEXT_TO_SPEECH_ENGINES},
      {"textToSpeechHeading",
       IDS_SETTINGS_ACCESSIBILITY_TEXT_TO_SPEECH_HEADING},
      {"textToSpeechLinkDescription",
       IDS_SETTINGS_ACCESSIBILITY_TEXT_TO_SPEECH_LINK_DESCRIPTION},
      {"textToSpeechLinkTitle",
       IDS_SETTINGS_ACCESSIBILITY_TEXT_TO_SPEECH_LINK_TITLE},
      {"textToSpeechMoreLanguages", IDS_SETTINGS_TEXT_TO_SPEECH_MORE_LANGUAGES},
      {"textToSpeechNoVoicesMessage",
       IDS_SETTINGS_TEXT_TO_SPEECH_NO_VOICES_MESSAGE},
      {"textToSpeechPitch", IDS_SETTINGS_TEXT_TO_SPEECH_PITCH},
      {"textToSpeechPitchMaximumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_PITCH_MAXIMUM_LABEL},
      {"textToSpeechPitchMinimumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_PITCH_MINIMUM_LABEL},
      {"textToSpeechPreviewHeading",
       IDS_SETTINGS_TEXT_TO_SPEECH_PREVIEW_HEADING},
      {"textToSpeechPreviewInput", IDS_SETTINGS_TEXT_TO_SPEECH_PREVIEW_INPUT},
      {"textToSpeechPreviewInputLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_PREVIEW_INPUT_LABEL},
      {"textToSpeechPreviewPlay", IDS_SETTINGS_TEXT_TO_SPEECH_PREVIEW_PLAY},
      {"textToSpeechPreviewVoice", IDS_SETTINGS_TEXT_TO_SPEECH_PREVIEW_VOICE},
      {"textToSpeechProperties", IDS_SETTINGS_TEXT_TO_SPEECH_PROPERTIES},
      {"textToSpeechRate", IDS_SETTINGS_TEXT_TO_SPEECH_RATE},
      {"textToSpeechRateMaximumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_RATE_MAXIMUM_LABEL},
      {"textToSpeechRateMinimumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_RATE_MINIMUM_LABEL},
      {"textToSpeechVoices", IDS_SETTINGS_TEXT_TO_SPEECH_VOICES},
      {"textToSpeechVolume", IDS_SETTINGS_TEXT_TO_SPEECH_VOLUME},
      {"textToSpeechVolumeMaximumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_VOLUME_MAXIMUM_LABEL},
      {"textToSpeechVolumeMinimumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_VOLUME_MINIMUM_LABEL},
      {"ttsSettingsLinkDescription", IDS_SETTINGS_TTS_LINK_DESCRIPTION},
      {"overscrollHistoryNavigationTitle",
       IDS_SETTINGS_OVERSCROLL_HISTORY_NAVIGATION_TITLE},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString("a11yLearnMoreUrl",
                         chrome::kChromeAccessibilityHelpURL);

  html_source->AddString("selectToSpeakLearnMoreUrl",
                         chrome::kSelectToSpeakLearnMoreURL);

  html_source->AddString(
      "displayAndMagnificationLinkDescription",
      l10n_util::GetStringUTF16(
          GetDisplayAndMangificationLinkDescriptionResourceId()));

  html_source->AddInteger("defaultCaretBlinkIntervalMs",
                          ash::kDefaultCaretBlinkIntervalMs);

  html_source->AddInteger("defaultFaceGazeCursorSpeed",
                          ash::kDefaultFaceGazeCursorSpeed);
  html_source->AddInteger("defaultFaceGazeCursorSmoothing",
                          ash::kDefaultFaceGazeCursorSmoothing);
  html_source->AddBoolean("defaultFaceGazeCursorUseAcceleration",
                          ash::kDefaultFaceGazeCursorUseAcceleration);
  html_source->AddInteger("defaultFaceGazeVelocityThreshold",
                          ash::kDefaultFaceGazeVelocityThreshold);

  html_source->AddBoolean(
      "showExperimentalAccessibilitySwitchAccessImprovedTextInput",
      IsSwitchAccessTextAllowed());

  html_source->AddBoolean("showTabletModeShelfNavigationButtonsSettings",
                          AreTabletNavigationButtonsAllowed());

  html_source->AddString("tabletModeShelfNavigationButtonsLearnMoreUrl",
                         chrome::kTabletModeGesturesLearnMoreURL);

  html_source->AddBoolean("isAccessibilityReducedAnimationsEnabled",
                          IsAccessibilityReducedAnimationsEnabled());

  html_source->AddBoolean("isAccessibilityMagnifierFollowsChromeVoxEnabled",
                          IsAccessibilityMagnifierFollowsChromeVoxEnabled());

  html_source->AddBoolean("isAccessibilityMagnifierFollowsStsEnabled",
                          IsAccessibilityMagnifierFollowsStsEnabled());

  html_source->AddBoolean("isAccessibilityFaceGazeEnabled",
                          IsAccessibilityFaceGazeEnabled());

  html_source->AddBoolean("isAccessibilityDisableTrackpadEnabled",
                          IsAccessibilityDisableTrackpadEnabled());

  html_source->AddBoolean("isAccessibilityMouseKeysEnabled",
                          IsAccessibilityMouseKeysEnabled());

  html_source->AddBoolean(
      "isAccessibilityCaretBlinkIntervalSettingEnabled",
      ::features::IsAccessibilityCaretBlinkIntervalSettingEnabled());

  html_source->AddBoolean("isAccessibilityOverscrollSettingFeatureEnabled",
                          IsAccessibilityOverscrollSettingFeatureEnabled());

  html_source->AddBoolean("isAccessibilityFlashNotificationFeatureEnabled",
                          IsAccessibilityFlashNotificationFeatureEnabled());

  ::settings::AddAxAnnotationsSectionStrings(html_source);
  ::settings::AddCaptionSubpageStrings(html_source);
}

void AccessibilitySection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<::settings::AccessibilityMainHandler>());
  web_ui->AddMessageHandler(std::make_unique<AccessibilityHandler>(profile()));
  web_ui->AddMessageHandler(
      std::make_unique<SwitchAccessHandler>(profile()->GetPrefs()));
  web_ui->AddMessageHandler(std::make_unique<TtsHandler>());
  web_ui->AddMessageHandler(std::make_unique<SelectToSpeakHandler>());
  web_ui->AddMessageHandler(
      std::make_unique<::settings::FontHandler>(profile()));
  web_ui->AddMessageHandler(
      std::make_unique<::settings::CaptionsHandler>(profile()->GetPrefs()));
  web_ui->AddMessageHandler(std::make_unique<FaceGazeSettingsHandler>());
}

int AccessibilitySection::GetSectionNameMessageId() const {
  return IDS_SETTINGS_ACCESSIBILITY;
}

mojom::Section AccessibilitySection::GetSection() const {
  return mojom::Section::kAccessibility;
}

mojom::SearchResultIcon AccessibilitySection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kA11y;
}

const char* AccessibilitySection::GetSectionPath() const {
  return mojom::kAccessibilitySectionPath;
}

bool AccessibilitySection::LogMetric(mojom::Setting setting,
                                     base::Value& value) const {
  // TODO(accessibility): Ensure to capture metrics for Switch Access's action
  // dialog on detach.
  switch (setting) {
    case mojom::Setting::kFullscreenMagnifierFocusFollowing:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Accessibility.FullscreenMagnifierFocusFollowing",
          value.GetBool());
      return true;
    case mojom::Setting::kAccessibilityMagnifierFollowsSts:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Accessibility."
          "FullscreenMagnifierSelectToSpeakFollowing",
          value.GetBool());
      return true;
    case mojom::Setting::kMagnifierFollowsChromeVox:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Accessibility.MagnifierFollowsChromeVox",
          value.GetBool());
      return true;
    case mojom::Setting::kFullscreenMagnifierMouseFollowingMode:
      base::UmaHistogramEnumeration(
          "ChromeOS.Settings.Accessibility."
          "FullscreenMagnifierMouseFollowingMode",
          static_cast<MagnifierMouseFollowingMode>(value.GetInt()));
      return true;
    case mojom::Setting::kColorCorrectionEnabled:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Accessibility.ColorCorrection.Enabled",
          value.GetBool());
      return true;
    case mojom::Setting::kColorCorrectionFilterType:
      base::UmaHistogramEnumeration(
          "ChromeOS.Settings.Accessibility.ColorCorrection.FilterType",
          static_cast<ColorVisionCorrectionType>(value.GetInt()));
      return true;
    case mojom::Setting::kColorCorrectionFilterAmount:
      base::UmaHistogramPercentage(
          "ChromeOS.Settings.Accessibility.ColorCorrection.FilterAmount",
          value.GetInt());
      return true;
    case mojom::Setting::kCaretBlinkInterval:
      base::UmaHistogramSparse(
          "ChromeOS.Settings.Accessibility.CaretBlinkInterval", value.GetInt());
      return true;
    case mojom::Setting::kLiveCaption:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Accessibility.LiveCaption.Enabled",
          value.GetBool());
      return true;
    case mojom::Setting::kMainNodeAnnotationsEnabled:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Accessibility.MainNodeAnnotations.Enabled",
          value.GetBool());
      return true;
    case mojom::Setting::kMonoAudio:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Accessibility.MonoAudio.Enabled", value.GetBool());
      return true;
    case mojom::Setting::kAutoClickWhenCursorStops:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Accessibility.Autoclick.Enabled", value.GetBool());
      return true;
    case mojom::Setting::kDictation:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Accessibility.Dictation.Enabled", value.GetBool());
      return true;
    case mojom::Setting::kOnScreenKeyboard:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Accessibility.OnScreenKeyboard.Enabled",
          value.GetBool());
      return true;
    case mojom::Setting::kStickyKeys:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Accessibility.StickyKeys.Enabled",
          value.GetBool());
      return true;
    case mojom::Setting::kEnableSwitchAccess:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Accessibility.SwitchAccess.Enabled",
          value.GetBool());
      return true;
    case mojom::Setting::kLargeCursor:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Accessibility.LargeCursor.Enabled",
          value.GetBool());
      return true;
    case mojom::Setting::kEnableCursorColor:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Accessibility.CursorColor.Enabled",
          value.GetBool());
      return true;
    case mojom::Setting::kHighlightCursorWhileMoving:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Accessibility.CursorHighlight.Enabled",
          value.GetBool());
      return true;
    case mojom::Setting::kHighlightTextCaret:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Accessibility.CaretHighlight.Enabled",
          value.GetBool());
      return true;
    case mojom::Setting::kCaretBrowsing:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Accessibility.CaretBrowsing.Enabled",
          value.GetBool());
      return true;
    case mojom::Setting::kHighContrastMode:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Accessibility.HighContrast.Enabled",
          value.GetBool());
      return true;
    case mojom::Setting::kFullscreenMagnifier:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Accessibility.FullscreenMagnifier.Enabled",
          value.GetBool());
      return true;
    case mojom::Setting::kDockedMagnifier:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Accessibility.DockedMagnifier.Enabled",
          value.GetBool());
      return true;
    case mojom::Setting::kHighlightKeyboardFocus:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Accessibility.FocusHighlight.Enabled",
          value.GetBool());
      return true;
    case mojom::Setting::kChromeVox:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Accessibility.SpokenFeedback.Enabled",
          value.GetBool());
      return true;
    case mojom::Setting::kSelectToSpeak:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Accessibility.SelectToSpeak.Enabled",
          value.GetBool());
      return true;
    case mojom::Setting::kReducedAnimationsEnabled:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.Accessibility.ReducedAnimations.Enabled",
          value.GetBool());
      return true;
    case mojom::Setting::kOverscrollEnabled:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.OverscrollHistoryNavigation.Enabled",
          value.GetBool());
      return true;
    case mojom::Setting::kFlashNotifications:
      base::UmaHistogramBoolean("ChromeOS.Settings.FlashNotifications.Enabled",
                                value.GetBool());
      return true;
    default:
      return false;
  }
}

void AccessibilitySection::RegisterHierarchy(
    HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSetting(mojom::Setting::kA11yQuickSettings);
  generator->RegisterTopLevelSetting(
      mojom::Setting::kGetImageDescriptionsFromGoogle);

  // Manage accessibility.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_ACCESSIBILITY_MANAGE_ACCESSIBILITY_FEATURES,
      mojom::Subpage::kManageAccessibility, mojom::SearchResultIcon::kA11y,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kManageAccessibilitySubpagePath);

  // Text-to-Speech page.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_ACCESSIBILITY_TEXT_TO_SPEECH_LINK_TITLE,
      mojom::Subpage::kTextToSpeechPage, mojom::SearchResultIcon::kA11y,
      mojom::SearchResultDefaultRank::kMedium, mojom::kTextToSpeechPagePath);
  // ChromeVox settings page.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_CHROMEVOX_OPTIONS_LABEL, mojom::Subpage::kChromeVox,
      mojom::SearchResultIcon::kA11y, mojom::SearchResultDefaultRank::kMedium,
      mojom::kChromeVoxSubpagePath);
  // Select to speak options page.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_LINK_TITLE,
      mojom::Subpage::kSelectToSpeak, mojom::SearchResultIcon::kA11y,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kSelectToSpeakSubpagePath);
  static constexpr mojom::Setting kSelectToSpeakSettings[] = {
      mojom::Setting::kSelectToSpeakWordHighlight,
      mojom::Setting::kSelectToSpeakBackgroundShading,
      mojom::Setting::kSelectToSpeakNavigationControls,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kSelectToSpeak,
                            kSelectToSpeakSettings, generator);
  // Display and magnification page.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_ACCESSIBILITY_DISPLAY_AND_MAGNIFICATION_LINK_TITLE,
      mojom::Subpage::kDisplayAndMagnification, mojom::SearchResultIcon::kA11y,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kDisplayAndMagnificationSubpagePath);
  // Keyboard and text input page.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_ACCESSIBILITY_KEYBOARD_AND_TEXT_INPUT_LINK_TITLE,
      mojom::Subpage::kKeyboardAndTextInput, mojom::SearchResultIcon::kA11y,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kKeyboardAndTextInputSubpagePath);
  // Cursor and touchpad page.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_ACCESSIBILITY_CURSOR_AND_TOUCHPAD_LINK_TITLE,
      mojom::Subpage::kCursorAndTouchpad, mojom::SearchResultIcon::kA11y,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kCursorAndTouchpadSubpagePath);
  // Audio and captions page.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_ACCESSIBILITY_AUDIO_AND_CAPTIONS_LINK_TITLE,
      mojom::Subpage::kAudioAndCaptions, mojom::SearchResultIcon::kA11y,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kAudioAndCaptionsSubpagePath);

  static constexpr mojom::Setting kManageAccessibilitySettings[] = {
      mojom::Setting::kChromeVox,
      mojom::Setting::kSelectToSpeak,
      mojom::Setting::kHighContrastMode,
      mojom::Setting::kFullscreenMagnifier,
      mojom::Setting::kFullscreenMagnifierFocusFollowing,
      mojom::Setting::kAccessibilityMagnifierFollowsSts,
      mojom::Setting::kMagnifierFollowsChromeVox,
      mojom::Setting::kFullscreenMagnifierMouseFollowingMode,
      mojom::Setting::kDockedMagnifier,
      mojom::Setting::kStickyKeys,
      mojom::Setting::kOnScreenKeyboard,
      mojom::Setting::kDictation,
      mojom::Setting::kHighlightKeyboardFocus,
      mojom::Setting::kEnableSwitchAccess,
      mojom::Setting::kHighlightTextCaret,
      mojom::Setting::kCaretBrowsing,
      mojom::Setting::kAutoClickWhenCursorStops,
      mojom::Setting::kMouseKeysEnabled,
      mojom::Setting::kMouseKeysShortcutToPauseEnabled,
      mojom::Setting::kMouseKeysDisableInTextFields,
      mojom::Setting::kMouseKeysAcceleration,
      mojom::Setting::kMouseKeysMaxSpeed,
      mojom::Setting::kMouseKeysDominantHand,
      mojom::Setting::kLargeCursor,
      mojom::Setting::kHighlightCursorWhileMoving,
      mojom::Setting::kTabletNavigationButtons,
      mojom::Setting::kLiveCaption,
      mojom::Setting::kMainNodeAnnotationsEnabled,
      mojom::Setting::kMonoAudio,
      mojom::Setting::kStartupSound,
      mojom::Setting::kEnableCursorColor,
      mojom::Setting::kColorCorrectionEnabled,
      mojom::Setting::kColorCorrectionFilterType,
      mojom::Setting::kColorCorrectionFilterAmount,
      mojom::Setting::kCaretBlinkInterval,
      mojom::Setting::kReducedAnimationsEnabled,
      mojom::Setting::kOverscrollEnabled,
      mojom::Setting::kFlashNotifications,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kManageAccessibility,
                            kManageAccessibilitySettings, generator);

  // Text-to-Speech.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_MANAGE_TTS_SETTINGS, mojom::Subpage::kTextToSpeech,
      mojom::SearchResultIcon::kTextToSpeech,
      mojom::SearchResultDefaultRank::kMedium, mojom::kTextToSpeechSubpagePath);
  static constexpr mojom::Setting kTextToSpeechSettings[] = {
      mojom::Setting::kTextToSpeechRate,    mojom::Setting::kTextToSpeechPitch,
      mojom::Setting::kTextToSpeechVolume,  mojom::Setting::kTextToSpeechVoice,
      mojom::Setting::kTextToSpeechEngines,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kTextToSpeech,
                            kTextToSpeechSettings, generator);

  // TODO(crbug.com/40246196): Change some of these to RegisterNestedSubpages.
  // Switch access.
  generator->RegisterTopLevelSubpage(IDS_SETTINGS_MANAGE_SWITCH_ACCESS_SETTINGS,
                                     mojom::Subpage::kSwitchAccessOptions,
                                     mojom::SearchResultIcon::kA11y,
                                     mojom::SearchResultDefaultRank::kMedium,
                                     mojom::kSwitchAccessOptionsSubpagePath);
  static constexpr mojom::Setting kSwitchAccessSettings[] = {
      mojom::Setting::kSwitchActionAssignment,
      mojom::Setting::kSwitchActionAutoScan,
      mojom::Setting::kSwitchActionAutoScanKeyboard,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kSwitchAccessOptions,
                            kSwitchAccessSettings, generator);

  // Facegaze settings.
  generator->RegisterTopLevelSubpage(
      IDS_OS_SETTINGS_ACCESSIBILITY_FACEGAZE_LABEL,
      mojom::Subpage::kFaceGazeSettings, mojom::SearchResultIcon::kA11y,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kFaceGazeSettingsSubpagePath);
}

void AccessibilitySection::OnVoicesChanged() {
  UpdateTextToSpeechVoiceSearchTags();
}

void AccessibilitySection::UpdateTextToSpeechVoiceSearchTags() {
  // Start with no text-to-speech voice search tags.
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.RemoveSearchTags(GetTextToSpeechVoiceSearchConcepts());

  content::TtsController* tts_controller =
      content::TtsController::GetInstance();
  std::vector<content::VoiceData> voices;
  tts_controller->GetVoices(profile(), GURL(), &voices);
  if (!voices.empty()) {
    updater.AddSearchTags(GetTextToSpeechVoiceSearchConcepts());
  }
}

void AccessibilitySection::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  UpdateTextToSpeechEnginesSearchTags();
}

void AccessibilitySection::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  UpdateTextToSpeechEnginesSearchTags();
}

void AccessibilitySection::UpdateTextToSpeechEnginesSearchTags() {
  // Start with no text-to-speech engines search tags.
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.RemoveSearchTags(GetTextToSpeechEnginesSearchConcepts());

  const std::set<std::string>& extensions =
      TtsEngineExtensionObserverChromeOSFactory::GetForProfile(profile())
          ->engine_extension_ids();
  if (!extensions.empty()) {
    updater.AddSearchTags(GetTextToSpeechEnginesSearchConcepts());
  }
}

void AccessibilitySection::UpdateSearchTags() {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  if (accessibility_state_utils::IsScreenReaderEnabled()) {
    updater.AddSearchTags(GetA11yLabelsSearchConcepts());
  } else {
    updater.RemoveSearchTags(GetA11yLabelsSearchConcepts());
  }

  updater.RemoveSearchTags(GetA11ySwitchAccessOnSearchConcepts());
  updater.RemoveSearchTags(GetA11ySwitchAccessKeyboardSearchConcepts());

  if (IsLiveCaptionEnabled()) {
    updater.AddSearchTags(GetA11yLiveCaptionSearchConcepts());
  } else {
    updater.RemoveSearchTags(GetA11yLiveCaptionSearchConcepts());
  }

  if (pref_service_->GetBoolean(prefs::kAccessibilityScreenMagnifierEnabled)) {
    updater.AddSearchTags(
        GetA11yFullscreenMagnifierFocusFollowingSearchConcepts());
  } else {
    updater.RemoveSearchTags(
        GetA11yFullscreenMagnifierFocusFollowingSearchConcepts());
  }

  if (IsAccessibilityMagnifierFollowsChromeVoxEnabled()) {
    updater.AddSearchTags(
        GetA11yMagnifierChromeVoxFocusFollowingSearchConcepts());
  } else {
    updater.RemoveSearchTags(
        GetA11yMagnifierChromeVoxFocusFollowingSearchConcepts());
  }

  if (IsAccessibilityMagnifierFollowsStsEnabled()) {
    updater.AddSearchTags(
        GetA11yFullscreenMagnifierSelectToSpeakFocusFollowingSearchConcepts());
  } else {
    updater.RemoveSearchTags(
        GetA11yFullscreenMagnifierSelectToSpeakFocusFollowingSearchConcepts());
  }

  updater.AddSearchTags(GetA11yColorCorrectionSearchConcepts());

  if (IsAccessibilityOverscrollSettingFeatureEnabled()) {
    updater.AddSearchTags(GetA11yOverscrollSettingSearchConcepts());
  }

  if (IsAccessibilityFlashNotificationFeatureEnabled()) {
    updater.AddSearchTags(GetA11yFlashNotificationsSearchConcepts());
  }

  if (!pref_service_->GetBoolean(prefs::kAccessibilitySwitchAccessEnabled)) {
    return;
  }

  updater.AddSearchTags(GetA11ySwitchAccessOnSearchConcepts());

  if (IsSwitchAccessTextAllowed() &&
      pref_service_->GetBoolean(
          prefs::kAccessibilitySwitchAccessAutoScanEnabled)) {
    updater.AddSearchTags(GetA11ySwitchAccessKeyboardSearchConcepts());
  }
}

}  // namespace ash::settings
