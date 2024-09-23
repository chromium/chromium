// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/multitasking/multitasking_section.h"

#include "ash/constants/ash_features.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/webui/ash/settings/os_settings_features_util.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash::settings {

namespace mojom {
using chromeos::settings::mojom::kPersonalizationSectionPath;
using chromeos::settings::mojom::kSystemPreferencesSectionPath;
using chromeos::settings::mojom::Section;
using chromeos::settings::mojom::Setting;
using chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

const std::vector<SearchConcept>& GetSnapWindowSuggestionsSearchConcepts(
    const char* section_path) {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_MULTITASKING_SNAP_WINDOW,
       section_path,
       mojom::SearchResultIcon::kSnapWindowSuggestions,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSnapWindowSuggestions},
       {IDS_OS_SETTINGS_TAG_MULTITASKING_SNAP_WINDOW_ALT1,
        IDS_OS_SETTINGS_TAG_MULTITASKING_SNAP_WINDOW_ALT2,
        IDS_OS_SETTINGS_TAG_MULTITASKING_SNAP_WINDOW_ALT3,
        IDS_OS_SETTINGS_TAG_MULTITASKING_SNAP_WINDOW_ALT4,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

}  // namespace

MultitaskingSection::MultitaskingSection(Profile* profile,
                                         SearchTagRegistry* search_tag_registry)
    : OsSettingsSection(profile, search_tag_registry) {
  CHECK(profile);
  CHECK(search_tag_registry);

  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.AddSearchTags(
      GetSnapWindowSuggestionsSearchConcepts(GetSectionPath()));
}

MultitaskingSection::~MultitaskingSection() = default;

void MultitaskingSection::AddLoadTimeData(
    content::WebUIDataSource* html_source) {
  webui::LocalizedString kLocalizedStrings[] = {
      {"multitaskingSettingsCardTitle",
       IDS_OS_SETTINGS_SYSTEM_PREFERENCES_MULTITASKING_TITLE},
      {"snapWindowLabel",
       IDS_OS_SETTINGS_SYSTEM_PREFERENCES_MULTITASKING_SNAP_WINDOW_LABEL},
      {"snapWindowDescription",
       IDS_OS_SETTINGS_SYSTEM_PREFERENCES_MULTITASKING_SNAP_WINDOW_DESCRIPTION},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
  html_source->AddBoolean("shouldShowMultitasking", ShouldShowMultitasking());
  html_source->AddBoolean("shouldShowMultitaskingInPersonalization",
                          ShouldShowMultitaskingInPersonalization());
}

void MultitaskingSection::AddHandlers(content::WebUI* web_ui) {
  // No handlers registered.
}

int MultitaskingSection::GetSectionNameMessageId() const {
  return IDS_OS_SETTINGS_SYSTEM_PREFERENCES_MULTITASKING_TITLE;
}

mojom::Section MultitaskingSection::GetSection() const {
  // Note: This is a subsection that exists under System Preferences. This is
  // not a top-level section and does not have a respective declaration in
  // chromeos::settings::mojom::Section.
  return ash::features::IsOsSettingsRevampWayfindingEnabled()
             ? mojom::Section::kSystemPreferences
             : mojom::Section::kPersonalization;
}

mojom::SearchResultIcon MultitaskingSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kSnapWindowSuggestions;
}

const char* MultitaskingSection::GetSectionPath() const {
  return ash::features::IsOsSettingsRevampWayfindingEnabled()
             ? mojom::kSystemPreferencesSectionPath
             : mojom::kPersonalizationSectionPath;
}

bool MultitaskingSection::LogMetric(mojom::Setting setting,
                                    base::Value& value) const {
  if (setting == mojom::Setting::kSnapWindowSuggestions) {
    base::UmaHistogramBoolean("ChromeOS.Settings.SnapWindowSuggestions",
                              value.GetBool());
    return true;
  }
  return false;
}

void MultitaskingSection::RegisterHierarchy(
    HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSetting(mojom::Setting::kSnapWindowSuggestions);
}

}  // namespace ash::settings
