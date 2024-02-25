// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/test_support/fake_hierarchy.h"

#include <utility>

#include "chrome/browser/ui/webui/ash/settings/test_support/fake_os_settings_section.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

FakeHierarchy::FakeHierarchy(const OsSettingsSections* sections)
    : Hierarchy(sections) {}

FakeHierarchy::~FakeHierarchy() = default;

void FakeHierarchy::AddSubpageMetadata(
    int name_message_id,
    mojom::Section section,
    mojom::Subpage subpage,
    mojom::SearchResultIcon icon,
    mojom::SearchResultDefaultRank default_rank,
    const std::string& url_path_with_parameters,
    std::optional<mojom::Subpage> parent_subpage) {
  auto pair = subpage_map_.emplace(
      std::piecewise_construct, std::forward_as_tuple(subpage),
      std::forward_as_tuple(name_message_id, section, subpage, icon,
                            default_rank, url_path_with_parameters, this));
  DCHECK(pair.second);
  pair.first->second.parent_subpage = parent_subpage;
}

void FakeHierarchy::AddSettingMetadata(
    mojom::Section section,
    mojom::Setting setting,
    std::optional<mojom::Subpage> parent_subpage) {
  auto pair = setting_map_.emplace(setting, section);
  DCHECK(pair.second);
  pair.first->second.primary.subpage = parent_subpage;
}

std::string FakeHierarchy::ModifySearchResultUrl(
    mojom::Section section,
    mojom::SearchResultType type,
    OsSettingsIdentifier id,
    const std::string& url_to_modify) const {
  return FakeOsSettingsSection::ModifySearchResultUrl(section, url_to_modify);
}

}  // namespace ash::settings
