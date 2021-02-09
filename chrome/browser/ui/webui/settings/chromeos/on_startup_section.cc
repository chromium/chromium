// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/on_startup_section.h"

#include "ash/public/cpp/ash_features.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetOnStartupSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_ON_STARTUP,
       mojom::kOnStartupSectionPath,
       mojom::SearchResultIcon::kStartup,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kOnStartup}},
      {IDS_OS_SETTINGS_TAG_RESTORE_APPS_AND_PAGES,
       mojom::kOnStartupSectionPath,
       mojom::SearchResultIcon::kStartup,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kRestoreAppsAndPages}},
  });
  return *tags;
}

bool ShouldShowStartup() {
  return ash::features::IsFullRestoreEnabled();
}

}  // namespace

OnStartupSection::OnStartupSection(Profile* profile,
                                   SearchTagRegistry* search_tag_registry,
                                   PrefService* pref_service)
    : OsSettingsSection(profile, search_tag_registry) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  if (ShouldShowStartup()) {
    updater.AddSearchTags(GetOnStartupSearchConcepts());
  }
}

OnStartupSection::~OnStartupSection() = default;

void OnStartupSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"onStartupPageTitle", IDS_OS_SETTINGS_ON_STARTUP},
      {"onStartupRadioGroundTitle",
       IDS_OS_SETTINGS_ON_STARTUP_RADIO_GROUP_TITLE},
      {"onStartupAlways", IDS_OS_SETTINGS_ON_STARTUP_ALWAYS},
      {"onStartupAskEveryTime", IDS_OS_SETTINGS_ON_STARTUP_ASK_EVERY_TIME},
      {"onStartupDoNotRestore", IDS_OS_SETTINGS_ON_STARTUP_DO_NOT_RESTORE},
  };

  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddBoolean("showStartup", ShouldShowStartup());
}

int OnStartupSection::GetSectionNameMessageId() const {
  return IDS_OS_SETTINGS_ON_STARTUP;
}

mojom::Section OnStartupSection::GetSection() const {
  return mojom::Section::kOnStartup;
}

mojom::SearchResultIcon OnStartupSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kStartup;
}

std::string OnStartupSection::GetSectionPath() const {
  return mojom::kOnStartupSectionPath;
}

bool OnStartupSection::LogMetric(mojom::Setting setting,
                                 base::Value& value) const {
  // Unimplemented.
  return false;
}

void OnStartupSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSetting(mojom::Setting::kRestoreAppsAndPages);
}

}  // namespace settings
}  // namespace chromeos
