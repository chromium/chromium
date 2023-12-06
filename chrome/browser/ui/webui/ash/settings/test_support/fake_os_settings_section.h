// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_TEST_SUPPORT_FAKE_OS_SETTINGS_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_TEST_SUPPORT_FAKE_OS_SETTINGS_SECTION_H_

#include <optional>
#include <utility>
#include <vector>

#include "ash/webui/settings/public/constants/setting.mojom-shared.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/settings/pages/os_settings_section.h"

namespace ash::settings {

// Fake OsSettingsSection implementation.
class FakeOsSettingsSection : public OsSettingsSection {
 public:
  explicit FakeOsSettingsSection(chromeos::settings::mojom::Section section);
  ~FakeOsSettingsSection() override;

  FakeOsSettingsSection(const FakeOsSettingsSection& other) = delete;
  FakeOsSettingsSection& operator=(const FakeOsSettingsSection& other) = delete;

  chromeos::settings::mojom::Section section() { return section_; }
  const std::vector<chromeos::settings::mojom::Setting>& logged_metrics()
      const {
    return logged_metrics_;
  }

  // Add fake subpage and settings information. If `subpage` is nullopt, the
  // `setting` must exist and is attached directly to the section.
  void AddSubpageAndSetting(
      std::optional<chromeos::settings::mojom::Subpage> subpage,
      std::optional<chromeos::settings::mojom::Setting> setting);

  // OsSettingsSection:
  void AddLoadTimeData(content::WebUIDataSource* html_source) override {}
  void RegisterHierarchy(HierarchyGenerator* generator) const override;

  // Returns the settings app name as a default value.
  int GetSectionNameMessageId() const override;

  chromeos::settings::mojom::Section GetSection() const override;

  // These functions return arbitrary dummy values.
  mojom::SearchResultIcon GetSectionIcon() const override;
  const char* GetSectionPath() const override;
  bool LogMetric(chromeos::settings::mojom::Setting setting,
                 base::Value& value) const override;

  // Prepends the section name and "::" to the URL in |concept|. For example, if
  // the URL is "networkDetails" and the section is mojom::Section::kNetwork,
  // the returned URL is "Section::kNetwork::networkDetails".
  std::string ModifySearchResultUrl(
      mojom::SearchResultType type,
      OsSettingsIdentifier id,
      const std::string& url_to_modify) const override;

  // Static function used to implement the function above.
  static std::string ModifySearchResultUrl(
      chromeos::settings::mojom::Section section,
      const std::string& url_to_modify);

 private:
  const chromeos::settings::mojom::Section section_;
  std::map<chromeos::settings::mojom::Subpage,
           std::vector<chromeos::settings::mojom::Setting>>
      subpages_;
  std::vector<chromeos::settings::mojom::Setting> settings_;
  // Use mutable to modify this vector within the overridden const LogMetric.
  mutable std::vector<chromeos::settings::mojom::Setting> logged_metrics_;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_TEST_SUPPORT_FAKE_OS_SETTINGS_SECTION_H_
