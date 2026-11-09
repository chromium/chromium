// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SETTINGS_SECTIONS_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SETTINGS_SECTIONS_H_

#include <memory>
#include <vector>

class Profile;

namespace settings_search {

class SettingsSearchRegistry;
class SettingsSection;

// Creates the SettingsSection of every page that has searchable settings.
// `registry` and `profile` must outlive the returned sections.
std::vector<std::unique_ptr<SettingsSection>> CreateSettingsSections(
    SettingsSearchRegistry* registry,
    Profile* profile);

}  // namespace settings_search

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SETTINGS_SECTIONS_H_
