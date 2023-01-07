// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/saved_tab_group_model.h"

#include <cstddef>
#include <memory>
#include <vector>

#include "base/observer_list.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_model_observer.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

SavedTabGroupModel::SavedTabGroupModel() = default;
SavedTabGroupModel::~SavedTabGroupModel() = default;

SavedTabGroupModel::SavedTabGroupModel(Profile* profile) : profile_(profile) {}

absl::optional<int> SavedTabGroupModel::GetIndexOf(
    tab_groups::TabGroupId tab_group_id) const {
  for (size_t i = 0; i < saved_tab_groups_.size(); i++) {
    if (saved_tab_groups_[i].local_group_id() == tab_group_id)
      return i;
  }

  return absl::nullopt;
}

absl::optional<int> SavedTabGroupModel::GetIndexOf(const base::GUID& id) const {
  for (size_t i = 0; i < saved_tab_groups_.size(); i++) {
    if (saved_tab_groups_[i].saved_guid() == id)
      return i;
  }

  return absl::nullopt;
}

const SavedTabGroup* SavedTabGroupModel::Get(const base::GUID& id) const {
  absl::optional<int> index = GetIndexOf(id);
  if (!index.has_value()) {
    return nullptr;
  }

  return &saved_tab_groups_[index.value()];
}

SavedTabGroup* SavedTabGroupModel::Get(const base::GUID& id) {
  absl::optional<int> index = GetIndexOf(id);
  if (!index.has_value())
    return nullptr;

  return &saved_tab_groups_[index.value()];
}

const SavedTabGroup* SavedTabGroupModel::Get(
    const tab_groups::TabGroupId local_group_id) const {
  absl::optional<int> index = GetIndexOf(local_group_id);
  if (!index.has_value())
    return nullptr;

  return &saved_tab_groups_[index.value()];
}

SavedTabGroup* SavedTabGroupModel::Get(
    const tab_groups::TabGroupId local_group_id) {
  absl::optional<int> index = GetIndexOf(local_group_id);
  if (!index.has_value())
    return nullptr;

  return &saved_tab_groups_[index.value()];
}

void SavedTabGroupModel::Add(SavedTabGroup saved_group) {
  if (Contains(saved_group.saved_guid()))
    return;

  saved_tab_groups_.emplace_back(std::move(saved_group));
  const int index = Count() - 1;
  for (auto& observer : observers_) {
    observer.SavedTabGroupAddedLocally(saved_tab_groups_[index].saved_guid());
  }
}

void SavedTabGroupModel::Remove(const tab_groups::TabGroupId tab_group_id) {
  if (!Contains(tab_group_id))
    return;

  const absl::optional<int> index = GetIndexOf(tab_group_id);
  base::GUID removed_guid = Get(tab_group_id)->saved_guid();
  std::unique_ptr<SavedTabGroup> removed_group = RemoveImpl(index.value());
  for (auto& observer : observers_)
    observer.SavedTabGroupRemovedLocally(removed_group.get());
}

void SavedTabGroupModel::Remove(const base::GUID& id) {
  if (!Contains(id))
    return;

  const absl::optional<int> index = GetIndexOf(id);
  base::GUID removed_guid = Get(id)->saved_guid();
  std::unique_ptr<SavedTabGroup> removed_group = RemoveImpl(index.value());
  for (auto& observer : observers_)
    observer.SavedTabGroupRemovedLocally(removed_group.get());
}

void SavedTabGroupModel::UpdateVisualData(
    tab_groups::TabGroupId tab_group_id,
    const tab_groups::TabGroupVisualData* visual_data) {
  if (!Contains(tab_group_id))
    return;

  const absl::optional<int> index = GetIndexOf(tab_group_id);
  UpdateVisualDataImpl(index.value(), visual_data);
  base::GUID updated_guid = Get(tab_group_id)->saved_guid();
  for (auto& observer : observers_)
    observer.SavedTabGroupUpdatedLocally(updated_guid);
}

void SavedTabGroupModel::UpdateVisualData(
    const base::GUID& id,
    const tab_groups::TabGroupVisualData* visual_data) {
  if (!Contains(id))
    return;

  const absl::optional<int> index = GetIndexOf(id);
  UpdateVisualDataImpl(index.value(), visual_data);
  for (auto& observer : observers_)
    observer.SavedTabGroupUpdatedLocally(id);
}

void SavedTabGroupModel::AddedFromSync(SavedTabGroup saved_group) {
  if (Contains(saved_group.saved_guid()))
    return;

  saved_tab_groups_.emplace_back(std::move(saved_group));
  const int index = Count() - 1;
  for (auto& observer : observers_)
    observer.SavedTabGroupAddedFromSync(saved_tab_groups_[index].saved_guid());
}

