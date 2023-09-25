// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/personalization_section.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/settings/ash/personalization_hub_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kPersonalizationSectionPath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
}  // namespace mojom

PersonalizationSection::PersonalizationSection(
    Profile* profile,
    SearchTagRegistry* search_tag_registry,
    PrefService* pref_service)
    : OsSettingsSection(profile, search_tag_registry),
      isRevampEnabled_(ash::features::IsOsSettingsRevampWayfindingEnabled()) {}

PersonalizationSection::~PersonalizationSection() = default;

void PersonalizationSection::AddLoadTimeData(
    content::WebUIDataSource* html_source) {
  webui::LocalizedString kLocalizedStrings[] = {
      {"personalizationPageTitle", isRevampEnabled_
                                       ? IDS_OS_SETTINGS_REVAMP_PERSONALIZATION
                                       : IDS_OS_SETTINGS_PERSONALIZATION},
      {"personalizationMenuItemDescription",
       IDS_OS_SETTINGS_PERSONALIZATION_MENU_ITEM_DESCRIPTION},
      {"personalizationHubTitle",
       isRevampEnabled_ ? IDS_OS_SETTINGS_REVAMP_OPEN_PERSONALIZATION_HUB
                        : IDS_OS_SETTINGS_OPEN_PERSONALIZATION_HUB},
      {"personalizationHubSubtitle",
       IDS_OS_SETTINGS_OPEN_PERSONALIZATION_HUB_SUBTITLE},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

void PersonalizationSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(std::make_unique<PersonalizationHubHandler>());
}

int PersonalizationSection::GetSectionNameMessageId() const {
  return isRevampEnabled_ ? IDS_OS_SETTINGS_REVAMP_PERSONALIZATION
                          : IDS_OS_SETTINGS_PERSONALIZATION;
}

mojom::Section PersonalizationSection::GetSection() const {
  return mojom::Section::kPersonalization;
}

mojom::SearchResultIcon PersonalizationSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kPaintbrush;
}

const char* PersonalizationSection::GetSectionPath() const {
  return mojom::kPersonalizationSectionPath;
}

bool PersonalizationSection::LogMetric(mojom::Setting setting,
                                       base::Value& value) const {
  // Unimplemented.
  return false;
}

void PersonalizationSection::RegisterHierarchy(
    HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSetting(mojom::Setting::kOpenWallpaper);
}

}  // namespace ash::settings
