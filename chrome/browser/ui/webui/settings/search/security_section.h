// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SECURITY_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SECURITY_SECTION_H_

#include "chrome/browser/ui/webui/settings/search/settings_section.h"

class Profile;

namespace settings_search {

// Searchable settings on the Security page and its Security keys subpage.
// Each item is registered under the same conditions that show its control on
// the page.
class SecuritySection : public SettingsSection {
 public:
  SecuritySection(SettingsSearchRegistry* registry, Profile* profile);
  SecuritySection(const SecuritySection&) = delete;
  SecuritySection& operator=(const SecuritySection&) = delete;
  ~SecuritySection() override;

 private:
  void RegisterSearchItems(Profile* profile);
};

}  // namespace settings_search

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SECURITY_SECTION_H_