void SavedTabGroupModel::RemovedFromSync(
    const tab_groups::TabGroupId tab_group_id) {
  if (!Contains(tab_group_id))
    return;

  const absl::optional<int> index = GetIndexOf(tab_group_id);
  base::GUID removed_guid = Get(tab_group_id)->saved_guid();
  std::unique_ptr<SavedTabGroup> removed_group = RemoveImpl(index.value());
  for (auto& observer : observers_)
    observer.SavedTabGroupRemovedFromSync(removed_group.get());
}

void SavedTabGroupModel::RemovedFromSync(const base::GUID& id) {
  if (!Contains(id))
    return;

  const absl::optional<int> index = GetIndexOf(id);
  base::GUID removed_guid = Get(id)->saved_guid();
  std::unique_ptr<SavedTabGroup> removed_group = RemoveImpl(index.value());
  for (auto& observer : observers_)
    observer.SavedTabGroupRemovedFromSync(removed_group.get());
}

void SavedTabGroupModel::UpdatedVisualDataFromSync(
    tab_groups::TabGroupId tab_group_id,
    const tab_groups::TabGroupVisualData* visual_data) {
  if (!Contains(tab_group_id))
    return;

  const absl::optional<int> index = GetIndexOf(tab_group_id);
  UpdateVisualDataImpl(index.value(), visual_data);
  base::GUID updated_guid = Get(tab_group_id)->saved_guid();
  for (auto& observer : observers_)
    observer.SavedTabGroupUpdatedFromSync(updated_guid);
}

void SavedTabGroupModel::UpdatedVisualDataFromSync(
    const base::GUID& id,
    const tab_groups::TabGroupVisualData* visual_data) {
  if (!Contains(id))
    return;

  const absl::optional<int> index = GetIndexOf(id);
  UpdateVisualDataImpl(index.value(), visual_data);
  for (auto& observer : observers_)
    observer.SavedTabGroupUpdatedFromSync(id);
}

void SavedTabGroupModel::AddTabToGroup(const base::GUID& group_id,
                                       SavedTabGroupTab tab,
                                       int index) {
  if (!Contains(group_id))
    return;

  absl::optional<int> group_index = GetIndexOf(group_id);
  saved_tab_groups_[group_index.value()].AddTab(index, tab);

  for (auto& observer : observers_)
    observer.SavedTabGroupUpdatedLocally(group_id);
}

void SavedTabGroupModel::RemoveTabFromGroup(const base::GUID& group_id,
                                            const base::GUID& tab_id) {
  if (!Contains(group_id))
    return;

  absl::optional<int> index = GetIndexOf(group_id);
  saved_tab_groups_[index.value()].RemoveTab(tab_id);

  for (auto& observer : observers_)
    observer.SavedTabGroupUpdatedLocally(group_id);
}

void SavedTabGroupModel::ReplaceTabInGroupAt(const base::GUID& group_id,
                                             const base::GUID& tab_id,
                                             SavedTabGroupTab new_tab) {
  if (!Contains(group_id))
    return;

  absl::optional<int> index = GetIndexOf(group_id);
  saved_tab_groups_[index.value()].ReplaceTabAt(tab_id, new_tab);

  for (auto& observer : observers_)
    observer.SavedTabGroupUpdatedLocally(group_id);
}

void SavedTabGroupModel::MoveTabInGroupTo(const base::GUID& group_id,
                                          const base::GUID& tab_id,
                                          int new_index) {
  if (!Contains(group_id))
    return;

  absl::optional<int> index = GetIndexOf(group_id);
  saved_tab_groups_[index.value()].MoveTab(tab_id, new_index);

  for (auto& observer : observers_)
    observer.SavedTabGroupUpdatedLocally(group_id);
}

std::unique_ptr<sync_pb::SavedTabGroupSpecifics> SavedTabGroupModel::MergeGroup(
    std::unique_ptr<sync_pb::SavedTabGroupSpecifics> sync_specific) {
  const base::GUID& group_id =
      base::GUID::ParseLowercase(sync_specific->guid());

  DCHECK(Contains(group_id));

  absl::optional<int> index = GetIndexOf(group_id);
  saved_tab_groups_[index.value()].MergeGroup(std::move(sync_specific));

  for (auto& observer : observers_)
    observer.SavedTabGroupUpdatedFromSync(group_id);

  return saved_tab_groups_[index.value()].ToSpecifics();
}

