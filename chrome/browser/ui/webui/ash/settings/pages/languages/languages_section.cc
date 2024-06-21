// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/languages/languages_section.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/input_method/editor_mediator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/webui/ash/settings/os_settings_features_util.h"
#include "chrome/browser/ui/webui/ash/settings/pages/device/inputs_section.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/settings/languages_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kAppLanguagesSubpagePath;
using ::chromeos::settings::mojom::kLanguagesAndInputSectionPath;
using ::chromeos::settings::mojom::kLanguagesSubpagePath;
using ::chromeos::settings::mojom::kSystemPreferencesSectionPath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

const std::vector<SearchConcept>& GetLanguagesPageSearchConceptsV2() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_LANGUAGES,
       mojom::kLanguagesSubpagePath,
       mojom::SearchResultIcon::kLanguage,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kLanguages}},
      {IDS_OS_SETTINGS_TAG_LANGUAGES_CHANGE_DEVICE_LANGUAGE,
       mojom::kLanguagesSubpagePath,
       mojom::SearchResultIcon::kLanguage,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kChangeDeviceLanguage}},
      {IDS_OS_SETTINGS_TAG_LANGUAGES_INPUT_ADD_LANGUAGE,
       mojom::kLanguagesSubpagePath,
       mojom::SearchResultIcon::kLanguage,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAddLanguage}},
      {IDS_OS_SETTINGS_TAG_LANGUAGES_OFFER_TRANSLATION,
       mojom::kLanguagesSubpagePath,
       mojom::SearchResultIcon::kLanguage,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kOfferTranslation}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetAppLanguagesPageSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_LANGUAGES_APP_LANGUAGES,
       mojom::kAppLanguagesSubpagePath,
       mojom::SearchResultIcon::kLanguage,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kAppLanguages}},
  });
  return *tags;
}

