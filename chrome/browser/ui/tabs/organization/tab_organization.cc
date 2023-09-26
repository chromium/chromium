// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_organization.h"

#include <string>

#include "chrome/browser/ui/tabs/organization/tab_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace {
constexpr int kMinValidTabsForOrganizing = 2;
}

TabOrganization::TabOrganization(
    TabDatas tab_datas,
    std::vector<std::u16string> names,
    absl::variant<size_t, std::u16string> current_name,
    absl::optional<UserChoice> choice)
    : tab_datas_(std::move(tab_datas)),
      names_(names),
      current_name_(current_name),
      choice_(choice) {}
TabOrganization::TabOrganization(const TabOrganization&) {}
TabOrganization::~TabOrganization() {}

const std::u16string TabOrganization::GetDisplayName() const {
  if (absl::holds_alternative<size_t>(current_name())) {
    const size_t index = absl::get<size_t>(current_name());
    CHECK(index < names_.size());
    return names_.at(index);
  } else if (absl::holds_alternative<std::u16string>(current_name())) {
    return absl::get<std::u16string>(current_name());
  }
  return u"";
}

bool TabOrganization::IsValidForOrganizing() const {
  // there must be at least 2 tabs that are valid for organization.
  int valid_tab_count = 0;
  for (const std::unique_ptr<TabData>& tab_data : tab_datas_) {
    if (tab_data->IsValidForOrganizing()) {
      valid_tab_count++;
      if (valid_tab_count >= kMinValidTabsForOrganizing) {
        return true;
      }
    }
  }
  return false;
}

// TODO(1469128) Add UKM/UMA Logging on user add.
void TabOrganization::AddTabData(std::unique_ptr<TabData> tab_data) {
  tab_datas_.emplace_back(std::move(tab_data));
}

// TODO(1469128) Add UKM/UMA Logging on user remove.
void TabOrganization::RemoveTabData(TabData::TabID tab_id) {
  TabDatas::iterator position =
      std::find_if(tab_datas_.begin(), tab_datas_.end(),
                   [tab_id](const std::unique_ptr<TabData>& tab_data) {
                     return tab_data->tab_id() == tab_id;
                   });
  CHECK(position != tab_datas_.end());
  tab_datas_.erase(position);
}

void TabOrganization::SetCurrentName(
    absl::variant<size_t, std::u16string> new_current_name) {
  current_name_ = new_current_name;
}

// TODO(1469128) Add UKM/UMA Logging on user accept.
void TabOrganization::Accept() {
  CHECK(!choice_.has_value());
  choice_ = UserChoice::ACCEPTED;
}

// TODO(1469128) Add UKM/UMA Logging on user reject.
void TabOrganization::Reject() {
  CHECK(!choice_.has_value());
  choice_ = UserChoice::REJECTED;
}
