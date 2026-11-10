// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/search/settings_sections.h"

#include <memory>
#include <vector>

#include "base/check.h"
#include "chrome/browser/ui/webui/settings/search/appearance_section.h"
#include "chrome/browser/ui/webui/settings/search/settings_section.h"

namespace settings_search {

std::vector<std::unique_ptr<SettingsSection>> CreateSettingsSections(
    SettingsSearchRegistry* registry,
    Profile* profile) {
  CHECK(registry);
  CHECK(profile);
  std::vector<std::unique_ptr<SettingsSection>> sections;
  sections.push_back(std::make_unique<AppearanceSection>(registry, profile));
  return sections;
}

}  // namespace settings_search
