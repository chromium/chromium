// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_FAKE_OS_SETTINGS_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_FAKE_OS_SETTINGS_SECTION_H_

#include "base/values.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_section.h"

namespace chromeos {
namespace settings {

// Fake OsSettingsSection implementation.
class FakeOsSettingsSection : public OsSettingsSection {
 public:
  explicit FakeOsSettingsSection(mojom::Section section);
  ~FakeOsSettingsSection() override;

  FakeOsSettingsSection(const FakeOsSettingsSection& other) = delete;
  FakeOsSettingsSection& operator=(const FakeOsSettingsSection& other) = delete;

  mojom::Section section() { return section_; }

  // OsSettingsSection:
  void AddLoadTimeData(content::WebUIDataSource* html_source) override {}
  void RegisterHierarchy(HierarchyGenerator* generator) const override {}

  // Returns the settings app name as a default value.
  int GetSectionNameMessageId() const override;

  mojom::Section GetSection() const override;

  // These functions return arbitrary dummy values.
  mojom::SearchResultIcon GetSectionIcon() const override;
  std::string GetSectionPath() const override;
  bool LogMetric(mojom::Setting setting, base::Value& value) const override;

  // Prepends the section name and "::" to the URL in |concept|. For example, if
  // the URL is "networkDetails" and the section is mojom::Section::kNetwork,
  // the returned URL is "Section::kNetwork::networkDetails".
  std::string ModifySearchResultUrl(
      mojom::SearchResultType type,
      OsSettingsIdentifier id,
      const std::string& url_to_modify) const override;

  // Static function used to implement the function above.
  static std::string ModifySearchResultUrl(mojom::Section section,
                                           const std::string& url_to_modify);

 private:
  const mojom::Section section_;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_FAKE_OS_SETTINGS_SECTION_H_
