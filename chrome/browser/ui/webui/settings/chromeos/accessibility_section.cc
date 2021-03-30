// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/accessibility_section.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_observer_chromeos.h"
#include "chrome/browser/ui/webui/settings/accessibility_main_handler.h"
#include "chrome/browser/ui/webui/settings/captions_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/accessibility_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/settings/chromeos/switch_access_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/tts_handler.h"
#include "chrome/browser/ui/webui/settings/font_handler.h"
#include "chrome/browser/ui/webui/settings/shared_settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "extensions/browser/extension_system.h"
#include "media/base/media_switches.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/events/keyboard_layout_util.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetA11ySearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_A11Y_ALWAYS_SHOW_OPTIONS,
       mojom::kAccessibilitySectionPath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kA11yQuickSettings},
       {IDS_OS_SETTINGS_TAG_A11Y_ALWAYS_SHOW_OPTIONS_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_STICKY_KEYS,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kStickyKeys}},
      {IDS_OS_SETTINGS_TAG_A11Y_LARGE_CURSOR,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kLargeCursor}},
      {IDS_OS_SETTINGS_TAG_A11Y,
       mojom::kAccessibilitySectionPath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kHigh,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kAccessibility},
       {IDS_OS_SETTINGS_TAG_A11Y_ALT1, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_DOCKED_MAGNIFIER,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kDockedMagnifier}},
      {IDS_OS_SETTINGS_TAG_A11y_CHROMEVOX,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kChromeVox},
       {IDS_OS_SETTINGS_TAG_A11y_CHROMEVOX_ALT1, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_MONO_AUDIO,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kMonoAudio},
       {IDS_OS_SETTINGS_TAG_A11Y_MONO_AUDIO_ALT1, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_TEXT_TO_SPEECH,
       mojom::kTextToSpeechSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kTextToSpeech},
       {IDS_OS_SETTINGS_TAG_A11Y_TEXT_TO_SPEECH_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_CAPTIONS,
       mojom::kCaptionsSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kCaptions}},
      {IDS_OS_SETTINGS_TAG_A11Y_HIGHLIGHT_CURSOR,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kHighlightCursorWhileMoving}},
      {IDS_OS_SETTINGS_TAG_A11Y_MANAGE,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kManageAccessibility},
       {IDS_OS_SETTINGS_TAG_A11Y_MANAGE_ALT1, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_ON_SCREEN_KEYBOARD,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kOnScreenKeyboard}},
      {IDS_OS_SETTINGS_TAG_A11Y_HIGHLIGHT_TEXT_CARET,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kHighlightTextCaret}},
      {IDS_OS_SETTINGS_TAG_A11Y_DICTATION,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kDictation},
       {IDS_OS_SETTINGS_TAG_A11Y_DICTATION_ALT1, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_HIGH_CONTRAST,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kHighContrastMode},
       {IDS_OS_SETTINGS_TAG_A11Y_HIGH_CONTRAST_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_A11Y_HIGHLIGHT_KEYBOARD_FOCUS,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kHighlightKeyboardFocus}},
      {IDS_OS_SETTINGS_TAG_A11Y_STARTUP_SOUND,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kStartupSound}},
      {IDS_OS_SETTINGS_TAG_A11Y_AUTOMATICALLY_CLICK,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAutoClickWhenCursorStops}},
      {IDS_OS_SETTINGS_TAG_A11Y_SELECT_TO_SPEAK,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSelectToSpeak}},
      {IDS_OS_SETTINGS_TAG_A11Y_SPEECH_PITCH,
       mojom::kTextToSpeechSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTextToSpeechPitch}},
      {IDS_OS_SETTINGS_TAG_A11Y_SPEECH_RATE,
       mojom::kTextToSpeechSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTextToSpeechRate}},
      {IDS_OS_SETTINGS_TAG_A11Y_SPEECH_VOLUME,
       mojom::kTextToSpeechSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTextToSpeechVolume}},
      {IDS_OS_SETTINGS_TAG_A11Y_FULLSCREEN_MAGNIFIER,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kFullscreenMagnifier}},
      {IDS_OS_SETTINGS_TAG_A11Y_ENABLE_SWITCH_ACCESS,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kEnableSwitchAccess}},
      {IDS_OS_SETTINGS_TAG_A11Y_CURSOR_COLOR,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kEnableCursorColor}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetTextToSpeechVoiceSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_A11Y_SPEECH_VOICE_PREVIEW,
       mojom::kTextToSpeechSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTextToSpeechVoice}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetTextToSpeechEnginesSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_A11Y_SPEECH_ENGINES,
       mojom::kTextToSpeechSubpagePath,
       mojom::SearchResultIcon::kA11y,
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
       mojom::kManageAccessibilitySubpagePath,
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
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_A11Y_LIVE_CAPTION,
       mojom::kCaptionsSubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kLiveCaption}},
  });
  return *tags;
}

