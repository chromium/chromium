// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/search/search_section.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/url_constants.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/assistant/assistant_util.h"
#include "chrome/browser/ash/input_method/editor_mediator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/assistant_optin/assistant_optin_utils.h"
#include "chrome/browser/ui/webui/ash/settings/pages/search/google_assistant_handler.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_concept.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/settings/search_engines_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kAssistantSubpagePath;
using ::chromeos::settings::mojom::kSearchAndAssistantSectionPath;
using ::chromeos::settings::mojom::kSearchSubpagePath;
using ::chromeos::settings::mojom::kSystemPreferencesSectionPath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

// Whether Quick answers is supported for the current language.
bool IsQuickAnswersSupported() {
  return QuickAnswersState::IsEligibleAs(
      QuickAnswersState::FeatureType::kQuickAnswers);
}

bool IsMagicBoostNoticeBannerVisible(Profile* profile) {
  chromeos::MagicBoostState* magic_boost_state =
      chromeos::MagicBoostState::Get();

  bool hmr_needs_notice_banner = magic_boost_state->IsMagicBoostAvailable() &&
                                 magic_boost_state->CanShowNoticeBannerForHMR();

  bool hmw_needs_notice_banner = false;

  if (chromeos::features::IsOrcaEnabled()) {
    ash::input_method::EditorMediator* editor_mediator =
        ash::input_method::EditorMediatorFactory::GetInstance()->GetForProfile(
            profile);

    hmw_needs_notice_banner = editor_mediator &&
                              editor_mediator->IsAllowedForUse() &&
                              editor_mediator->CanShowNoticeBanner();
  }

  return hmr_needs_notice_banner || hmw_needs_notice_banner;
}

