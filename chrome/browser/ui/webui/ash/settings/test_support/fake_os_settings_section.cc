// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/test_support/fake_os_settings_section.h"

#include <optional>
#include <sstream>

#include "ash/webui/settings/public/constants/routes.mojom-shared.h"
#include "base/containers/contains.h"
#include "chrome/grit/generated_resources.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

FakeOsSettingsSection::FakeOsSettingsSection(mojom::Section section)
    : section_(section) {}

FakeOsSettingsSection::~FakeOsSettingsSection() = default;

void FakeOsSettingsSection::AddSubpageAndSetting(
    std::optional<chromeos::settings::mojom::Subpage> subpage,
    std::optional<chromeos::settings::mojom::Setting> setting) {
  if (subpage) {
    if (!base::Contains(subpages_, subpage.value())) {
      // This is a new `subpage` and settings on this subpage.
      subpages_.insert({subpage.value(), {}});
    }

    if (setting) {
      subpages_[subpage.value()].push_back(setting.value());
    }
  } else {
    // `subpage` is empty, this is a setting directly on the section.
    CHECK(setting);
    settings_.push_back(setting.value());
  }
}

void FakeOsSettingsSection::RegisterHierarchy(
    HierarchyGenerator* generator) const {
  for (auto& it : subpages_) {
    generator->RegisterTopLevelSubpage(
        /* Arbitrary title string */ IDS_SETTINGS_KERBEROS_ACCOUNTS_PAGE_TITLE,
        it.first,
        /* Arbitrary icon */ mojom::SearchResultIcon::kAuthKey,
        mojom::SearchResultDefaultRank::kMedium,
        "fake/url_path_with_parameters");
    RegisterNestedSettingBulk(it.first, it.second, generator);
  }

  for (auto& it : settings_) {
    generator->RegisterTopLevelAltSetting(it);
  }
}

int FakeOsSettingsSection::GetSectionNameMessageId() const {
  return IDS_INTERNAL_APP_SETTINGS;
}

mojom::Section FakeOsSettingsSection::GetSection() const {
  return section_;
}

mojom::SearchResultIcon FakeOsSettingsSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kWifi;
}

const char* FakeOsSettingsSection::GetSectionPath() const {
  return "";
}

bool FakeOsSettingsSection::LogMetric(mojom::Setting setting,
                                      base::Value& value) const {
  logged_metrics_.push_back(setting);
  return true;
}

std::string FakeOsSettingsSection::ModifySearchResultUrl(
    mojom::SearchResultType type,
    OsSettingsIdentifier id,
    const std::string& url_to_modify) const {
  return ModifySearchResultUrl(section_, url_to_modify);
}

// static
std::string FakeOsSettingsSection::ModifySearchResultUrl(
    mojom::Section section,
    const std::string& url_to_modify) {
  std::stringstream ss;
  ss << section << "::" << url_to_modify;
  return ss.str();
}

}  // namespace ash::settings
