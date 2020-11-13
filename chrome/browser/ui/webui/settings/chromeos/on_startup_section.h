// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_ON_STARTUP_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_ON_STARTUP_SECTION_H_

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_section.h"
#include "components/prefs/pref_change_registrar.h"

namespace chromeos {
namespace settings {

class SearchTagRegistry;

// Provides UI strings and search tags for On Startup settings.
class OnStartupSection : public OsSettingsSection {
 public:
  OnStartupSection(Profile* profile,
                   SearchTagRegistry* search_tag_registry,
                   PrefService* pref_service);
  ~OnStartupSection() override;

 private:
  // OsSettingsSection:
  void AddLoadTimeData(content::WebUIDataSource* html_source) override;
  int GetSectionNameMessageId() const override;
  mojom::Section GetSection() const override;
  mojom::SearchResultIcon GetSectionIcon() const override;
  std::string GetSectionPath() const override;
  void RegisterHierarchy(HierarchyGenerator* generator) const override;
  bool LogMetric(mojom::Setting setting, base::Value& value) const override;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_ON_STARTUP_SECTION_H_
