// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_H_

#include <string>
#include <vector>

#include "chrome/browser/ui/tabs/organization/tab_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class TabOrganization {
 public:
  // TODO(dpenning): Make this a base::Token.
  using ID = int;
  using TabDatas = std::vector<std::unique_ptr<TabData>>;

  enum class UserChoice {
    ACCEPTED,
    REJECTED,
  };

  TabOrganization(TabDatas tab_datas,
                  std::vector<std::u16string> names,
                  absl::variant<size_t, std::u16string> current_name,
                  absl::optional<UserChoice> choice);
  TabOrganization(TabOrganization&& organization);
  TabOrganization& operator=(const TabOrganization& other) = default;
  ~TabOrganization();

  const TabDatas& tab_datas() const { return tab_datas_; }
  const std::vector<std::u16string>& names() const { return names_; }
  const absl::variant<size_t, std::u16string>& current_name() const {
    return current_name_;
  }
  const absl::optional<UserChoice> choice() const { return choice_; }
  ID organization_id() const { return organization_id_; }
  const std::u16string GetDisplayName() const;

  bool IsValidForOrganizing() const;

  void AddTabData(std::unique_ptr<TabData> tab_data);
  void RemoveTabData(TabData::TabID id);
  void SetCurrentName(absl::variant<size_t, std::u16string> new_current_name);
  void Accept();
  void Reject();

 private:
  TabDatas tab_datas_;
  std::vector<std::u16string> names_;
  absl::variant<size_t, std::u16string> current_name_;
  absl::optional<UserChoice> choice_;
  ID organization_id_;
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_H_
