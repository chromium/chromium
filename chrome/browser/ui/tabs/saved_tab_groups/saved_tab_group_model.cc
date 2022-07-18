// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model.h"

#include <cstddef>
#include <vector>

#include "base/observer_list.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_observer.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

SavedTabGroupModel::SavedTabGroupModel() = default;
SavedTabGroupModel::~SavedTabGroupModel() = default;

SavedTabGroupModel::SavedTabGroupModel(Profile* profile) : profile_(profile) {}

int SavedTabGroupModel::GetIndexOf(tab_groups::TabGroupId tab_group_id) const {
  for (size_t i = 0; i < saved_tab_groups_.size(); i++)
    if (saved_tab_groups_[i].tab_group_id() == tab_group_id)
      return i;

  return -1;
}

int SavedTabGroupModel::GetIndexOf(const base::GUID& id) const {
  for (size_t i = 0; i < saved_tab_groups_.size(); i++) {
    if (saved_tab_groups_[i].saved_guid() == id)
      return i;
  }

  return -1;
}

const SavedTabGroup* SavedTabGroupModel::Get(const base::GUID& id) const {
  int index = GetIndexOf(id);
  if (index < 0) {
    return nullptr;
  }

  return &saved_tab_groups_[index];
}

void SavedTabGroupModel::Add(SavedTabGroup saved_group) {
  if (Contains(saved_group.saved_guid()))
    return;

  saved_tab_groups_.emplace_back(std::move(saved_group));
  const int index = Count() - 1;
  for (auto& observer : observers_)
    observer.SavedTabGroupAdded(saved_tab_groups_[index], index);
}

void SavedTabGroupModel::Remove(const tab_groups::TabGroupId tab_group_id) {
  if (!Contains(tab_group_id))
    return;

  const int index = GetIndexOf(tab_group_id);
  RemoveImpl(index);
}

void SavedTabGroupModel::Remove(const base::GUID& id) {
  if (!Contains(id))
    return;

  const int index = GetIndexOf(id);
  return RemoveImpl(index);
}

void SavedTabGroupModel::UpdateVisualData(
    tab_groups::TabGroupId tab_group_id,
    const tab_groups::TabGroupVisualData* visual_data) {
  if (!Contains(tab_group_id))
    return;

  const int index = GetIndexOf(tab_group_id);
  UpdateVisualDataImpl(index, visual_data);
}

void SavedTabGroupModel::UpdateVisualData(
    const base::GUID& id,
    const tab_groups::TabGroupVisualData* visual_data) {
  if (!Contains(id))
    return;

  const int index = GetIndexOf(id);
  UpdateVisualDataImpl(index, visual_data);
}

void SavedTabGroupModel::Reorder(const base::GUID& id, int new_index) {
  DCHECK_GE(new_index, 0);
  DCHECK_LT(new_index, Count());

  int index = GetIndexOf(id);
  CHECK_GE(index, 0);

  SavedTabGroup group = saved_tab_groups_[index];

  saved_tab_groups_.erase(saved_tab_groups_.begin() + index);
  saved_tab_groups_.emplace(saved_tab_groups_.begin() + new_index,
                            std::move(group));

  for (auto& observer : observers_)
    observer.SavedTabGroupMoved(saved_tab_groups_[new_index], index, new_index);
}

void SavedTabGroupModel::OnGroupClosedInTabStrip(
    const tab_groups::TabGroupId& tab_group_id) {
  const int index = GetIndexOf(tab_group_id);
  if (index == -1)
    return;

  SavedTabGroup& saved_group = saved_tab_groups_[index];
  saved_group.SetLocalGroupId(absl::nullopt);

  for (auto& observer : observers_)
    observer.SavedTabGroupUpdated(saved_group, index);
}

void SavedTabGroupModel::OnGroupOpenedInTabStrip(
    const base::GUID& id,
    const tab_groups::TabGroupId& tab_group_id) {
  const int index = GetIndexOf(id);
  CHECK_GE(index, 0);

  SavedTabGroup& saved_group = saved_tab_groups_[index];
  saved_group.SetLocalGroupId(tab_group_id);

  for (auto& observer : observers_)
    observer.SavedTabGroupUpdated(saved_group, index);
}

void SavedTabGroupModel::RemoveImpl(int index) {
  CHECK_GE(index, 0);
  saved_tab_groups_.erase(saved_tab_groups_.begin() + index);

  for (auto& observer : observers_)
    observer.SavedTabGroupRemoved(index);
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
  for (auto& observer : observers_)
    observer.SavedTabGroupUpdated(saved_group, index);
}

void SavedTabGroupModel::AddObserver(SavedTabGroupModelObserver* observer) {
  observers_.AddObserver(observer);
}

void SavedTabGroupModel::RemoveObserver(SavedTabGroupModelObserver* observer) {
  observers_.RemoveObserver(observer);
}
