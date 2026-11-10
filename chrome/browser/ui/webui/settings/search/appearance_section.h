// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_APPEARANCE_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_APPEARANCE_SECTION_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/settings/search/settings_section.h"

#if BUILDFLAG(IS_LINUX)
#include "components/prefs/pref_change_registrar.h"
#endif

class Profile;

namespace settings_search {

// Searchable settings on the Appearance page and its Fonts subpage. Each item
// is registered under the same conditions that show its control on the page.
class AppearanceSection : public SettingsSection {
 public:
  AppearanceSection(SettingsSearchRegistry* registry, Profile* profile);
  AppearanceSection(const AppearanceSection&) = delete;
  AppearanceSection& operator=(const AppearanceSection&) = delete;
  ~AppearanceSection() override;

 private:
  void RegisterSearchItems();
#if BUILDFLAG(IS_LINUX)
  // The color scheme control is hidden while a GTK or Qt system theme is in
  // use without a custom theme, so its item follows the theme prefs.
  void UpdateColorSchemeModeItem();

  PrefChangeRegistrar pref_change_registrar_;
  bool color_scheme_mode_shown_ = false;
#endif

  const raw_ptr<Profile> profile_;
};

}  // namespace settings_search

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_APPEARANCE_SECTION_H_
