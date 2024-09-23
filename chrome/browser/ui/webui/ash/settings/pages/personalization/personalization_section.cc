// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/personalization/personalization_section.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/settings/os_settings_features_util.h"
#include "chrome/browser/ui/webui/ash/settings/pages/multitasking/multitasking_section.h"
#include "chrome/browser/ui/webui/ash/settings/pages/personalization/personalization_hub_handler.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
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

namespace {

const std::vector<SearchConcept>& GetPersonalizationSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_WALLPAPER_AND_STYLE,
       mojom::kPersonalizationSectionPath,
       mojom::SearchResultIcon::kPersonalization,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kPersonalization}},
  });
  return *tags;
}

}  // namespace

PersonalizationSection::PersonalizationSection(
    Profile* profile,
    SearchTagRegistry* search_tag_registry,
    PrefService* pref_service)
    : OsSettingsSection(profile, search_tag_registry),
      isRevampEnabled_(ash::features::IsOsSettingsRevampWayfindingEnabled()),
      multitasking_subsection_(
          !isRevampEnabled_
              ? std::make_optional<MultitaskingSection>(profile,
                                                        search_tag_registry)
              : std::nullopt) {
  if (isRevampEnabled_) {
    SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
    updater.AddSearchTags(GetPersonalizationSearchConcepts());
  }
}

PersonalizationSection::~PersonalizationSection() = default;

void PersonalizationSection::AddLoadTimeData(
    content::WebUIDataSource* html_source) {
  const bool kIsGuest = IsGuestModeActive();

  webui::LocalizedString kWallpaperLocalizedStrings[] = {
      {"personalizationPageTitle", isRevampEnabled_
                                       ? IDS_OS_SETTINGS_REVAMP_PERSONALIZATION
                                       : IDS_OS_SETTINGS_PERSONALIZATION},
      {"personalizationMenuItemDescription",
       kIsGuest
           ? IDS_OS_SETTINGS_PERSONALIZATION_MENU_ITEM_DESCRIPTION_GUEST_MODE
           : IDS_OS_SETTINGS_PERSONALIZATION_MENU_ITEM_DESCRIPTION},
      {"personalizationHubTitle", IDS_OS_SETTINGS_OPEN_PERSONALIZATION_HUB},
      {"personalizationHubSubtitle",
       isRevampEnabled_
           ? (kIsGuest
                  ? IDS_OS_SETTINGS_REVAMP_OPEN_PERSONALIZATION_HUB_SUBTITLE_GUEST_MODE
                  : IDS_OS_SETTINGS_REVAMP_OPEN_PERSONALIZATION_HUB_SUBTITLE)
           : IDS_OS_SETTINGS_OPEN_PERSONALIZATION_HUB_SUBTITLE},
  };

  html_source->AddLocalizedStrings(kWallpaperLocalizedStrings);
  if (multitasking_subsection_) {
    multitasking_subsection_->AddLoadTimeData(html_source);
  }
}

void PersonalizationSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(std::make_unique<PersonalizationHubHandler>());
  if (multitasking_subsection_) {
    multitasking_subsection_->AddHandlers(web_ui);
  }
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
  if (multitasking_subsection_) {
    multitasking_subsection_->RegisterHierarchy(generator);
  }
}

}  // namespace ash::settings