const std::vector<SearchConcept>& GetSearchPageSearchConcepts(
    const char* section_path) {
  if (IsQuickAnswersSupported()) {
    static const base::NoDestructor<std::vector<SearchConcept>> tags({
        {IDS_OS_SETTINGS_TAG_PREFERRED_SEARCH_ENGINE,
         mojom::kSearchSubpagePath,
         mojom::SearchResultIcon::kSearch,
         mojom::SearchResultDefaultRank::kMedium,
         mojom::SearchResultType::kSetting,
         {.setting = mojom::Setting::kPreferredSearchEngine}},
    });
    return *tags;
  }

  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_PREFERRED_SEARCH_ENGINE,
       section_path,
       mojom::SearchResultIcon::kSearch,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kPreferredSearchEngine}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetQuickAnswersSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_QUICK_ANSWERS,
       mojom::kSearchSubpagePath,
       mojom::SearchResultIcon::kSearch,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kQuickAnswersOnOff},
       {IDS_OS_SETTINGS_TAG_QUICK_ANSWERS_ALT1,
        IDS_OS_SETTINGS_TAG_QUICK_ANSWERS_ALT2,
        IDS_OS_SETTINGS_TAG_QUICK_ANSWERS_ALT3, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetQuickAnswersOnSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_QUICK_ANSWERS_DEFINITION,
       mojom::kSearchSubpagePath,
       mojom::SearchResultIcon::kSearch,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kQuickAnswersDefinition}},
      {IDS_OS_SETTINGS_TAG_QUICK_ANSWERS_TRANSLATION,
       mojom::kSearchSubpagePath,
       mojom::SearchResultIcon::kSearch,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kQuickAnswersTranslation}},
      {IDS_OS_SETTINGS_TAG_QUICK_ANSWERS_UNIT_CONVERSION,
       mojom::kSearchSubpagePath,
       mojom::SearchResultIcon::kSearch,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kQuickAnswersUnitConversion}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetAssistantSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_ASSISTANT,
       mojom::kAssistantSubpagePath,
       mojom::SearchResultIcon::kAssistant,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kAssistant}},
      {IDS_OS_SETTINGS_TAG_ASSISTANT_OK_GOOGLE,
       mojom::kAssistantSubpagePath,
       mojom::SearchResultIcon::kAssistant,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAssistantOkGoogle},
       {IDS_OS_SETTINGS_TAG_ASSISTANT_OK_GOOGLE_ALT1,
        IDS_OS_SETTINGS_TAG_ASSISTANT_OK_GOOGLE_ALT2,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetAssistantOnSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_ASSISTANT_TURN_OFF,
       mojom::kAssistantSubpagePath,
       mojom::SearchResultIcon::kAssistant,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAssistantOnOff},
       {IDS_OS_SETTINGS_TAG_ASSISTANT_TURN_OFF_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_ASSISTANT_PREFERRED_INPUT,
       mojom::kAssistantSubpagePath,
       mojom::SearchResultIcon::kAssistant,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAssistantVoiceInput}},
      {IDS_OS_SETTINGS_TAG_ASSISTANT_NOTIFICATIONS,
       mojom::kAssistantSubpagePath,
       mojom::SearchResultIcon::kAssistant,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAssistantNotifications}},
      {IDS_OS_SETTINGS_TAG_ASSISTANT_RELATED_INFO,
       mojom::kAssistantSubpagePath,
       mojom::SearchResultIcon::kAssistant,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAssistantRelatedInfo}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetAssistantOffSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_ASSISTANT_TURN_ON,
       mojom::kAssistantSubpagePath,
       mojom::SearchResultIcon::kAssistant,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAssistantOnOff},
       {IDS_OS_SETTINGS_TAG_ASSISTANT_TURN_ON_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetAssistantVoiceMatchSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_ASSISTANT_TRAIN_VOICE_MODEL,
       mojom::kAssistantSubpagePath,
       mojom::SearchResultIcon::kAssistant,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTrainAssistantVoiceModel}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetMagicBoostSearchConcepts(
    const char* section_path) {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_MAGIC_BOOST,
       section_path,
       mojom::SearchResultIcon::kMagicBoost,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kMagicBoostOnOff}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetMagicBoostSubSearchConcepts(
    const char* section_path) {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_MAGIC_BOOST_HMR,
       section_path,
       mojom::SearchResultIcon::kHelpMeRead,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kMahiOnOff}},
      {IDS_OS_SETTINGS_TAG_MAGIC_BOOST_HMW,
       section_path,
       mojom::SearchResultIcon::kHelpMeWrite,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kShowOrca}},
  });
  return *tags;
}

bool IsVoiceMatchAllowed() {
  return !assistant::features::IsVoiceMatchDisabled();
}

void AddQuickAnswersStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"quickAnswersEnable", IDS_SETTINGS_QUICK_ANSWERS_ENABLE},
      {"quickAnswersEnableDescription",
       IDS_SETTINGS_QUICK_ANSWERS_ENABLE_DESCRIPTION},
      {"quickAnswersEnableDescriptionWithLink",
       IDS_SETTINGS_QUICK_ANSWERS_ENABLE_DESCRIPTION_WITH_LINK},
      {"quickAnswersDefinitionEnable",
       IDS_SETTINGS_QUICK_ANSWERS_DEFINITION_ENABLE},
      {"quickAnswersTranslationEnable",
       IDS_SETTINGS_QUICK_ANSWERS_TRANSLATION_ENABLE},
      {"quickAnswersTranslationEnableDescription",
       IDS_SETTINGS_QUICK_ANSWERS_TRANSLATION_ENABLE_DESCRIPTION},
      {"quickAnswersUnitConversionEnable",
       IDS_SETTINGS_QUICK_ANSWERS_UNIT_CONVERSION_ENABLE},
  };

  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddBoolean(
      "quickAnswersTranslationDisabled",
      chromeos::features::IsQuickAnswersV2TranslationDisabled());
  html_source->AddBoolean(
      "quickAnswersSubToggleEnabled",
      chromeos::features::IsQuickAnswersV2SettingsSubToggleEnabled());
}

void AddGoogleAssistantStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"googleAssistantPageTitle", IDS_SETTINGS_GOOGLE_ASSISTANT},
      {"googleAssistantEnableContext", IDS_ASSISTANT_SCREEN_CONTEXT_TITLE},
      {"googleAssistantEnableContextDescription",
       IDS_SETTINGS_GOOGLE_ASSISTANT_SCREEN_CONTEXT_DESCRIPTION},
      {"googleAssistantEnableHotword",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_HOTWORD},
      {"googleAssistantEnableHotwordDescription",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_HOTWORD_DESCRIPTION},
      {"googleAssistantVoiceSettings",
       IDS_SETTINGS_GOOGLE_ASSISTANT_VOICE_SETTINGS},
      {"googleAssistantVoiceSettingsDescription",
       IDS_ASSISTANT_VOICE_MATCH_RECORDING},
      {"googleAssistantVoiceSettingsRetrainButton",
       IDS_SETTINGS_GOOGLE_ASSISTANT_VOICE_SETTINGS_RETRAIN},
      {"googleAssistantEnableHotwordWithoutDspDescription",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_HOTWORD_WITHOUT_DSP_DESCRIPTION},
      {"googleAssistantEnableHotwordWithoutDspRecommended",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_HOTWORD_WITHOUT_DSP_RECOMMENDED},
      {"googleAssistantEnableHotwordWithoutDspAlwaysOn",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_HOTWORD_WITHOUT_DSP_ALWAYS_ON},
      {"googleAssistantEnableHotwordWithoutDspOff",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_HOTWORD_WITHOUT_DSP_OFF},
      {"googleAssistantEnableNotification",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_NOTIFICATION},
      {"googleAssistantEnableNotificationDescription",
       IDS_SETTINGS_GOOGLE_ASSISTANT_ENABLE_NOTIFICATION_DESCRIPTION},
      {"googleAssistantLaunchWithMicOpen",
       IDS_SETTINGS_GOOGLE_ASSISTANT_LAUNCH_WITH_MIC_OPEN},
      {"googleAssistantLaunchWithMicOpenDescription",
       IDS_SETTINGS_GOOGLE_ASSISTANT_LAUNCH_WITH_MIC_OPEN_DESCRIPTION},
      {"googleAssistantSettings", IDS_SETTINGS_GOOGLE_ASSISTANT_SETTINGS},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddBoolean("hotwordDspAvailable", IsHotwordDspAvailable());
  html_source->AddBoolean("voiceMatchDisabled", !IsVoiceMatchAllowed());
}

}  // namespace

SearchSection::SearchSection(Profile* profile,
                             SearchTagRegistry* search_tag_registry)
    : OsSettingsSection(profile, search_tag_registry) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  updater.AddSearchTags(GetSearchPageSearchConcepts(GetSectionPath()));

  AssistantState* assistant_state = AssistantState::Get();
  if (IsAssistantAllowed() && assistant_state) {
    updater.AddSearchTags(GetAssistantSearchConcepts());

    assistant_state->AddObserver(this);
    UpdateAssistantSearchTags();
  }

  if (QuickAnswersState::Get()) {
    QuickAnswersState::Get()->AddObserver(this);
    UpdateQuickAnswersSearchTags();
  }

  auto* magic_boost_state = chromeos::MagicBoostState::Get();
  if (magic_boost_state && magic_boost_state->IsMagicBoostAvailable()) {
    updater.AddSearchTags(GetMagicBoostSearchConcepts(GetSectionPath()));
    magic_boost_state->AddObserver(this);
    UpdateSubMagicBoostSearchTags();
  }
}

SearchSection::~SearchSection() {
  AssistantState* assistant_state = AssistantState::Get();
  if (IsAssistantAllowed() && assistant_state) {
    assistant_state->RemoveObserver(this);
  }
}

void SearchSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  const bool kIsRevampEnabled =
      ash::features::IsOsSettingsRevampWayfindingEnabled();

  webui::LocalizedString kLocalizedStrings[] = {
      {"enableMagicBoost", IDS_OS_SETTINGS_ENABLE_MAGIC_BOOST},
      {"enableMagicBoostDesc", IDS_OS_SETTINGS_ENABLE_MAGIC_BOOST_DESCRIPTION},
      {"magicBoostReviewTermsBannerDescription",
       IDS_OS_SETTINGS_MAGIC_BOOST_REVIEW_TERMS_BANNER_DESCRIPTION},
      {"magicBoostReviewTermsButtonLabel",
       IDS_OS_SETTINGS_MAGIC_BOOST_REVIEW_TERMS_BUTTON_LABEL},
      {"enableMagicBoostDesc", IDS_OS_SETTINGS_ENABLE_MAGIC_BOOST_DESCRIPTION},
      {"enableHelpMeRead", IDS_OS_SETTINGS_ENABLE_HELP_ME_READ},
      {"enableHelpMeReadDesc", IDS_OS_SETTINGS_ENABLE_HELP_ME_READ_DESCRIPTION},
      {"enableHelpMeWrite", IDS_OS_SETTINGS_ENABLE_HELP_ME_WRITE},
      {"enableHelpMeWriteDesc",
       IDS_OS_SETTINGS_ENABLE_HELP_ME_WRITE_DESCRIPTION},
      {"osSearchEngineLabel", kIsRevampEnabled
                                  ? IDS_OS_SETTINGS_REVAMP_SEARCH_ENGINE_LABEL
                                  : IDS_OS_SETTINGS_SEARCH_ENGINE_LABEL},
      {"searchSubpageTitle", IDS_SETTINGS_SEARCH_SUBPAGE_TITLE},
      {"searchGoogleAssistant", IDS_SETTINGS_SEARCH_GOOGLE_ASSISTANT},
      {"searchGoogleAssistantEnabled",
       kIsRevampEnabled ? IDS_OS_SETTINGS_SEARCH_GOOGLE_ASSISTANT_ON
                        : IDS_SETTINGS_SEARCH_GOOGLE_ASSISTANT_ENABLED},
      {"searchGoogleAssistantDisabled",
       kIsRevampEnabled ? IDS_OS_SETTINGS_SEARCH_GOOGLE_ASSISTANT_OFF
                        : IDS_SETTINGS_SEARCH_GOOGLE_ASSISTANT_DISABLED},
      {"searchGoogleAssistantOn", IDS_OS_SETTINGS_SEARCH_GOOGLE_ASSISTANT_ON},
      {"searchGoogleAssistantOff", IDS_OS_SETTINGS_SEARCH_GOOGLE_ASSISTANT_OFF},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString("helpMeReadWriteLearnMoreUrl",
                         chrome::kHelpMeReadWriteLearnMoreURL);

  html_source->AddBoolean("isQuickAnswersSupported", IsQuickAnswersSupported());

  html_source->AddBoolean(
      "isMagicBoostFeatureEnabled",
      chromeos::MagicBoostState::Get()->IsMagicBoostAvailable());

  html_source->AddBoolean("isMagicBoostNoticeBannerVisible",
                          IsMagicBoostNoticeBannerVisible(profile()));

  const bool is_assistant_allowed = IsAssistantAllowed();
  html_source->AddBoolean("isAssistantAllowed", is_assistant_allowed);
  html_source->AddLocalizedString("osSearchPageTitle",
                                  is_assistant_allowed
                                      ? IDS_SETTINGS_SEARCH_AND_ASSISTANT
                                      : IDS_SETTINGS_SEARCH);
  html_source->AddString("osSearchEngineDescription",
                         ui::SubstituteChromeOSDeviceType(
                             IDS_OS_SETTINGS_SEARCH_ENGINE_DESCRIPTION));

  AddQuickAnswersStrings(html_source);
  AddGoogleAssistantStrings(html_source);
}

void SearchSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<::settings::SearchEnginesHandler>(profile()));
  web_ui->AddMessageHandler(std::make_unique<GoogleAssistantHandler>());
}

