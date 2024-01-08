// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_ABOUT_ABOUT_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_ABOUT_ABOUT_SECTION_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/webui/ash/settings/pages/crostini/crostini_section.h"
#include "chrome/browser/ui/webui/ash/settings/pages/os_settings_section.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/user_manager/user_manager.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash::settings {

class SearchTagRegistry;

// Provides UI strings and search tags for the settings "About Chrome OS" page.
class AboutSection : public OsSettingsSection {
 public:
  AboutSection(Profile* profile,
               SearchTagRegistry* search_tag_registry,
               PrefService* pref_service);
  ~AboutSection() override;

  // OsSettingsSection:
  void AddLoadTimeData(content::WebUIDataSource* html_source) override;
  void AddHandlers(content::WebUI* web_ui) override;
  int GetSectionNameMessageId() const override;
  chromeos::settings::mojom::Section GetSection() const override;
  mojom::SearchResultIcon GetSectionIcon() const override;
  const char* GetSectionPath() const override;
  bool LogMetric(chromeos::settings::mojom::Setting setting,
                 base::Value& value) const override;
  void RegisterHierarchy(HierarchyGenerator* generator) const override;

 private:
  // Returns if the auto update toggle should be shown for the active user.
  bool ShouldShowAUToggle(user_manager::User* active_user);

  raw_ptr<PrefService> pref_service_;
  std::optional<CrostiniSection> crostini_subsection_;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  void UpdateReportIssueSearchTags();

  PrefChangeRegistrar pref_change_registrar_;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_ABOUT_ABOUT_SECTION_H_
