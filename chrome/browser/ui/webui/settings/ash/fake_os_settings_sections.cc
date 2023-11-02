// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/fake_os_settings_sections.h"

#include "chrome/browser/ui/webui/settings/ash/fake_os_settings_section.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/constants_util.h"

namespace ash::settings {

FakeOsSettingsSections::FakeOsSettingsSections() : OsSettingsSections() {
  for (const auto& section : constants::AllSections()) {
    auto fake_section = std::make_unique<FakeOsSettingsSection>(section);
    sections_map_[section] = fake_section.get();
    sections_.push_back(std::move(fake_section));
  }
}

FakeOsSettingsSections::~FakeOsSettingsSections() = default;

}  // namespace ash::settings
