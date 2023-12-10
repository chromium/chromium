// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_TEST_SUPPORT_FAKE_HIERARCHY_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_TEST_SUPPORT_FAKE_HIERARCHY_H_

#include <optional>

#include "chrome/browser/ui/webui/ash/settings/search/hierarchy.h"

namespace ash::settings {

class OsSettingsSections;

// Fake Hierarchy implementation. Note that this class currently does not
// provide "alternate settings location" functionality.
class FakeHierarchy : public Hierarchy {
 public:
  explicit FakeHierarchy(const OsSettingsSections* sections);
  FakeHierarchy(const FakeHierarchy& other) = delete;
  FakeHierarchy& operator=(const FakeHierarchy& other) = delete;
  ~FakeHierarchy() override;

  void AddSubpageMetadata(int name_message_id,
                          chromeos::settings::mojom::Section section,
                          chromeos::settings::mojom::Subpage subpage,
                          mojom::SearchResultIcon icon,
                          mojom::SearchResultDefaultRank default_rank,
                          const std::string& url_path_with_parameters,
                          std::optional<chromeos::settings::mojom::Subpage>
                              parent_subpage = std::nullopt);
  void AddSettingMetadata(chromeos::settings::mojom::Section section,
                          chromeos::settings::mojom::Setting setting,
                          std::optional<chromeos::settings::mojom::Subpage>
                              parent_subpage = std::nullopt);

 private:
  // Hierarchy:
  std::string ModifySearchResultUrl(
      chromeos::settings::mojom::Section section,
      mojom::SearchResultType type,
      OsSettingsIdentifier id,
      const std::string& url_to_modify) const override;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_TEST_SUPPORT_FAKE_HIERARCHY_H_
