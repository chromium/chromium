// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_organization.h"

#include <optional>
#include <string>
#include <unordered_set>

#include "base/debug/dump_without_crashing.h"
#include "chrome/browser/ui/tabs/organization/tab_data.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tab_groups/tab_group_id.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace {
constexpr int kMinValidTabsForOrganizing = 2;
int kNextOrganizationID = 1;
}

TabOrganization::TabOrganization(
    TabDatas tab_datas,
    std::vector<std::u16string> names,
    int first_new_tab_index,
    absl::variant<size_t, std::u16string> current_name,
    UserChoice choice)
    : first_new_tab_index_(first_new_tab_index),
      names_(names),
      current_name_(current_name),
      choice_(choice),
      organization_id_(kNextOrganizationID) {
  for (auto& tab_data : tab_datas_) {
    tab_data->AddObserver(this);
  }
  kNextOrganizationID++;

  // TabDatas must not be duplicates, immediately destroy TabDatas that are.
  std::vector<const tabs::TabModel*> existing_tabs;
  for (auto& tab_data : tab_datas) {
    if (!base::Contains(existing_tabs, tab_data->tab())) {
      existing_tabs.emplace_back(tab_data->tab());
      tab_data->AddObserver(this);
      tab_datas_.emplace_back(std::move(tab_data));
    }
  }
}

TabOrganization::~TabOrganization() {
  for (auto& tab_data : tab_datas_) {
    tab_data->RemoveObserver(this);
  }

  for (auto& observer : observers_) {
    observer.OnTabOrganizationDestroyed(organization_id_);
  }
}

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

void TabOrganization::AddObserver(TabOrganization::Observer* observer) {
  observers_.AddObserver(observer);
}