std::unique_ptr<sync_pb::SavedTabGroupSpecifics> SavedTabGroupModel::MergeTab(
    std::unique_ptr<sync_pb::SavedTabGroupSpecifics> sync_specific) {
  const base::GUID& group_id =
      base::GUID::ParseLowercase(sync_specific->tab().group_guid());
  const base::GUID& tab_id = base::GUID::ParseLowercase(sync_specific->guid());
  DCHECK(Contains(group_id));

  absl::optional<int> index = GetIndexOf(group_id);
  saved_tab_groups_[index.value()].GetTab(tab_id)->MergeTab(
      std::move(sync_specific));

  for (auto& observer : observers_)
    observer.SavedTabGroupUpdatedFromSync(group_id);

  return saved_tab_groups_[index.value()].GetTab(tab_id)->ToSpecifics();
}

void SavedTabGroupModel::Reorder(const base::GUID& id, int new_index) {
  DCHECK_GE(new_index, 0);
  DCHECK_LT(new_index, Count());

  absl::optional<int> index = GetIndexOf(id);
  CHECK(index.has_value());
  CHECK_GE(index.value(), 0);

  SavedTabGroup group = saved_tab_groups_[index.value()];

  saved_tab_groups_.erase(saved_tab_groups_.begin() + index.value());
  saved_tab_groups_.emplace(saved_tab_groups_.begin() + new_index,
                            std::move(group));

  for (auto& observer : observers_)
    observer.SavedTabGroupReorderedLocally();
}

void SavedTabGroupModel::LoadStoredEntries(
    std::vector<sync_pb::SavedTabGroupSpecifics> entries) {
  // TODO(crbug/1372095): Figure out if we should clear `saved_tab_groups`, in
  // the case there are entries saved before the bridge had a chance to load.
  std::vector<SavedTabGroupTab> tabs;

  // The `entries` is not ordered such that groups are guaranteed to be
  // at the front of the vector. As such, we can run into the case where we
  // try to add a tab to a group that does not exist for us yet.
  for (sync_pb::SavedTabGroupSpecifics proto : entries) {
    if (proto.has_group())
      saved_tab_groups_.emplace_back(SavedTabGroup::FromSpecifics(proto));
    else
      tabs.emplace_back(SavedTabGroupTab::FromSpecifics(proto));
  }

  for (auto tab : tabs) {
    absl::optional<int> index = GetIndexOf(tab.group_guid());
    if (!index.has_value())
      continue;

    saved_tab_groups_[index.value()].AddTab(0, std::move(tab));
  }

  for (SavedTabGroup group : saved_tab_groups_) {
    for (auto& observer : observers_)
      observer.SavedTabGroupAddedLocally(group.saved_guid());
  }
}

void SavedTabGroupModel::OnGroupClosedInTabStrip(
    const tab_groups::TabGroupId& tab_group_id) {
  const absl::optional<int> index = GetIndexOf(tab_group_id);
  if (!index.has_value())
    return;

  SavedTabGroup& saved_group = saved_tab_groups_[index.value()];
  saved_group.SetLocalGroupId(absl::nullopt);

  for (auto& observer : observers_)
    observer.SavedTabGroupUpdatedLocally(saved_group.saved_guid());
}

void SavedTabGroupModel::OnGroupOpenedInTabStrip(
    const base::GUID& id,
    const tab_groups::TabGroupId& tab_group_id) {
  const absl::optional<int> index = GetIndexOf(id);
  CHECK(index.has_value());
  CHECK_GE(index.value(), 0);

  SavedTabGroup& saved_group = saved_tab_groups_[index.value()];
  saved_group.SetLocalGroupId(tab_group_id);

  for (auto& observer : observers_)
    observer.SavedTabGroupUpdatedLocally(saved_group.saved_guid());
}

std::unique_ptr<SavedTabGroup> SavedTabGroupModel::RemoveImpl(int index) {
  CHECK_GE(index, 0);
  std::unique_ptr<SavedTabGroup> removed_group =
      std::make_unique<SavedTabGroup>(std::move(saved_tab_groups_[index]));
  saved_tab_groups_.erase(saved_tab_groups_.begin() + index);
  return removed_group;
}

void SavedTabGroupModel::UpdateVisualDataImpl(
    int index,
    const tab_groups::TabGroupVisualData* visual_data) {
  SavedTabGroup& saved_group = saved_tab_groups_[index];
  if (saved_group.title() == visual_data->title() &&
      saved_group.color() == visual_data->color())
    return;

  saved_group.SetTitle(visual_data->title());
  saved_group.SetColor(visual_data->color());
}

void SavedTabGroupModel::AddObserver(SavedTabGroupModelObserver* observer) {
  observers_.AddObserver(observer);
}

void SavedTabGroupModel::RemoveObserver(SavedTabGroupModelObserver* observer) {
  observers_.RemoveObserver(observer);
}