void AddLanguagesPageStringsV2(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"deviceLanguageTitle", IDS_OS_SETTINGS_LANGUAGES_DEVICE_LANGUAGE_TITLE},
      {"changeDeviceLanguageLabel",
       IDS_OS_SETTINGS_LANGUAGES_CHANGE_DEVICE_LANGUAGE_BUTTON_LABEL},
      {"changeDeviceLanguageButtonDescription",
       IDS_OS_SETTINGS_LANGUAGES_CHANGE_DEVICE_LANGUAGE_BUTTON_DESCRIPTION},
      {"languagesPreferenceTitle",
       IDS_OS_SETTINGS_LANGUAGES_LANGUAGES_PREFERENCE_TITLE},
      {"websiteLanguagesTitle",
       IDS_OS_SETTINGS_LANGUAGES_WEBSITE_LANGUAGES_TITLE},
      {"offerToTranslateThisLanguage",
       IDS_OS_SETTINGS_LANGUAGES_OFFER_TO_TRANSLATE_THIS_LANGUAGE},
      {"offerTranslationLabel",
       IDS_OS_SETTINGS_LANGUAGES_OFFER_TRANSLATION_LABEL},
      {"offerTranslationSublabel",
       IDS_OS_SETTINGS_LANGUAGES_OFFER_TRANSLATION_SUBLABEL},
      {"offerGoogleTranslateLabel",
       IDS_OS_SETTINGS_LANGUAGES_OFFER_GOOGLE_TRANSLATE_LABEL},
      {"changeDeviceLanguageDialogTitle",
       IDS_OS_SETTINGS_LANGUAGES_CHANGE_DEVICE_LANGUAGE_DIALOG_TITLE},
      {"selectedDeviceLanguageInstruction",
       IDS_OS_SETTINGS_LANGUAGES_SELECTED_DEVICE_LANGUAGE_INSTRUCTION},
      {"notSelectedDeviceLanguageInstruction",
       IDS_OS_SETTINGS_LANGUAGES_NOT_SELECTED_DEVICE_LANGUAGE_INSTRUCTION},
      {"changeDeviceLanguageConfirmButtonLabel",
       IDS_OS_SETTINGS_LANGUAGES_CHANGE_DEVICE_LANGUAGE_CONFIRM_BUTTON_LABEL},
      {"googleAccountLanguageTitle",
       IDS_OS_SETTINGS_LANGUAGES_GOOGLE_ACCOUNT_LANGUAGE_TITLE},
      {"googleAccountLanguageDescription",
       IDS_OS_SETTINGS_LANGUAGES_GOOGLE_ACCOUNT_LANGUAGE_DESCRIPTION},
      {"manageGoogleAccountLanguageLabel",
       IDS_OS_SETTINGS_LANGUAGES_MANAGE_GOOGLE_ACCOUNT_LANGUAGE_LABEL},
      {"appLanguagesTitle", IDS_OS_SETTINGS_LANGUAGES_APP_LANGUAGES_TITLE},
      {"appLanguagesDescription",
       IDS_OS_SETTINGS_LANGUAGES_APP_LANGUAGES_DESCRIPTION},
      {"appLanguagesSupportedAppsDescription",
       IDS_OS_SETTINGS_LANGUAGES_APP_LANGUAGES_SUPPORTED_APPS_DESCRIPTION},
      {"appLanguagesNoAppsSupportedDescription",
       IDS_OS_SETTINGS_LANGUAGES_APP_LANGUAGES_NO_APPS_SUPPORTED_DESCRIPTION},
      {"appLanguagesEditSelectionLabel",
       IDS_OS_SETTINGS_LANGUAGES_APP_LANGUAGES_EDIT_SELECTION_LABEL},
      {"appLanguagesResetSelectionLabel",
       IDS_OS_SETTINGS_LANGUAGES_APP_LANGUAGES_RESET_SELECTION_LABEL},
      {"appLanguagesChangeLanguageButtonDescription",
       IDS_OS_SETTINGS_LANGUAGES_APP_LANGUAGES_CHANGE_LANGUAGE_BUTTON_DESCRIPTION},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString(
      "languagesPreferenceDescription",
      l10n_util::GetStringFUTF16(
          IDS_OS_SETTINGS_LANGUAGES_LANGUAGES_PREFERENCE_DESCRIPTION,
          chrome::kLanguageSettingsLearnMoreUrl));
  html_source->AddString(
      "websiteLanguagesDescription",
      l10n_util::GetStringFUTF16(
          IDS_OS_SETTINGS_LANGUAGES_WEBSITE_LANGUAGES_DESCRIPTION,
          chrome::kLanguageSettingsLearnMoreUrl));
  html_source->AddString(
      "translateTargetLabel",
      l10n_util::GetStringUTF16(
          QuickAnswersState::IsEligibleAs(
              QuickAnswersState::FeatureType::kQuickAnswers)
              ? IDS_OS_SETTINGS_LANGUAGES_TRANSLATE_TARGET_LABEL_WITH_QUICK_ANSWERS
              : IDS_OS_SETTINGS_LANGUAGES_TRANSLATE_TARGET_LABEL));
  html_source->AddString(
      "changeDeviceLanguageDialogDescription",
      l10n_util::GetStringFUTF16(
          IDS_OS_SETTINGS_LANGUAGES_CHANGE_DEVICE_LANGUAGE_DIALOG_DESCRIPTION,
          chrome::kLanguageSettingsLearnMoreUrl));

  html_source->AddString(
      "googleAccountLanguagesURL",
      net::AppendQueryParameter(GURL(chrome::kGoogleAccountLanguagesURL),
                                "utm_source", "chrome-settings")
          .spec());
}

}  // namespace

LanguagesSection::LanguagesSection(Profile* profile,
                                   SearchTagRegistry* search_tag_registry,
                                   PrefService* pref_service)
    : OsSettingsSection(profile, search_tag_registry),
      inputs_subsection_(
          !ash::features::IsOsSettingsRevampWayfindingEnabled()
              ? std::make_optional<InputsSection>(
                    profile,
                    search_tag_registry,
                    pref_service,
                    chromeos::features::IsOrcaEnabled()
                        ? input_method::EditorMediatorFactory::GetInstance()
                              ->GetForProfile(profile)
                        : nullptr)
              : std::nullopt) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.AddSearchTags(GetLanguagesPageSearchConceptsV2());
  if (IsPerAppLanguageEnabled(profile)) {
    updater.AddSearchTags(GetAppLanguagesPageSearchConcepts());
  }
}

LanguagesSection::~LanguagesSection() = default;

void LanguagesSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  webui::LocalizedString kLocalizedStrings[] = {
      {"osLanguagesPageTitle", IDS_OS_SETTINGS_LANGUAGES_AND_INPUT_PAGE_TITLE},
      {"languagesPageTitle", IDS_OS_SETTINGS_LANGUAGES_LANGUAGES_PAGE_TITLE},
      {"moveDown", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_DOWN},
      {"searchLanguages", IDS_SETTINGS_LANGUAGE_SEARCH},
      {"addLanguagesDialogTitle",
       IDS_SETTINGS_LANGUAGES_MANAGE_LANGUAGES_TITLE},
      {"moveToTop", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_TO_TOP},
      {"removeLanguage", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_REMOVE},
      {"addLanguages", IDS_SETTINGS_LANGUAGES_LANGUAGES_ADD},
      {"moveUp", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_UP},
      {"noSearchResults", IDS_SEARCH_NO_RESULTS},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
  html_source->AddBoolean("isPerAppLanguageEnabled",
                          IsPerAppLanguageEnabled(profile()));

  AddLanguagesPageStringsV2(html_source);

  // Inputs subsection exists only when the OsSettingsRevampWayfinding feature
  // is disabled. It is part of the Device section when the feature is enabled.
  if (!ash::features::IsOsSettingsRevampWayfindingEnabled()) {
    CHECK(inputs_subsection_);
    inputs_subsection_->AddLoadTimeData(html_source);
  }
}

void LanguagesSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<::settings::LanguagesHandler>(profile()));
}

int LanguagesSection::GetSectionNameMessageId() const {
  return IDS_OS_SETTINGS_LANGUAGES_AND_INPUT_PAGE_TITLE;
}

mojom::Section LanguagesSection::GetSection() const {
  return ash::features::IsOsSettingsRevampWayfindingEnabled()
             ? mojom::Section::kSystemPreferences
             : mojom::Section::kLanguagesAndInput;
}

mojom::SearchResultIcon LanguagesSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kLanguage;
}

const char* LanguagesSection::GetSectionPath() const {
  return ash::features::IsOsSettingsRevampWayfindingEnabled()
             ? mojom::kSystemPreferencesSectionPath
             : mojom::kLanguagesAndInputSectionPath;
}

bool LanguagesSection::LogMetric(mojom::Setting setting,
                                 base::Value& value) const {
  // No metrics are logged.
  return false;
}

void LanguagesSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  // Languages.
  generator->RegisterTopLevelSubpage(
      IDS_OS_SETTINGS_LANGUAGES_LANGUAGES_PAGE_TITLE,
      mojom::Subpage::kLanguages, mojom::SearchResultIcon::kLanguage,
      mojom::SearchResultDefaultRank::kMedium, mojom::kLanguagesSubpagePath);
  static constexpr mojom::Setting kLanguagesPageSettings[] = {
      mojom::Setting::kAddLanguage,
      mojom::Setting::kChangeDeviceLanguage,
      mojom::Setting::kOfferTranslation,
      mojom::Setting::kRemoveLanguage,
      mojom::Setting::kMoveLanguageToFront,
      mojom::Setting::kMoveLanguageUp,
      mojom::Setting::kMoveLanguageDown,
      mojom::Setting::kEnableTranslateLanguage,
      mojom::Setting::kDisableTranslateLanguage,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kLanguages, kLanguagesPageSettings,
                            generator);

  if (IsPerAppLanguageEnabled(profile())) {
    // App language subpage.
    generator->RegisterNestedSubpage(
        IDS_OS_SETTINGS_LANGUAGES_APP_LANGUAGES_TITLE,
        mojom::Subpage::kAppLanguages, mojom::Subpage::kLanguages,
        mojom::SearchResultIcon::kLanguage,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::kAppLanguagesSubpagePath);
  }

  // Inputs subsection exists only when the OsSettingsRevampWayfinding feature
  // is disabled. It is part of the Device section when the feature is enabled.
  if (!ash::features::IsOsSettingsRevampWayfindingEnabled()) {
    CHECK(inputs_subsection_);
    inputs_subsection_->RegisterHierarchy(generator);
  }
}

}  // namespace ash::settings