void TabOrganization::RemoveObserver(TabOrganization::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool TabOrganization::IsValidForOrganizing() const {
  if (invalidated_by_tab_change_) {
    return false;
  }

  // There must be at least 1 tab that is new to the group.
  if ((tab_datas_.size() - first_new_tab_index_) == 0) {
    return false;
  }

  // There must be at least 2 tabs that are valid for organization.
  int valid_tab_count = 0;
  for (const std::unique_ptr<TabData>& tab_data : tab_datas_) {
    if (tab_data->IsValidForOrganizing(group_id_)) {
      valid_tab_count++;
      if (valid_tab_count >= kMinValidTabsForOrganizing) {
        return true;
      }
    }
  }
  return false;
}

// TODO(crbug.com/40925231) Add UKM/UMA Logging on user add.
void TabOrganization::AddTabData(std::unique_ptr<TabData> new_tab_data) {
  // Guarantee uniqueness. early return and drop the new tab data if not unique.
  for (std::unique_ptr<TabData>& existing_tab_data : tab_datas_) {
    if (existing_tab_data->tab() == new_tab_data->tab()) {
      return;
    }
  }

  new_tab_data->AddObserver(this);
  tab_datas_.emplace_back(std::move(new_tab_data));
  NotifyObserversOfUpdate();
}

// TODO(crbug.com/40925231) Add UKM/UMA Logging on user remove.
void TabOrganization::RemoveTabData(TabData::TabID tab_id) {
  TabDatas::iterator position =
      std::find_if(tab_datas_.begin(), tab_datas_.end(),
                   [tab_id](const std::unique_ptr<TabData>& tab_data) {
                     return tab_data->tab_id() == tab_id;
                   });
  CHECK(position != tab_datas_.end());
  CHECK(static_cast<size_t>(first_new_tab_index_) < tab_datas_.size());
  // If the removed tab is already a part of the tab group (if any)
  // corresponding to this organization, decrement |first_new_tab_index_| to
  // account for its removal.
  if (position < tab_datas_.begin() + first_new_tab_index_) {
    first_new_tab_index_--;
  }

  user_removed_tab_ids_.push_back(tab_id);
  tab_datas_.erase(position);
  NotifyObserversOfUpdate();
}

void TabOrganization::SetCurrentName(
    absl::variant<size_t, std::u16string> new_current_name) {
  current_name_ = new_current_name;
  NotifyObserversOfUpdate();
}

void TabOrganization::Accept() {
  CHECK(choice_ == UserChoice::kNoChoice);
  CHECK(IsValidForOrganizing());
  choice_ = UserChoice::kAccepted;

  CHECK(tab_datas_.size() > 0);
  TabStripModel* tab_strip_model = tab_datas_[0]->original_tab_strip_model();
  CHECK(tab_strip_model);
  std::vector<int> valid_indices;
  std::unordered_set<raw_ptr<const tabs::TabModel>> tab_data_tabs;
  for (const std::unique_ptr<TabData>& tab_data : tab_datas_) {
    // Individual tabs may become invalid. in those cases, where the tab is
    // invalid but the organization is not, do not include the tab in the
    // organization, but still create the organization.
    const tabs::TabModel* tab = tab_data->tab();
    tab_data_tabs.insert(tab);
    const int index = tab_strip_model->GetIndexOfTab(tab);
    if (tab_data->IsValidForOrganizing() &&
        !base::Contains(valid_indices, index)) {
      valid_indices.emplace_back(index);
    }
  }
  std::sort(valid_indices.begin(), valid_indices.end());

  // TODO(b/319273296): Find a more permanent fix.
  // From this point on, we start modifying the tab strip, which
  // potentially notifies a large set of observers. TabOrganizationSession
  // (which owns |this|) gets destroyed when a tab is added or removed
  // from the tab strip. There is a risk that a tab strip observer modifies
  // the tab strip and therefore causes |this| to be deleted. So we keep
  // a WeakPtr to |this| to detect this case and avoid accessing member
  // variables, just in case.
  base::WeakPtr<TabOrganization> this_weak_ref =
      weak_ptr_factory_.GetWeakPtr();

  if (group_id_.has_value()) {
    CHECK(tab_strip_model->group_model()->ContainsTabGroup(group_id_.value()));
    tab_strip_model->AddToExistingGroup(valid_indices, group_id_.value(), true);

    // Remove tabs that should not longer be a part of the group. Do this after
    // adding new tabs to avoid the group ever becoming empty, which would
    // delete the group.
    TabGroup* const tab_group =
        tab_strip_model->group_model()->GetTabGroup(group_id_.value());
    const gfx::Range tab_indices = tab_group->ListTabs();
    std::vector<int> indices_to_remove;
    for (size_t grouped_tab_index = tab_indices.start();
         grouped_tab_index < tab_indices.end(); grouped_tab_index++) {
      tabs::TabModel* tab = tab_strip_model->GetTabAtIndex(grouped_tab_index);
      if (!tab_data_tabs.contains(tab)) {
        indices_to_remove.emplace_back(grouped_tab_index);
      }
    }
    tab_strip_model->RemoveFromGroup(indices_to_remove);
  } else {
    group_id_ =
        std::make_optional(tab_strip_model->AddToNewGroup(valid_indices));

    // Move the entire group to the start left of the tabstrip.
    // Iterate through the tabstrip model looking for the first non pinned, non
    // grouped tab. If this group is already in the leftmost position then leave
    // it there. Else move the group at the index of that tab.
    int move_index = tab_strip_model->IndexOfFirstNonPinnedTab();
    while (move_index < tab_strip_model->GetTabCount() &&
           (tab_strip_model->GetTabGroupForTab(move_index).has_value() &&
            tab_strip_model->GetTabGroupForTab(move_index).value() !=
                group_id_.value())) {
      move_index++;
    }
    CHECK(move_index < tab_strip_model->GetTabCount());

    if (tab_strip_model->GetTabGroupForTab(move_index) != group_id_.value()) {
      tab_strip_model->MoveGroupTo(group_id_.value(), move_index);
    }
  }

  TabGroup* const tab_group =
      tab_strip_model->group_model()->GetTabGroup(group_id_.value());
  tab_groups::TabGroupVisualData new_visual_data(
      GetDisplayName(), tab_group->visual_data()->color());
  tab_group->SetVisualData(std::move(new_visual_data),
                           tab_group->IsCustomized());

  // If |this| has been destroyed, there is no need to notify the observers:
  // in practice, the only observer is the TabOrganizationSession which owns
  // this object (and therefore has been destroyed) and who will just
  // notify WebUI it has been updated (of which there is no need because
  // WebUI is now tracking the new TabOrganizationSession which has replaced
  // the destroyed one).
  if (this_weak_ref) {
    NotifyObserversOfUpdate();
  } else {
    // We'd like to know if this really happens: if so, we should really
    // change the ownership model of TabOrganizationSession.
    base::debug::DumpWithoutCrashing();
  }
}

void TabOrganization::Reject() {
  CHECK(choice_ == UserChoice::kNoChoice);
  choice_ = UserChoice::kRejected;

  NotifyObserversOfUpdate();
}

void TabOrganization::OnTabDataUpdated(const TabData* tab_data) {
  if (!tab_data->IsValidForOrganizing(group_id_)) {
    invalidated_by_tab_change_ = true;
  }
  NotifyObserversOfUpdate();
}

void TabOrganization::OnTabDataDestroyed(TabData::TabID tab_id) {
  // Only invalidate if RemoveTabData was not previously called on this tab id.
  // Closure of a tab that is a part of an organization should invalidate it,
  // but removal of the tab from the organization should not.
  if (std::find(user_removed_tab_ids_.begin(), user_removed_tab_ids_.end(),
                tab_id) == user_removed_tab_ids_.end()) {
    invalidated_by_tab_change_ = true;
    NotifyObserversOfUpdate();
  }
}

void TabOrganization::NotifyObserversOfUpdate() {
  for (auto& observer : observers_) {
    observer.OnTabOrganizationUpdated(this);
  }
}
