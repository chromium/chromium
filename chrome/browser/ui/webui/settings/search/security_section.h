// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SECURITY_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SECURITY_SECTION_H_

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/ui/webui/settings/search/settings_section.h"

#if !BUILDFLAG(IS_CHROMEOS)
class DefaultDnsOverHttpsConfigSource;
#endif
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
#if !BUILDFLAG(IS_CHROMEOS)
  // Adds or removes the secure DNS provider item to match whether the
  // Security page shows the dropdown.
  void UpdateSecureDnsProviderItem();

  std::unique_ptr<DefaultDnsOverHttpsConfigSource> doh_source_;
  bool secure_dns_provider_shown_ = false;
#endif
};

}  // namespace settings_search

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SECURITY_SECTION_H_