int SearchSection::GetSectionNameMessageId() const {
  return IsAssistantAllowed() ? IDS_SETTINGS_SEARCH_AND_ASSISTANT
                              : IDS_SETTINGS_SEARCH;
}

mojom::Section SearchSection::GetSection() const {
  return ash::features::IsOsSettingsRevampWayfindingEnabled()
             ? mojom::Section::kSystemPreferences
             : mojom::Section::kSearchAndAssistant;
}

mojom::SearchResultIcon SearchSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kSearch;
}

const char* SearchSection::GetSectionPath() const {
  return ash::features::IsOsSettingsRevampWayfindingEnabled()
             ? mojom::kSystemPreferencesSectionPath
             : mojom::kSearchAndAssistantSectionPath;
}

bool SearchSection::LogMetric(mojom::Setting setting,
                              base::Value& value) const {
  switch (setting) {
    case mojom::Setting::kMagicBoostOnOff:
      base::UmaHistogramBoolean("ChromeOS.Settings.MagicBoost.Enabled",
                                value.GetBool());
      return true;

    case mojom::Setting::kMahiOnOff:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.MagicBoost.HelpMeReadEnabled", value.GetBool());
      return true;

    case mojom::Setting::kShowOrca:
      base::UmaHistogramBoolean(
          "ChromeOS.Settings.MagicBoost.HelpMeWriteEnabled", value.GetBool());
      return true;

    default:
      return false;
  }
}

void SearchSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  // Register Preferred search engine as top level settings if Quick answers is
  // not available.
  if (!IsQuickAnswersSupported()) {
    generator->RegisterTopLevelSetting(mojom::Setting::kPreferredSearchEngine);
  }

  // TODO(b:337868408): Setting::kShowOrca is already registered in
  // device/input_section.cc, therefore UMA emitted from search_secion fails to
  // log it.
  generator->RegisterTopLevelSetting(mojom::Setting::kMahiOnOff);
  generator->RegisterTopLevelSetting(mojom::Setting::kMagicBoostOnOff);

  // Search.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_SEARCH_SUBPAGE_TITLE, mojom::Subpage::kSearch,
      mojom::SearchResultIcon::kSearch, mojom::SearchResultDefaultRank::kMedium,
      mojom::kSearchSubpagePath);
  // Register Preferred search engine under Search subpage if Quick answers is
  // available.
  if (IsQuickAnswersSupported()) {
    static constexpr mojom::Setting kSearchSettingsWithPreferredSearchEngine[] =
        {
            mojom::Setting::kQuickAnswersOnOff,
            mojom::Setting::kQuickAnswersDefinition,
            mojom::Setting::kQuickAnswersTranslation,
            mojom::Setting::kQuickAnswersUnitConversion,
            mojom::Setting::kPreferredSearchEngine,
        };
    RegisterNestedSettingBulk(mojom::Subpage::kSearch,
                              kSearchSettingsWithPreferredSearchEngine,
                              generator);
  } else {
    static constexpr mojom::Setting
        kSearchSettingsWithoutPreferredSearchEngine[] = {
            mojom::Setting::kQuickAnswersOnOff,
            mojom::Setting::kQuickAnswersDefinition,
            mojom::Setting::kQuickAnswersTranslation,
            mojom::Setting::kQuickAnswersUnitConversion,
        };
    RegisterNestedSettingBulk(mojom::Subpage::kSearch,
                              kSearchSettingsWithoutPreferredSearchEngine,
                              generator);
  }

  // Assistant.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_GOOGLE_ASSISTANT, mojom::Subpage::kAssistant,
      mojom::SearchResultIcon::kAssistant,
      mojom::SearchResultDefaultRank::kMedium, mojom::kAssistantSubpagePath);
  static constexpr mojom::Setting kAssistantSettings[] = {
      mojom::Setting::kAssistantOnOff,
      mojom::Setting::kAssistantRelatedInfo,
      mojom::Setting::kAssistantOkGoogle,
      mojom::Setting::kAssistantNotifications,
      mojom::Setting::kAssistantVoiceInput,
      mojom::Setting::kTrainAssistantVoiceModel,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kAssistant, kAssistantSettings,
                            generator);
}

