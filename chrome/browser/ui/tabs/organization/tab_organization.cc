// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_organization.h"

#include <string>

#include "chrome/browser/ui/tabs/organization/tab_data.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace {
constexpr int kMinValidTabsForOrganizing = 2;
int kNextOrganizationID = 1;
}

TabOrganization::TabOrganization(
    TabDatas tab_datas,
    std::vector<std::u16string> names,
    absl::variant<size_t, std::u16string> current_name,
    absl::optional<UserChoice> choice)
    : tab_datas_(std::move(tab_datas)),
      names_(names),
      current_name_(current_name),
      choice_(choice),
      organization_id_(kNextOrganizationID) {
  kNextOrganizationID++;
}
TabOrganization::TabOrganization(TabOrganization&& organization) = default;
TabOrganization::~TabOrganization() = default;

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
  CHECK(IsValidForOrganizing());
  choice_ = UserChoice::ACCEPTED;

  CHECK(tab_datas_.size() > 0);
  TabStripModel* tab_strip_model = tab_datas_[0]->original_tab_strip_model();
  CHECK(tab_strip_model);
  std::vector<int> valid_indices;
  for (const std::unique_ptr<TabData>& tab_data : tab_datas_) {
    // Individual tabs may become invalid. in those cases, where the tab is
    // invalid but the organization is not, do not include the tab in the
    // organization, but still create the organization.
    if (tab_data->IsValidForOrganizing()) {
      valid_indices.emplace_back(
          tab_strip_model->GetIndexOfWebContents(tab_data->web_contents()));
    }
  }

  tab_groups::TabGroupId group_id =
      tab_strip_model->AddToNewGroup(valid_indices);
  TabGroup* const tab_group =
      tab_strip_model->group_model()->GetTabGroup(group_id);
  tab_groups::TabGroupVisualData new_visual_data(
      GetDisplayName(), tab_group->visual_data()->color());
  tab_group->SetVisualData(std::move(new_visual_data),
                           tab_group->IsCustomized());
}

// TODO(1469128) Add UKM/UMA Logging on user reject.
void TabOrganization::Reject() {
  CHECK(!choice_.has_value());
  choice_ = UserChoice::REJECTED;
}
