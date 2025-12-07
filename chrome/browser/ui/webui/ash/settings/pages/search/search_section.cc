// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/search/search_section.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/url_constants.h"
#include "ash/lobster/lobster_controller.h"
#include "ash/public/cpp/capture_mode/capture_mode_api.h"
#include "ash/scanner/scanner_controller.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/input_method/editor_mediator_factory.h"
#include "chrome/browser/ash/lobster/lobster_service.h"
#include "chrome/browser/ash/lobster/lobster_service_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_concept.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/settings/search_engines_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
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
#include "ui/webui/webui_util.h"

namespace ash::settings {

namespace mojom {
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

  bool hmr_needs_notice_banner =
      magic_boost_state->IsUserEligibleForGenAIFeatures() &&
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

bool IsOrcaSettingsToggleVisible(Profile* profile) {
  if (!chromeos::features::IsOrcaEnabled()) {
    return false;
  }

  ash::input_method::EditorMediator* editor_mediator =
      ash::input_method::EditorMediatorFactory::GetInstance()->GetForProfile(
          profile);
  return editor_mediator && editor_mediator->IsAllowedForUse();
}

bool IsLobsterSettingsToggleVisible(Profile* profile) {
  return ash::features::IsLobsterEnabled() &&
         LobsterServiceProvider::GetForProfile(profile) != nullptr &&
         LobsterServiceProvider::GetForProfile(profile)
             ->CanShowFeatureSettingsToggle();
}

bool IsScannerSettingsToggleVisible() {
  ash::Shell* shell = ash::Shell::HasInstance() ? Shell::Get() : nullptr;
  return shell && shell->scanner_controller() &&
         shell->scanner_controller()->CanShowFeatureSettingsToggle();
}

base::span<const SearchConcept> GetSearchPageSearchConcepts() {
  if (IsQuickAnswersSupported()) {
    static constexpr auto tags = std::to_array<SearchConcept>({
        {IDS_OS_SETTINGS_TAG_PREFERRED_SEARCH_ENGINE,
         mojom::kSearchSubpagePath,
         mojom::SearchResultIcon::kSearch,
         mojom::SearchResultDefaultRank::kMedium,
         mojom::SearchResultType::kSetting,
         {.setting = mojom::Setting::kPreferredSearchEngine}},
    });
    return tags;
  }

  static constexpr auto tags = std::to_array<SearchConcept>({
      {IDS_OS_SETTINGS_TAG_PREFERRED_SEARCH_ENGINE,
       mojom::kSystemPreferencesSectionPath,
       mojom::SearchResultIcon::kSearch,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kPreferredSearchEngine}},
  });
  return tags;
}

base::span<const SearchConcept> GetQuickAnswersSearchConcepts() {
  static constexpr auto tags = std::to_array<SearchConcept>({
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
  return tags;
}

base::span<const SearchConcept> GetQuickAnswersOnSearchConcepts() {
  static constexpr auto tags = std::to_array<SearchConcept>({
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
  return tags;
}

base::span<const SearchConcept> GetScannerSearchConcepts() {
  static constexpr auto tags = std::to_array<SearchConcept>({
      {IDS_OS_SETTINGS_TAG_SUGGESTED_ACTIONS,
       mojom::kSystemPreferencesSectionPath,
       mojom::SearchResultIcon::kScannerActions,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kScannerOnOff}},
  });
  return tags;
}

base::span<const SearchConcept> GetMagicBoostSearchConcepts() {
  static constexpr auto tags = std::to_array<SearchConcept>({
      {IDS_OS_SETTINGS_TAG_MAGIC_BOOST,
       mojom::kSystemPreferencesSectionPath,
       mojom::SearchResultIcon::kMagicBoost,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kMagicBoostOnOff}},
  });
  return tags;
}

base::span<const SearchConcept> GetMagicBoostSubSearchConcepts() {
  static constexpr auto tags = std::to_array<SearchConcept>({
      {IDS_OS_SETTINGS_TAG_MAGIC_BOOST_HMR,
       mojom::kSystemPreferencesSectionPath,
       mojom::SearchResultIcon::kHelpMeRead,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kMahiOnOff}},
      {IDS_OS_SETTINGS_TAG_MAGIC_BOOST_HMW,
       mojom::kSystemPreferencesSectionPath,
       mojom::SearchResultIcon::kHelpMeWrite,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kShowOrca}},
      {IDS_OS_SETTINGS_TAG_MAGIC_BOOST_LOBSTER,
       mojom::kSystemPreferencesSectionPath,
       mojom::SearchResultIcon::kCreateImage,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kLobsterOnOff}},
  });
  return tags;
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
}  // namespace

SearchSection::SearchSection(Profile* profile,
                             SearchTagRegistry* search_tag_registry)
    : OsSettingsSection(profile, search_tag_registry) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  updater.AddSearchTags(GetSearchPageSearchConcepts());
  if (IsScannerSettingsToggleVisible()) {
    updater.AddSearchTags(GetScannerSearchConcepts());
  }

  if (QuickAnswersState::Get()) {
    QuickAnswersState::Get()->AddObserver(this);
    UpdateQuickAnswersSearchTags();
  }

  // Magic boost related search tags are updated in `AddLoadTimeData`, i.e. when
  // the settings app is opened. See `AddLoadTimeData`.
  auto* magic_boost_state = chromeos::MagicBoostState::Get();
  if (magic_boost_state) {
    magic_boost_state->AddObserver(this);
  }
}

SearchSection::~SearchSection() = default;

void SearchSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
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
      {"enableLobster", IDS_LOBSTER_OS_SETTINGS_ENABLE},
      {"enableLobsterDesc", IDS_LOBSTER_OS_SETTINGS_ENABLE_DESCRIPTION},
      {"enableScanner", IDS_OS_SETTINGS_ENABLE_SCANNER},
      {"enableScannerDesc", IDS_OS_SETTINGS_ENABLE_SCANNER_DESCRIPTION},
      {"osSearchEngineLabel", IDS_OS_SETTINGS_SEARCH_ENGINE_LABEL},
      {"searchSubpageTitle", IDS_SETTINGS_SEARCH_SUBPAGE_TITLE},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString("helpMeReadWriteLearnMoreUrl",
                         chrome::kHelpMeReadWriteLearnMoreURL);

  html_source->AddString("lobsterLearnMoreUrl", chrome::kLobsterLearnMoreURL);

  html_source->AddString("scannerLearnMoreUrl", chrome::kScannerLearnMoreUrl);

  html_source->AddBoolean("isQuickAnswersSupported", IsQuickAnswersSupported());

  bool is_magic_boost_feature_enabled =
      chromeos::MagicBoostState::Get()->IsUserEligibleForGenAIFeatures() ||
      IsLobsterSettingsToggleVisible(profile()) ||
      IsOrcaSettingsToggleVisible(profile());

  // Updates magic boost search tags each time when the load time data is added,
  // instead of a one-off update in the constructor, to avoid the unreliable
  // value of chromeos::MagicBoostState::Get()->IsUserEligibleForGenAIFeatures()
  // during the user login. See crbug.com/379281461 for more details.
  UpdateMagicBoostSearchTags(is_magic_boost_feature_enabled);

  html_source->AddBoolean("isMagicBoostFeatureEnabled",
                          is_magic_boost_feature_enabled);

  html_source->AddBoolean("isMagicBoostNoticeBannerVisible",
                          IsMagicBoostNoticeBannerVisible(profile()));

  html_source->AddBoolean("isLobsterSettingsToggleVisible",
                          IsLobsterSettingsToggleVisible(profile()));

  html_source->AddBoolean("isScannerSettingsToggleVisible",
                          IsScannerSettingsToggleVisible());

  html_source->AddLocalizedString("osSearchPageTitle",
                                  IDS_OS_SETTINGS_SEARCH_AND_SUGGESTIONS_TITLE);
  html_source->AddString("osSearchEngineDescription",
                         ui::SubstituteChromeOSDeviceType(
                             IDS_OS_SETTINGS_SEARCH_ENGINE_DESCRIPTION));

  AddQuickAnswersStrings(html_source);
}

void SearchSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<::settings::SearchEnginesHandler>(profile()));
}

int SearchSection::GetSectionNameMessageId() const {
  return IDS_OS_SETTINGS_SEARCH_AND_SUGGESTIONS_TITLE;
}

mojom::Section SearchSection::GetSection() const {
  return mojom::Section::kSystemPreferences;
}

mojom::SearchResultIcon SearchSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kSearch;
}

const char* SearchSection::GetSectionPath() const {
  return mojom::kSystemPreferencesSectionPath;
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

    case mojom::Setting::kLobsterOnOff:
      base::UmaHistogramBoolean("ChromeOS.Settings.MagicBoost.LobsterEnabled",
                                value.GetBool());
      return true;

    case mojom::Setting::kSunfishOnOff:
      base::UmaHistogramBoolean("ChromeOS.Settings.SunfishEnabled",
                                value.GetBool());
      return true;

    case mojom::Setting::kScannerOnOff:
      base::UmaHistogramBoolean("ChromeOS.Settings.ScannerEnabled",
                                value.GetBool());
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

  generator->RegisterTopLevelSetting(mojom::Setting::kShowOrca);
  generator->RegisterTopLevelSetting(mojom::Setting::kMahiOnOff);
  generator->RegisterTopLevelSetting(mojom::Setting::kMagicBoostOnOff);
  generator->RegisterTopLevelSetting(mojom::Setting::kLobsterOnOff);
  generator->RegisterTopLevelSetting(mojom::Setting::kSunfishOnOff);
  generator->RegisterTopLevelSetting(mojom::Setting::kScannerOnOff);

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
}

void SearchSection::OnSettingsEnabled(bool enabled) {
  UpdateQuickAnswersSearchTags();
}

void SearchSection::OnEligibilityChanged(bool eligible) {
  UpdateQuickAnswersSearchTags();
}

void SearchSection::OnFeatureTypeChanged() {
  UpdateQuickAnswersSearchTags();
}

void SearchSection::OnMagicBoostEnabledUpdated(bool enabled) {
  // This is triggered on magic boost prefs value changes, which means magic
  // boost must be available.
  UpdateMagicBoostSearchTags(/*is_magic_boost_available=*/true);
}

void SearchSection::OnIsDeleting() {
  magic_boost_state_observation_.Reset();
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

void SearchSection::UpdateMagicBoostSearchTags(bool is_magic_boost_available) {
  auto* magic_boost_state = chromeos::MagicBoostState::Get();
  DCHECK(magic_boost_state);

  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  updater.RemoveSearchTags(GetMagicBoostSearchConcepts());
  updater.RemoveSearchTags(GetMagicBoostSubSearchConcepts());

  if (!is_magic_boost_available) {
    return;
  }

  updater.AddSearchTags(GetMagicBoostSearchConcepts());

  if (magic_boost_state->magic_boost_enabled().value_or(false)) {
    updater.AddSearchTags(GetMagicBoostSubSearchConcepts());
  }
}

}  // namespace ash::settings