void SearchSection::OnAssistantConsentStatusChanged(int consent_status) {
  UpdateAssistantSearchTags();
}

void SearchSection::OnAssistantContextEnabled(bool enabled) {
  UpdateAssistantSearchTags();
}

void SearchSection::OnAssistantSettingsEnabled(bool enabled) {
  UpdateAssistantSearchTags();
}

void SearchSection::OnAssistantHotwordEnabled(bool enabled) {
  UpdateAssistantSearchTags();
}

void SearchSection::OnSettingsEnabled(bool enabled) {
  UpdateQuickAnswersSearchTags();
}

void SearchSection::OnEligibilityChanged(bool eligible) {
  UpdateQuickAnswersSearchTags();
}

void SearchSection::OnMagicBoostEnabledUpdated(bool enabled) {
  UpdateSubMagicBoostSearchTags();
}

void SearchSection::OnIsDeleting() {
  magic_boost_state_observation_.Reset();
}

bool SearchSection::IsAssistantAllowed() const {
  // NOTE: This will be false when the flag is disabled.
  return ::assistant::IsAssistantAllowedForProfile(profile()) ==
         assistant::AssistantAllowedState::ALLOWED;
}

void SearchSection::UpdateAssistantSearchTags() {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  // Start without any Assistant search concepts, then add if needed below.
  updater.RemoveSearchTags(GetAssistantOnSearchConcepts());
  updater.RemoveSearchTags(GetAssistantOffSearchConcepts());
  updater.RemoveSearchTags(GetAssistantVoiceMatchSearchConcepts());

  AssistantState* assistant_state = AssistantState::Get();

  // The setting_enabled() function is the top-level enabled state. If this is
  // off, none of the sub-features are enabled.
  if (!assistant_state->settings_enabled() ||
      !assistant_state->settings_enabled().value()) {
    updater.AddSearchTags(GetAssistantOffSearchConcepts());
    return;
  }

  updater.AddSearchTags(GetAssistantOnSearchConcepts());

  if (IsVoiceMatchAllowed() && assistant_state->hotword_enabled() &&
      assistant_state->hotword_enabled().value() &&
      assistant_state->consent_status() &&
      assistant_state->consent_status().value() ==
          assistant::prefs::ConsentStatus::kActivityControlAccepted) {
    updater.AddSearchTags(GetAssistantVoiceMatchSearchConcepts());
  }
}

void SearchSection::UpdateQuickAnswersSearchTags() {
  DCHECK(QuickAnswersState::Get());

  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  updater.RemoveSearchTags(GetQuickAnswersSearchConcepts());
  updater.RemoveSearchTags(GetQuickAnswersOnSearchConcepts());

  if (!IsQuickAnswersSupported()) {
    return;
  }

  updater.AddSearchTags(GetQuickAnswersSearchConcepts());

  if (chromeos::features::IsQuickAnswersV2SettingsSubToggleEnabled() &&
      QuickAnswersState::IsEnabledAs(
          QuickAnswersState::FeatureType::kQuickAnswers)) {
    updater.AddSearchTags(GetQuickAnswersOnSearchConcepts());
  }
}

void SearchSection::UpdateSubMagicBoostSearchTags() {
  auto* magic_boost_state = chromeos::MagicBoostState::Get();
  DCHECK(magic_boost_state);

  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  updater.RemoveSearchTags(GetMagicBoostSubSearchConcepts(GetSectionPath()));

  if (magic_boost_state->magic_boost_enabled().value_or(false)) {
    updater.AddSearchTags(GetMagicBoostSubSearchConcepts(GetSectionPath()));
  }
}

}  // namespace ash::settings
