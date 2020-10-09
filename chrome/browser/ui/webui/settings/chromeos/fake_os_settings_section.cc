// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/fake_os_settings_section.h"

#include <sstream>

#include "chrome/grit/generated_resources.h"

namespace chromeos {
namespace settings {

FakeOsSettingsSection::FakeOsSettingsSection(mojom::Section section)
    : section_(section) {}

FakeOsSettingsSection::~FakeOsSettingsSection() = default;

int FakeOsSettingsSection::GetSectionNameMessageId() const {
  return IDS_INTERNAL_APP_SETTINGS;
}

mojom::Section FakeOsSettingsSection::GetSection() const {
  return section_;
}

mojom::SearchResultIcon FakeOsSettingsSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kWifi;
}

std::string FakeOsSettingsSection::GetSectionPath() const {
  return std::string();
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

}  // namespace settings
}  // namespace chromeos