const std::vector<SearchConcept>&
GetA11yFullscreenMagnifierFocusFollowingSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_A11Y_FULLSCREEN_MAGNIFIER_FOCUS_FOLLOWING,
       mojom::kManageAccessibilitySubpagePath,
       mojom::SearchResultIcon::kA11y,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kFullscreenMagnifierFocusFollowing}},
  });
  return *tags;
}

bool AreExperimentalA11yLabelsAllowed() {
  return base::FeatureList::IsEnabled(
      ::features::kExperimentalAccessibilityLabels);
}

bool IsLiveCaptionEnabled() {
  return base::FeatureList::IsEnabled(media::kLiveCaption);
}

bool IsMagnifierPanningImprovementsEnabled() {
  return features::IsMagnifierPanningImprovementsEnabled();
}

bool IsMagnifierContinuousMouseFollowingModeSettingEnabled() {
  return features::IsMagnifierContinuousMouseFollowingModeSettingEnabled();
}

bool IsSwitchAccessTextAllowed() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableExperimentalAccessibilitySwitchAccessText);
}

bool IsSwitchAccessPointScanningEnabled() {
  return features::IsSwitchAccessPointScanningEnabled();
}

bool IsSwitchAccessSetupGuideAllowed() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableExperimentalAccessibilitySwitchAccessSetupGuide);
}

bool AreTabletNavigationButtonsAllowed() {
  return ash::features::IsHideShelfControlsInTabletModeEnabled() &&
         ash::TabletMode::IsBoardTypeMarkedAsTabletCapable();
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

  if (AreTabletNavigationButtonsAllowed())
    updater.AddSearchTags(GetA11yTabletNavigationButtonSearchConcepts());

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      ash::prefs::kAccessibilitySwitchAccessEnabled,
      base::BindRepeating(&AccessibilitySection::UpdateSearchTags,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      ash::prefs::kAccessibilitySwitchAccessAutoScanEnabled,
      base::BindRepeating(&AccessibilitySection::UpdateSearchTags,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      ash::prefs::kAccessibilityScreenMagnifierEnabled,
      base::BindRepeating(&AccessibilitySection::UpdateSearchTags,
                          base::Unretained(this)));

  UpdateSearchTags();

  // ExtensionService can be null for tests.
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!extension_service)
    return;
  content::TtsController::GetInstance()->AddVoicesChangedDelegate(this);
  extension_registry_ = extensions::ExtensionRegistry::Get(profile);
  extension_registry_->AddObserver(this);

  UpdateTextToSpeechVoiceSearchTags();
  UpdateTextToSpeechEnginesSearchTags();
}

AccessibilitySection::~AccessibilitySection() {
  content::TtsController::GetInstance()->RemoveVoicesChangedDelegate(this);
  if (extension_registry_)
    extension_registry_->RemoveObserver(this);
}

