// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_RESET_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_RESET_SECTION_H_

#include "base/values.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_section.h"
// TODO(https://crbug.com/1164001): move to forward declaration
#include "chrome/browser/ui/webui/settings/ash/search/search_tag_registry.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace chromeos {
namespace settings {

// Provides UI strings and search tags for Reset settings. Note that search tags
// are only added when powerwashing is allowed, since currently this is the only
// setting in the Reset section.
class ResetSection : public OsSettingsSection {
 public:
  ResetSection(Profile* profile, SearchTagRegistry* search_tag_registry);
  ~ResetSection() override;

 private:
  // OsSettingsSection:
  void AddLoadTimeData(content::WebUIDataSource* html_source) override;
  void AddHandlers(content::WebUI* web_ui) override;
  int GetSectionNameMessageId() const override;
  mojom::Section GetSection() const override;
  ash::settings::mojom::SearchResultIcon GetSectionIcon() const override;
  std::string GetSectionPath() const override;
  bool LogMetric(mojom::Setting setting, base::Value& value) const override;
  void RegisterHierarchy(HierarchyGenerator* generator) const override;
};

}  // namespace settings
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when it moved to ash.
namespace ash::settings {
using ::chromeos::settings::ResetSection;
}

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_RESET_SECTION_H_
