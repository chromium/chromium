// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/test_support/fake_os_settings_sections.h"

#include <optional>

#include "ash/webui/settings/public/constants/routes.mojom-shared.h"
#include "base/rand_util.h"
#include "chrome/browser/ui/webui/ash/settings/constants/constants_util.h"
#include "chrome/browser/ui/webui/ash/settings/test_support/fake_os_settings_section.h"

namespace ash::settings {

FakeOsSettingsSections::FakeOsSettingsSections() {
  for (const auto& section : AllSections()) {
    auto fake_section = std::make_unique<FakeOsSettingsSection>(section);
    sections_map_[section] = fake_section.get();
    sections_.push_back(std::move(fake_section));
  }
}

FakeOsSettingsSections::~FakeOsSettingsSections() = default;

void FakeOsSettingsSections::FillWithFakeSettings() {
  std::vector<chromeos::settings::mojom::Subpage> shuffled_subpages =
      AllSubpages();
  base::RandomShuffle(shuffled_subpages.begin(), shuffled_subpages.end());

  std::vector<chromeos::settings::mojom::Setting> shuffled_settings =
      AllSettings();
  base::RandomShuffle(shuffled_settings.begin(), shuffled_settings.end());

  auto subpage_it = shuffled_subpages.begin();
  auto setting_it = shuffled_settings.begin();

  for (const auto& section : AllSections()) {
    auto* fake_section =
        static_cast<FakeOsSettingsSection*>(sections_map_[section]);
    // one subpage with one setting.
    fake_section->AddSubpageAndSetting(*subpage_it, *setting_it);
    // one setting directly on the section.
    fake_section->AddSubpageAndSetting(std::nullopt, *setting_it);
    subpage_it++;
    setting_it++;
  }
}

}  // namespace ash::settings