void AccessibilitySection::AddLoadTimeData(
    content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"a11yPageTitle", IDS_SETTINGS_ACCESSIBILITY},
      {"a11yWebStore", IDS_SETTINGS_ACCESSIBILITY_WEB_STORE},
      {"moreFeaturesLinkDescription",
       IDS_SETTINGS_MORE_FEATURES_LINK_DESCRIPTION},
      {"accessibleImageLabelsTitle",
       IDS_SETTINGS_ACCESSIBLE_IMAGE_LABELS_TITLE},
      {"accessibleImageLabelsSubtitle",
       IDS_SETTINGS_ACCESSIBLE_IMAGE_LABELS_SUBTITLE},
      {"settingsSliderRoleDescription",
       IDS_SETTINGS_SLIDER_MIN_MAX_ARIA_ROLE_DESCRIPTION},
      {"manageAccessibilityFeatures",
       IDS_SETTINGS_ACCESSIBILITY_MANAGE_ACCESSIBILITY_FEATURES},
      {"optionsInMenuLabel", IDS_SETTINGS_OPTIONS_IN_MENU_LABEL},
      {"largeMouseCursorLabel", IDS_SETTINGS_LARGE_MOUSE_CURSOR_LABEL},
      {"largeMouseCursorSizeLabel", IDS_SETTINGS_LARGE_MOUSE_CURSOR_SIZE_LABEL},
      {"largeMouseCursorSizeDefaultLabel",
       IDS_SETTINGS_LARGE_MOUSE_CURSOR_SIZE_DEFAULT_LABEL},
      {"largeMouseCursorSizeLargeLabel",
       IDS_SETTINGS_LARGE_MOUSE_CURSOR_SIZE_LARGE_LABEL},
      {"cursorColorOptionsLabel", IDS_SETTINGS_CURSOR_COLOR_OPTIONS_LABEL},
      {"cursorColorBlack", IDS_SETTINGS_CURSOR_COLOR_BLACK},
      {"cursorColorRed", IDS_SETTINGS_CURSOR_COLOR_RED},
      {"cursorColorYellow", IDS_SETTINGS_CURSOR_COLOR_YELLOW},
      {"cursorColorGreen", IDS_SETTINGS_CURSOR_COLOR_GREEN},
      {"cursorColorCyan", IDS_SETTINGS_CURSOR_COLOR_CYAN},
      {"cursorColorBlue", IDS_SETTINGS_CURSOR_COLOR_BLUE},
      {"cursorColorMagenta", IDS_SETTINGS_CURSOR_COLOR_MAGENTA},
      {"cursorColorPink", IDS_SETTINGS_CURSOR_COLOR_PINK},
      {"highContrastLabel", IDS_SETTINGS_HIGH_CONTRAST_LABEL},
      {"stickyKeysLabel", IDS_SETTINGS_STICKY_KEYS_LABEL},
      {"chromeVoxLabel", IDS_SETTINGS_CHROMEVOX_LABEL},
      {"chromeVoxOptionsLabel", IDS_SETTINGS_CHROMEVOX_OPTIONS_LABEL},
      {"screenMagnifierLabel", IDS_SETTINGS_SCREEN_MAGNIFIER_LABEL},
      {"screenMagnifierHintLabel", IDS_SETTINGS_SCREEN_MAGNIFIER_HINT_LABEL},
      {"screenMagnifierMouseFollowingModeContinuous",
       IDS_SETTINGS_SCREEN_MANIFIER_MOUSE_FOLLOWING_MODE_CONTINUOUS},
      {"screenMagnifierMouseFollowingModeCentered",
       IDS_SETTINGS_SCREEN_MANIFIER_MOUSE_FOLLOWING_MODE_CENTERED},
      {"screenMagnifierMouseFollowingModeEdge",
       IDS_SETTINGS_SCREEN_MANIFIER_MOUSE_FOLLOWING_MODE_EDGE},
      {"screenMagnifierFocusFollowingLabel",
       IDS_SETTINGS_SCREEN_MAGNIFIER_FOCUS_FOLLOWING_LABEL},
      {"screenMagnifierZoomLabel", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_LABEL},
      {"screenMagnifierZoomHintLabel",
       IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_HINT_LABEL},
      {"dockedMagnifierLabel", IDS_SETTINGS_DOCKED_MAGNIFIER_LABEL},
      {"dockedMagnifierZoomLabel", IDS_SETTINGS_DOCKED_MAGNIFIER_ZOOM_LABEL},
      {"screenMagnifierZoom2x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_2_X},
      {"screenMagnifierZoom4x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_4_X},
      {"screenMagnifierZoom6x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_6_X},
      {"screenMagnifierZoom8x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_8_X},
      {"screenMagnifierZoom10x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_10_X},
      {"screenMagnifierZoom12x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_12_X},
      {"screenMagnifierZoom14x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_14_X},
      {"screenMagnifierZoom16x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_16_X},
      {"screenMagnifierZoom18x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_18_X},
      {"screenMagnifierZoom20x", IDS_SETTINGS_SCREEN_MAGNIFIER_ZOOM_20_X},
      {"tapDraggingLabel", IDS_SETTINGS_TAP_DRAGGING_LABEL},
      {"clickOnStopLabel", IDS_SETTINGS_CLICK_ON_STOP_LABEL},
      {"delayBeforeClickLabel", IDS_SETTINGS_DELAY_BEFORE_CLICK_LABEL},
      {"delayBeforeClickExtremelyShort",
       IDS_SETTINGS_DELAY_BEFORE_CLICK_EXTREMELY_SHORT},
      {"delayBeforeClickVeryShort", IDS_SETTINGS_DELAY_BEFORE_CLICK_VERY_SHORT},
      {"delayBeforeClickShort", IDS_SETTINGS_DELAY_BEFORE_CLICK_SHORT},
      {"delayBeforeClickLong", IDS_SETTINGS_DELAY_BEFORE_CLICK_LONG},
      {"delayBeforeClickVeryLong", IDS_SETTINGS_DELAY_BEFORE_CLICK_VERY_LONG},
      {"autoclickRevertToLeftClick",
       IDS_SETTINGS_AUTOCLICK_REVERT_TO_LEFT_CLICK},
      {"autoclickStabilizeCursorPosition",
       IDS_SETTINGS_AUTOCLICK_STABILIZE_CURSOR_POSITION},
      {"autoclickMovementThresholdLabel",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_LABEL},
      {"autoclickMovementThresholdExtraSmall",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_EXTRA_SMALL},
      {"autoclickMovementThresholdSmall",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_SMALL},
      {"autoclickMovementThresholdDefault",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_DEFAULT},
      {"autoclickMovementThresholdLarge",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_LARGE},
      {"autoclickMovementThresholdExtraLarge",
       IDS_SETTINGS_AUTOCLICK_MOVEMENT_THRESHOLD_EXTRA_LARGE},
      {"dictationDescription",
       IDS_SETTINGS_ACCESSIBILITY_DICTATION_DESCRIPTION},
      {"dictationLabel", IDS_SETTINGS_ACCESSIBILITY_DICTATION_LABEL},
      {"onScreenKeyboardLabel", IDS_SETTINGS_ON_SCREEN_KEYBOARD_LABEL},
      {"monoAudioLabel", IDS_SETTINGS_MONO_AUDIO_LABEL},
      {"startupSoundLabel", IDS_SETTINGS_STARTUP_SOUND_LABEL},
      {"a11yExplanation", IDS_SETTINGS_ACCESSIBILITY_EXPLANATION},
      {"caretHighlightLabel",
       IDS_SETTINGS_ACCESSIBILITY_CARET_HIGHLIGHT_DESCRIPTION},
      {"cursorHighlightLabel",
       IDS_SETTINGS_ACCESSIBILITY_CURSOR_HIGHLIGHT_DESCRIPTION},
      {"focusHighlightLabel",
       IDS_SETTINGS_ACCESSIBILITY_FOCUS_HIGHLIGHT_DESCRIPTION},
      {"selectToSpeakTitle", IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_TITLE},
      {"selectToSpeakDisabledDescription",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_DISABLED_DESCRIPTION},
      {"selectToSpeakDescription",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_DESCRIPTION},
      {"selectToSpeakDescriptionWithoutKeyboard",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_DESCRIPTION_WITHOUT_KEYBOARD},
      {"selectToSpeakOptionsLabel",
       IDS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_OPTIONS_LABEL},
      {"switchAccessLabel",
       IDS_SETTINGS_ACCESSIBILITY_SWITCH_ACCESS_DESCRIPTION},
      {"switchAccessOptionsLabel",
       IDS_SETTINGS_ACCESSIBILITY_SWITCH_ACCESS_OPTIONS_LABEL},
      {"manageSwitchAccessSettings",
       IDS_SETTINGS_MANAGE_SWITCH_ACCESS_SETTINGS},
      {"switchAssignmentHeading", IDS_SETTINGS_SWITCH_ASSIGNMENT_HEADING},
      {"switchAccessSetupGuideLabel",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_GUIDE_LABEL},
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
      {"assignSelectSwitchLabel", IDS_SETTINGS_ASSIGN_SELECT_SWITCH_LABEL},
      {"assignNextSwitchLabel", IDS_SETTINGS_ASSIGN_NEXT_SWITCH_LABEL},
      {"assignPreviousSwitchLabel", IDS_SETTINGS_ASSIGN_PREVIOUS_SWITCH_LABEL},
      {"switchAccessInternalDeviceTypeLabel",
       IDS_SETTINGS_SWITCH_ACCESS_INTERNAL_DEVICE_TYPE_LABEL},
      {"switchAccessUsbDeviceTypeLabel",
       IDS_SETTINGS_SWITCH_ACCESS_USB_DEVICE_TYPE_LABEL},
      {"switchAccessBluetoothDeviceTypeLabel",
       IDS_SETTINGS_SWITCH_ACCESS_BLUETOOTH_DEVICE_TYPE_LABEL},
      {"switchAccessUnknownDeviceTypeLabel",
       IDS_SETTINGS_SWITCH_ACCESS_UNKNOWN_DEVICE_TYPE_LABEL},
      {"switchAccessActionAssignmentDialogAssignedIconLabel",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_DIALOG_ASSIGNED_ICON_LABEL},
      {"switchAccessActionAssignmentDialogAddAssignmentIconLabel",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_DIALOG_ADD_ASSIGNMENT_ICON_LABEL},
      {"switchAccessActionAssignmentDialogRemoveAssignmentIconLabel",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_DIALOG_REMOVE_ASSIGNMENT_ICON_LABEL},
      {"switchAccessActionAssignmentDialogErrorIconLabel",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_DIALOG_ERROR_ICON_LABEL},
      {"switchAccessActionAssignmentDialogTitle",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_DIALOG_TITLE},
      {"switchAccessActionAssignmentDialogWarnNotConfirmedPrompt",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_DIALOG_WARN_NOT_CONFIRMED_PROMPT},
      {"switchAccessActionAssignmentDialogWarnAlreadyAssignedActionPrompt",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_DIALOG_WARN_ALREADY_ASSIGNED_ACTION_PROMPT},
      {"switchAccessActionAssignmentDialogWarnUnrecognizedKeyPrompt",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_DIALOG_WARN_UNRECOGNIZED_KEY_PROMPT},
      {"switchAccessActionAssignmentDialogWaitForKeyPromptNoSwitches",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_DIALOG_WAIT_FOR_KEY_PROMPT_NO_SWITCHES},
      {"switchAccessActionAssignmentDialogWaitForKeyPromptAtLeastOneSwitch",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_DIALOG_WAIT_FOR_KEY_PROMPT_AT_LEAST_ONE_SWITCH},
      {"switchAccessActionAssignmentDialogWaitForConfirmationPrompt",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_DIALOG_WAIT_FOR_CONFIRMATION_PROMPT},
      {"switchAccessActionAssignmentDialogWaitForConfirmationRemovalPrompt",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_DIALOG_WAIT_FOR_CONFIRMATION_REMOVAL_PROMPT},
      {"switchAccessActionAssignmentDialogWarnCannotRemoveLastSelectSwitch",
       IDS_SETTINGS_SWITCH_ACCESS_ACTION_ASSIGNMENT_DIALOG_WARN_CANNOT_REMOVE_LAST_SELECT_SWITCH},
      {"switchAndDeviceType", IDS_SETTINGS_SWITCH_AND_DEVICE_TYPE},
      {"noSwitchesAssigned", IDS_SETTINGS_NO_SWITCHES_ASSIGNED},
      {"switchAccessDialogExit", IDS_SETTINGS_SWITCH_ACCESS_DIALOG_EXIT},
      {"switchAccessSetupIntroTitle",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_INTRO_TITLE},
      {"switchAccessSetupIntroBody",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_INTRO_BODY},
      {"switchAccessSetupPairBluetooth",
       IDS_SETTINGS_SWITCH_ACCESS_SETUP_PAIR_BLUETOOTH},
      {"switchAccessSetupNext", IDS_SETTINGS_SWITCH_ACCESS_SETUP_NEXT},
      {"switchAccessSetupPrevious", IDS_SETTINGS_SWITCH_ACCESS_SETUP_PREVIOUS},
      {"switchAccessAutoScanHeading",
       IDS_SETTINGS_SWITCH_ACCESS_AUTO_SCAN_HEADING},
      {"switchAccessAutoScanLabel", IDS_SETTINGS_SWITCH_ACCESS_AUTO_SCAN_LABEL},
      {"switchAccessAutoScanSpeedLabel",
       IDS_SETTINGS_SWITCH_ACCESS_AUTO_SCAN_SPEED_LABEL},
      {"switchAccessAutoScanKeyboardSpeedLabel",
       IDS_SETTINGS_SWITCH_ACCESS_AUTO_SCAN_KEYBOARD_SPEED_LABEL},
      {"switchAccessPointScanSpeedLabel",
       IDS_SETTINGS_SWITCH_ACCESS_POINT_SCAN_SPEED_LABEL},
      {"durationInSeconds", IDS_SETTINGS_DURATION_IN_SECONDS},
      {"manageAccessibilityFeatures",
       IDS_SETTINGS_ACCESSIBILITY_MANAGE_ACCESSIBILITY_FEATURES},
      {"textToSpeechHeading",
       IDS_SETTINGS_ACCESSIBILITY_TEXT_TO_SPEECH_HEADING},
      {"displayHeading", IDS_SETTINGS_ACCESSIBILITY_DISPLAY_HEADING},
      {"displaySettingsTitle",
       IDS_SETTINGS_ACCESSIBILITY_DISPLAY_SETTINGS_TITLE},
      {"displaySettingsDescription",
       IDS_SETTINGS_ACCESSIBILITY_DISPLAY_SETTINGS_DESCRIPTION},
      {"appearanceSettingsTitle",
       IDS_SETTINGS_ACCESSIBILITY_APPEARANCE_SETTINGS_TITLE},
      {"appearanceSettingsDescription",
       IDS_SETTINGS_ACCESSIBILITY_APPEARANCE_SETTINGS_DESCRIPTION},
      {"keyboardAndTextInputHeading",
       IDS_SETTINGS_ACCESSIBILITY_KEYBOARD_AND_TEXT_INPUT_HEADING},
      {"keyboardSettingsTitle",
       IDS_SETTINGS_ACCESSIBILITY_KEYBOARD_SETTINGS_TITLE},
      {"keyboardSettingsDescription",
       IDS_SETTINGS_ACCESSIBILITY_KEYBOARD_SETTINGS_DESCRIPTION},
      {"mouseAndTouchpadHeading",
       IDS_SETTINGS_ACCESSIBILITY_MOUSE_AND_TOUCHPAD_HEADING},
      {"mouseSettingsTitle", IDS_SETTINGS_ACCESSIBILITY_MOUSE_SETTINGS_TITLE},
      {"mouseSettingsDescription",
       IDS_SETTINGS_ACCESSIBILITY_MOUSE_SETTINGS_DESCRIPTION},
      {"audioAndCaptionsHeading",
       IDS_SETTINGS_ACCESSIBILITY_AUDIO_AND_CAPTIONS_HEADING},
      {"additionalFeaturesTitle",
       IDS_SETTINGS_ACCESSIBILITY_ADDITIONAL_FEATURES_TITLE},
      {"manageTtsSettings", IDS_SETTINGS_MANAGE_TTS_SETTINGS},
      {"ttsSettingsLinkDescription", IDS_SETTINGS_TTS_LINK_DESCRIPTION},
      {"textToSpeechVoices", IDS_SETTINGS_TEXT_TO_SPEECH_VOICES},
      {"textToSpeechNoVoicesMessage",
       IDS_SETTINGS_TEXT_TO_SPEECH_NO_VOICES_MESSAGE},
      {"textToSpeechMoreLanguages", IDS_SETTINGS_TEXT_TO_SPEECH_MORE_LANGUAGES},
      {"textToSpeechProperties", IDS_SETTINGS_TEXT_TO_SPEECH_PROPERTIES},
      {"textToSpeechRate", IDS_SETTINGS_TEXT_TO_SPEECH_RATE},
      {"textToSpeechRateMinimumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_RATE_MINIMUM_LABEL},
      {"textToSpeechRateMaximumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_RATE_MAXIMUM_LABEL},
      {"textToSpeechPitch", IDS_SETTINGS_TEXT_TO_SPEECH_PITCH},
      {"textToSpeechPitchMinimumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_PITCH_MINIMUM_LABEL},
      {"textToSpeechPitchMaximumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_PITCH_MAXIMUM_LABEL},
      {"textToSpeechVolume", IDS_SETTINGS_TEXT_TO_SPEECH_VOLUME},
      {"textToSpeechVolumeMinimumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_VOLUME_MINIMUM_LABEL},
      {"textToSpeechVolumeMaximumLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_VOLUME_MAXIMUM_LABEL},
      {"percentage", IDS_SETTINGS_PERCENTAGE},
      {"defaultPercentage", IDS_SETTINGS_DEFAULT_PERCENTAGE},
      {"textToSpeechPreviewHeading",
       IDS_SETTINGS_TEXT_TO_SPEECH_PREVIEW_HEADING},
      {"textToSpeechPreviewInputLabel",
       IDS_SETTINGS_TEXT_TO_SPEECH_PREVIEW_INPUT_LABEL},
      {"textToSpeechPreviewInput", IDS_SETTINGS_TEXT_TO_SPEECH_PREVIEW_INPUT},
      {"textToSpeechPreviewVoice", IDS_SETTINGS_TEXT_TO_SPEECH_PREVIEW_VOICE},
      {"textToSpeechPreviewPlay", IDS_SETTINGS_TEXT_TO_SPEECH_PREVIEW_PLAY},
      {"textToSpeechEngines", IDS_SETTINGS_TEXT_TO_SPEECH_ENGINES},
      {"tabletModeShelfNavigationButtonsSettingLabel",
       IDS_SETTINGS_A11Y_TABLET_MODE_SHELF_BUTTONS_LABEL},
      {"tabletModeShelfNavigationButtonsSettingDescription",
       IDS_SETTINGS_A11Y_TABLET_MODE_SHELF_BUTTONS_DESCRIPTION},
      {"caretBrowsingTitle", IDS_SETTINGS_ENABLE_CARET_BROWSING_TITLE},
      {"caretBrowsingSubtitle", IDS_SETTINGS_ENABLE_CARET_BROWSING_SUBTITLE},
      {"cancel", IDS_CANCEL},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddLocalizedString("screenMagnifierHintSearchKey",
                                  ui::DeviceUsesKeyboardLayout2()
                                      ? IDS_SETTINGS_KEYBOARD_KEY_LAUNCHER
                                      : IDS_SETTINGS_KEYBOARD_KEY_SEARCH);

  html_source->AddString("a11yLearnMoreUrl",
                         chrome::kChromeAccessibilityHelpURL);

  html_source->AddBoolean(
      "showExperimentalAccessibilitySwitchAccessImprovedTextInput",
      IsSwitchAccessTextAllowed());

  html_source->AddBoolean("isSwitchAccessPointScanningEnabled",
                          IsSwitchAccessPointScanningEnabled());

  html_source->AddBoolean("showSwitchAccessSetupGuide",
                          IsSwitchAccessSetupGuideAllowed());

  html_source->AddBoolean("showExperimentalA11yLabels",
                          AreExperimentalA11yLabelsAllowed());

  html_source->AddBoolean("showTabletModeShelfNavigationButtonsSettings",
                          AreTabletNavigationButtonsAllowed());

  html_source->AddString("tabletModeShelfNavigationButtonsLearnMoreUrl",
                         chrome::kTabletModeGesturesLearnMoreURL);

  html_source->AddBoolean("isMagnifierPanningImprovementsEnabled",
                          IsMagnifierPanningImprovementsEnabled());

  html_source->AddBoolean(
      "isMagnifierContinuousMouseFollowingModeSettingEnabled",
      IsMagnifierContinuousMouseFollowingModeSettingEnabled());

  ::settings::AddCaptionSubpageStrings(html_source);
}

void AccessibilitySection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<::settings::AccessibilityMainHandler>());
  web_ui->AddMessageHandler(std::make_unique<AccessibilityHandler>(profile()));
  web_ui->AddMessageHandler(
      std::make_unique<SwitchAccessHandler>(profile()->GetPrefs()));
  web_ui->AddMessageHandler(std::make_unique<::settings::TtsHandler>());
  web_ui->AddMessageHandler(
      std::make_unique<::settings::FontHandler>(profile()));
  web_ui->AddMessageHandler(
      std::make_unique<::settings::CaptionsHandler>(profile()->GetPrefs()));
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

std::string AccessibilitySection::GetSectionPath() const {
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
    case mojom::Setting::kFullscreenMagnifierMouseFollowingMode:
      base::UmaHistogramEnumeration(
          "ChromeOS.Settings.Accessibility."
          "FullscreenMagnifierMouseFollowingMode",
          static_cast<ash::MagnifierMouseFollowingMode>(value.GetInt()));
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
  static constexpr mojom::Setting kManageAccessibilitySettings[] = {
      mojom::Setting::kChromeVox,
      mojom::Setting::kSelectToSpeak,
      mojom::Setting::kHighContrastMode,
      mojom::Setting::kFullscreenMagnifier,
      mojom::Setting::kFullscreenMagnifierFocusFollowing,
      mojom::Setting::kFullscreenMagnifierMouseFollowingMode,
      mojom::Setting::kDockedMagnifier,
      mojom::Setting::kStickyKeys,
      mojom::Setting::kOnScreenKeyboard,
      mojom::Setting::kDictation,
      mojom::Setting::kHighlightKeyboardFocus,
      mojom::Setting::kEnableSwitchAccess,
      mojom::Setting::kHighlightTextCaret,
      mojom::Setting::kAutoClickWhenCursorStops,
      mojom::Setting::kLargeCursor,
      mojom::Setting::kHighlightCursorWhileMoving,
      mojom::Setting::kTabletNavigationButtons,
      mojom::Setting::kMonoAudio,
      mojom::Setting::kStartupSound,
      mojom::Setting::kEnableCursorColor,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kManageAccessibility,
                            kManageAccessibilitySettings, generator);

  // Text-to-Speech.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_MANAGE_TTS_SETTINGS, mojom::Subpage::kTextToSpeech,
      mojom::SearchResultIcon::kA11y, mojom::SearchResultDefaultRank::kMedium,
      mojom::kTextToSpeechSubpagePath);
  static constexpr mojom::Setting kTextToSpeechSettings[] = {
      mojom::Setting::kTextToSpeechRate,    mojom::Setting::kTextToSpeechPitch,
      mojom::Setting::kTextToSpeechVolume,  mojom::Setting::kTextToSpeechVoice,
      mojom::Setting::kTextToSpeechEngines,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kTextToSpeech,
                            kTextToSpeechSettings, generator);

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

  // Caption preferences.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_CAPTIONS, mojom::Subpage::kCaptions,
      mojom::SearchResultIcon::kA11y, mojom::SearchResultDefaultRank::kMedium,
      mojom::kCaptionsSubpagePath);
  static constexpr mojom::Setting kCaptionsSettings[] = {
      mojom::Setting::kLiveCaption,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kCaptions, kCaptionsSettings,
                            generator);
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
  tts_controller->GetVoices(profile(), &voices);
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
      TtsEngineExtensionObserverChromeOS::GetInstance(profile())
          ->engine_extension_ids();
  if (!extensions.empty()) {
    updater.AddSearchTags(GetTextToSpeechEnginesSearchConcepts());
  }
}

void AccessibilitySection::UpdateSearchTags() {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  if (accessibility_state_utils::IsScreenReaderEnabled() &&
      AreExperimentalA11yLabelsAllowed()) {
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

  if (IsMagnifierPanningImprovementsEnabled() &&
      pref_service_->GetBoolean(
          ash::prefs::kAccessibilityScreenMagnifierEnabled)) {
    updater.AddSearchTags(
        GetA11yFullscreenMagnifierFocusFollowingSearchConcepts());
  } else {
    updater.RemoveSearchTags(
        GetA11yFullscreenMagnifierFocusFollowingSearchConcepts());
  }

  if (!pref_service_->GetBoolean(
          ash::prefs::kAccessibilitySwitchAccessEnabled)) {
    return;
  }

  updater.AddSearchTags(GetA11ySwitchAccessOnSearchConcepts());

  if (IsSwitchAccessTextAllowed() &&
      pref_service_->GetBoolean(
          ash::prefs::kAccessibilitySwitchAccessAutoScanEnabled)) {
    updater.AddSearchTags(GetA11ySwitchAccessKeyboardSearchConcepts());
  }
}

}  // namespace settings
}  // namespace chromeos
