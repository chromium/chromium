// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/search/settings_section.h"

#include "base/check.h"

namespace settings_search {

SettingsSection::SettingsSection(SettingsSearchRegistry* registry)
    : registry_(registry) {
  CHECK(registry_);
}

SettingsSection::~SettingsSection() = default;

}  // namespace settings_search
