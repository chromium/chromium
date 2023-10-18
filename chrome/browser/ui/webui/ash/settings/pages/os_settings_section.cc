// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/os_settings_section.h"

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

// static
constexpr const char OsSettingsSection::kSettingIdUrlParam[];

// static
std::u16string OsSettingsSection::GetHelpUrlWithBoard(
    const std::string& original_url) {
  return base::ASCIIToUTF16(original_url +
                            "&b=" + base::SysInfo::GetLsbReleaseBoard());
}

// static
void OsSettingsSection::RegisterNestedSettingBulk(
    mojom::Subpage subpage,
    const base::span<const mojom::Setting>& settings,
    HierarchyGenerator* generator) {
  for (const auto& setting : settings)
    generator->RegisterNestedSetting(setting, subpage);
}

OsSettingsSection::~OsSettingsSection() = default;

OsSettingsSection::OsSettingsSection(Profile* profile,
                                     SearchTagRegistry* search_tag_registry)
    : profile_(profile), search_tag_registry_(search_tag_registry) {
  DCHECK(profile);
  DCHECK(search_tag_registry);
}

OsSettingsSection::OsSettingsSection() = default;

std::string OsSettingsSection::ModifySearchResultUrl(
    mojom::SearchResultType type,
    OsSettingsIdentifier id,
    const std::string& url_to_modify) const {
  return GetDefaultModifiedUrl(type, id, url_to_modify);
}

mojom::SearchResultPtr OsSettingsSection::GenerateSectionSearchResult(
    double relevance_score) const {
  return mojom::SearchResult::New(
      /*text=*/l10n_util::GetStringUTF16(GetSectionNameMessageId()),
      /*canonical_text=*/
      l10n_util::GetStringUTF16(GetSectionNameMessageId()),
      ModifySearchResultUrl(mojom::SearchResultType::kSection,
                            {.section = GetSection()}, GetSectionPath()),
      GetSectionIcon(), relevance_score,
      std::vector<std::u16string>{
          l10n_util::GetStringUTF16(IDS_INTERNAL_APP_SETTINGS),
          l10n_util::GetStringUTF16(GetSectionNameMessageId())},
      mojom::SearchResultDefaultRank::kMedium,
      /*was_generated_from_text_match=*/false,
      mojom::SearchResultType::kSection,
      mojom::SearchResultIdentifier::NewSection(GetSection()));
}

// static
std::string OsSettingsSection::GetDefaultModifiedUrl(
    mojom::SearchResultType type,
    OsSettingsIdentifier id,
    const std::string& url_to_modify) {
  // Default case for static URLs which do not need to be modified.
  if (type != mojom::SearchResultType::kSetting)
    return url_to_modify;

  std::stringstream ss;
  ss << url_to_modify;
  // Handle existing query parameters.
  if (url_to_modify.find('?') == std::string::npos) {
    ss << '?';
  } else {
    ss << '&';
  }
  // Add deep link to query i.e. "settingId=4".
  ss << kSettingIdUrlParam << '=' << static_cast<int32_t>(id.setting);
  return ss.str();
}

}  // namespace ash::settings
