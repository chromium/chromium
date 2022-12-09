// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/reset_section.h"

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/management_utils.h"
#include "chrome/browser/ui/webui/settings/ash/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/settings/reset_settings_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kResetSectionPath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

const std::vector<SearchConcept>& GetResetSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_RESET,
       mojom::kResetSectionPath,
       mojom::SearchResultIcon::kReset,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kReset}},
      {IDS_OS_SETTINGS_TAG_RESET_POWERWASH,
       mojom::kResetSectionPath,
       mojom::SearchResultIcon::kReset,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kPowerwash},
       {IDS_OS_SETTINGS_TAG_RESET_POWERWASH_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

bool IsPowerwashAllowed() {
  return !policy::IsDeviceEnterpriseManaged() &&
         !user_manager::UserManager::Get()->IsLoggedInAsGuest() &&
         !user_manager::UserManager::Get()->IsLoggedInAsChildUser();
}

}  // namespace

ResetSection::ResetSection(Profile* profile,
                           SearchTagRegistry* search_tag_registry)
    : OsSettingsSection(profile, search_tag_registry) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  if (IsPowerwashAllowed())
    updater.AddSearchTags(GetResetSearchConcepts());
}

ResetSection::~ResetSection() = default;

void ResetSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"resetPageTitle", IDS_SETTINGS_RESET_TITLE},
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
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddBoolean("allowPowerwash", IsPowerwashAllowed());

  html_source->AddBoolean(
      "showResetProfileBanner",
      ::settings::ResetSettingsHandler::ShouldShowResetProfileBanner(
          profile()));

  html_source->AddString(
      "powerwashDescription",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_FACTORY_RESET_DESCRIPTION,
                                 l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
}

void ResetSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<::settings::ResetSettingsHandler>(profile()));
}

int ResetSection::GetSectionNameMessageId() const {
  return IDS_SETTINGS_RESET;
}

mojom::Section ResetSection::GetSection() const {
  return mojom::Section::kReset;
}

mojom::SearchResultIcon ResetSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kReset;
}

std::string ResetSection::GetSectionPath() const {
  return mojom::kResetSectionPath;
}

bool ResetSection::LogMetric(mojom::Setting setting, base::Value& value) const {
  // Unimplemented.
  return false;
}

void ResetSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSetting(mojom::Setting::kPowerwash);
}

}  // namespace ash::settings
