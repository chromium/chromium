// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_contents_data.h"

#include <vector>

#include "base/containers/adapters.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_collection.h"
#include "chrome/browser/ui/ui_features.h"

TabContentsData::TabContentsData() = default;
TabContentsData::~TabContentsData() = default;

class TabContentsDataImpl : public TabContentsData {
 public:
  TabContentsDataImpl() = default;
  ~TabContentsDataImpl() override = default;
  TabContentsDataImpl(const TabContentsDataImpl&) = delete;
  TabContentsDataImpl& operator=(const TabContentsDataImpl&) = delete;

  size_t TotalTabCount() const override;
  size_t IndexOfFirstNonPinnedTab() const override;

  tabs::TabModel* GetTabAtIndexRecursive(size_t index) const override;

  std::optional<size_t> GetIndexOfTabRecursive(
      const tabs::TabModel* tab_handle) const override;

  void AddTabRecursive(std::unique_ptr<tabs::TabModel> tab_model,
                       size_t index,
                       std::optional<tab_groups::TabGroupId> new_group_id,
                       bool new_pinned_state) override;

  std::unique_ptr<tabs::TabModel> RemoveTabAtIndexRecursive(
      size_t index) override;

  void MoveTabRecursive(size_t initial_index,
                        size_t final_index,
                        std::optional<tab_groups::TabGroupId> new_group_id,
                        bool new_pinned_state) override;

  void MoveTabsRecursive(const std::vector<int>& tab_indices,
                         size_t destination_index,
                         std::optional<tab_groups::TabGroupId> new_group_id,
                         bool new_pinned_state) override;

  void MoveGroupTo(const TabGroupModel* group_model,
                   const tab_groups::TabGroupId& group,
                   int to_index) override;

  void ValidateData(const TabGroupModel* group_model) override;

 private:
  std::vector<std::unique_ptr<tabs::TabModel>> contents_data_;
};

std::unique_ptr<TabContentsData> CreateTabContentsDataImpl() {
  if (base::FeatureList::IsEnabled(tabs::kTabStripCollectionStorage)) {
    return std::make_unique<tabs::TabStripCollection>();
  } else {
    return std::make_unique<TabContentsDataImpl>();
  }
}

size_t TabContentsDataImpl::TotalTabCount() const {
  return contents_data_.size();
}

size_t TabContentsDataImpl::IndexOfFirstNonPinnedTab() const {
  for (size_t i = 0; i < contents_data_.size(); ++i) {
    if (!contents_data_[i].get()->pinned()) {
      return i;
    }
  }
  return contents_data_.size();
}

tabs::TabModel* TabContentsDataImpl::GetTabAtIndexRecursive(
    size_t index) const {
  return contents_data_[index].get();
}

std::optional<size_t> TabContentsDataImpl::GetIndexOfTabRecursive(
    const tabs::TabModel* tab_model) const {
  const auto is_same_tab =
      [tab_model](const std::unique_ptr<tabs::TabModel>& other) {
        return other.get() == tab_model;
      };

  const auto iter =
      std::find_if(contents_data_.cbegin(), contents_data_.cend(), is_same_tab);
  if (iter == contents_data_.cend()) {
    return std::nullopt;
  }
  return iter - contents_data_.begin();
}

void TabContentsDataImpl::AddTabRecursive(
    std::unique_ptr<tabs::TabModel> tab_model,
    size_t index,
    std::optional<tab_groups::TabGroupId> new_group_id,
    bool new_pinned_state) {
  tab_model->set_group(new_group_id);
  tab_model->set_pinned(new_pinned_state);
  contents_data_.insert(contents_data_.begin() + index, std::move(tab_model));
}

std::unique_ptr<tabs::TabModel> TabContentsDataImpl::RemoveTabAtIndexRecursive(
    size_t index) {
  // Remove the tab.
  std::unique_ptr<tabs::TabModel> old_data;

  old_data = std::move(contents_data_[index]);
  contents_data_.erase(contents_data_.begin() + index);

  // Update the tab.
  old_data->set_group(std::nullopt);

  return old_data;
}

void TabContentsDataImpl::MoveTabRecursive(
    size_t initial_index,
    size_t final_index,
    std::optional<tab_groups::TabGroupId> new_group_id,
    bool new_pinned_state) {
  tabs::TabModel* tab_model = GetTabAtIndexRecursive(initial_index);

  tab_model->set_pinned(new_pinned_state);
  tab_model->set_group(new_group_id);

  // Move the tab to the destination index.
  if (initial_index != final_index) {
    std::unique_ptr<tabs::TabModel> moved_data =
        std::move(contents_data_[initial_index]);
    contents_data_.erase(contents_data_.begin() + initial_index);
    contents_data_.insert(contents_data_.begin() + final_index,
                          std::move(moved_data));
  }
}

void TabContentsDataImpl::MoveTabsRecursive(
    const std::vector<int>& tab_indices,
    size_t destination_index,
    std::optional<tab_groups::TabGroupId> new_group_id,
    bool new_pinned_state) {
  std::vector<std::unique_ptr<tabs::TabModel>> moved_datas;
  // Remove all the tabs from the model.
  for (int tab_index : base::Reversed(tab_indices)) {
    std::unique_ptr<tabs::TabModel> moved_data =
        RemoveTabAtIndexRecursive(tab_index);
    moved_datas.insert(moved_datas.begin(), std::move(moved_data));
  }

  //  Add all the tabs back to the model.
  for (size_t i = 0; i < moved_datas.size(); i++) {
    AddTabRecursive(std::move(moved_datas[i]), destination_index + i,
                    new_group_id, new_pinned_state);
  }
}

void TabContentsDataImpl::MoveGroupTo(const TabGroupModel* group_model,
                                      const tab_groups::TabGroupId& group,
                                      int to_index) {
  gfx::Range tabs_in_group = group_model->GetTabGroup(group)->ListTabs();
  std::vector<int> tab_indices;
  for (size_t i = tabs_in_group.start(); i < tabs_in_group.end(); ++i) {
    tab_indices.push_back(i);
  }
  MoveTabsRecursive(tab_indices, to_index, group, false);
}

void TabContentsDataImpl::ValidateData(const TabGroupModel* group_model) {
#if DCHECK_IS_ON()
  if (contents_data_.empty()) {
    return;
  }

  // Check for pinned validity.
  bool unpinned_found = contents_data_[0].get()->pinned() ? false : true;
  for (size_t i = 1; i < contents_data_.size(); i++) {
    if (!unpinned_found) {
      if (!contents_data_[i].get()->pinned()) {
        unpinned_found = true;
      }
    } else {
      DCHECK(!contents_data_[i].get()->pinned());
    }
  }

  // If tab groups are not supported return true.
  if (!group_model) {
    return;
  }

  for (const auto& group_id : group_model->ListTabGroups()) {
    gfx::Range tabs_in_group = group_model->GetTabGroup(group_id)->ListTabs();
    if (!tabs_in_group.is_empty()) {
      for (size_t index = tabs_in_group.start(); index < tabs_in_group.end();
           ++index) {
        DCHECK(contents_data_[index].get()->group() == group_id);
      }
    }
  }
#endif
}
