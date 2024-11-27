// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/reset/reset_section.h"

#include <array>

#include "ash/constants/ash_features.h"
#include "base/containers/span.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/settings/os_settings_features_util.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/settings/reset_settings_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kSystemPreferencesSectionPath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {
base::span<const SearchConcept> GetResetSearchConcept() {
  static constexpr auto tags = std::to_array<SearchConcept>({
      {IDS_OS_SETTINGS_TAG_RESET,
       mojom::kSystemPreferencesSectionPath,
       mojom::SearchResultIcon::kReset,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kPowerwash}},
  });

  return tags;
}
}  // namespace

ResetSection::ResetSection(Profile* profile,
                           SearchTagRegistry* search_tag_registry)
    : OsSettingsSection(profile, search_tag_registry) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  auto* user = BrowserContextHelper::Get()->GetUserByBrowserContext(profile);
  if (IsPowerwashAllowed(user)) {
    updater.AddSearchTags(GetResetSearchConcept());

    updater.AddSearchTags(GetPowerwashSearchConcept());
  }
}

ResetSection::~ResetSection() = default;

void ResetSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"resetPageTitle", IDS_OS_SETTINGS_REVAMP_RESET_TITLE},
      {"powerwashTitle", IDS_SETTINGS_FACTORY_RESET},
      {"powerwashDialogTitle", IDS_SETTINGS_FACTORY_RESET_HEADING},
      {"powerwashDialogButton", IDS_SETTINGS_RESTART},
      {"powerwashButton", IDS_SETTINGS_FACTORY_RESET_BUTTON_LABEL},
      {"powerwashDialogExplanation", IDS_SETTINGS_FACTORY_RESET_WARNING},
      {"powerwashLearnMoreUrl", IDS_FACTORY_RESET_HELP_URL},
      {"powerwashButtonRoleDescription",
       IDS_SETTINGS_FACTORY_RESET_BUTTON_ROLE},
      {"powerwashDialogESimWarningTitle",
       IDS_SETTINGS_FACTORY_RESET_ESIM_WARNING_TITLE},
      {"powerwashDialogESimWarning", IDS_SETTINGS_FACTORY_RESET_ESIM_WARNING},
      {"powerwashDialogESimListTitle",
       IDS_SETTINGS_FACTORY_RESET_ESIM_LIST_TITLE},
      {"powerwashDialogESimListItemTitle",
       IDS_SETTINGS_FACTORY_RESET_ESIM_LIST_ITEM_TITLE},
      {"powerwashDialogESimWarningCheckbox",
       IDS_SETTINGS_FACTORY_RESET_ESIM_WARNING_CHECKBOX_LABEL},
      {"powerwashContinue", IDS_SETTINGS_FACTORY_CONTINUE_BUTTON_LABEL},
      {"sanitizeTitle", IDS_OS_SETTINGS_SANITIZE},
      {"sanitizeDialogTitle", IDS_OS_SETTINGS_SANITIZE_HEADING},
      {"sanitizeFeedback", IDS_OS_SETTINGS_SANITIZE_FEEDBACK},
      {"sanitizeDialogButton", IDS_OS_SETTINGS_SANITIZE},
      {"sanitizeButton", IDS_OS_SETTINGS_SANITIZE},
      {"sanitizeShortDescription", IDS_OS_SETTINGS_SANITIZE_SHORT_DESCRIPTION},
      {"sanitizeDescription", IDS_OS_SETTINGS_SANITIZE_DESCRIPTION},
      {"sanitizeDialogExplanation", IDS_OS_SETTINGS_SANITIZE_WARNING},
      {"sanitizeLearnMoreUrl", IDS_SANITIZE_HELP_URL},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  auto* user = BrowserContextHelper::Get()->GetUserByBrowserContext(profile());
  html_source->AddBoolean("allowPowerwash", IsPowerwashAllowed(user));
  html_source->AddBoolean("allowSanitize", IsSanitizeAllowed(user));

  html_source->AddBoolean(
      "showResetProfileBanner",
      ::settings::ResetSettingsHandler::ShouldShowResetProfileBanner(
          profile()));

  html_source->AddString("powerwashDescription",
                         l10n_util::GetStringUTF16(
                             IDS_OS_SETTINGS_REVAMP_FACTORY_RESET_DESCRIPTION));
}

void ResetSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<::settings::ResetSettingsHandler>(profile()));
}

int ResetSection::GetSectionNameMessageId() const {
  return IDS_SETTINGS_RESET;
}

mojom::Section ResetSection::GetSection() const {
  return mojom::Section::kSystemPreferences;
}

mojom::SearchResultIcon ResetSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kReset;
}

const char* ResetSection::GetSectionPath() const {
  return mojom::kSystemPreferencesSectionPath;
}

bool ResetSection::LogMetric(mojom::Setting setting, base::Value& value) const {
  // Unimplemented.
  return false;
}

void ResetSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSetting(mojom::Setting::kPowerwash);
  generator->RegisterTopLevelSetting(mojom::Setting::kSanitizeCrosSettings);
}

base::span<const SearchConcept> ResetSection::GetPowerwashSearchConcept() {
  static constexpr auto tags = std::to_array<SearchConcept>({
      {IDS_OS_SETTINGS_TAG_RESET_POWERWASH,
       mojom::kSystemPreferencesSectionPath,
       mojom::SearchResultIcon::kReset,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kPowerwash},
       {IDS_OS_SETTINGS_TAG_RESET_POWERWASH_ALT1, SearchConcept::kAltTagEnd}},
  });

  return tags;
}

}  // namespace ash::settings
