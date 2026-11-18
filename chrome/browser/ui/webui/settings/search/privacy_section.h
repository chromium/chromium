// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_PRIVACY_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_PRIVACY_SECTION_H_

#include "chrome/browser/ui/webui/settings/search/settings_section.h"

class Profile;

namespace settings_search {

// Searchable settings on the Privacy and security page, its Third-party
// cookies subpage, and the Safety Check page and Delete browsing data dialog
// it opens. Each item is registered under the same conditions that show its
// control on the page. Rows that open the Security and Site settings pages
// belong to those pages' sections.
class PrivacySection : public SettingsSection {
 public:
  // `registry` must be non-null and outlive this; `profile` must be non-null
  // and is only used during construction.
  PrivacySection(SettingsSearchRegistry* registry, Profile* profile);
  PrivacySection(const PrivacySection&) = delete;
  PrivacySection& operator=(const PrivacySection&) = delete;
  ~PrivacySection() override;

 private:
  void RegisterSearchItems(Profile* profile);
};

}  // namespace settings_search

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_PRIVACY_SECTION_H_
