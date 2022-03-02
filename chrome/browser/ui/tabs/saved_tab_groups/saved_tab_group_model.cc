// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model.h"

#include <vector>

#include "base/observer_list.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_observer.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"

SavedTabGroupModel::SavedTabGroupModel() = default;
SavedTabGroupModel::~SavedTabGroupModel() = default;

SavedTabGroupModel::SavedTabGroupModel(Profile* profile) : profile_(profile) {}

int SavedTabGroupModel::GetIndexOf(tab_groups::TabGroupId tab_group_id) {
  for (int i = 0; i < Count(); i++)
    if (saved_tab_groups_[i].group_id == tab_group_id)
      return i;

  return -1;
}

const SavedTabGroup* SavedTabGroupModel::Get(
    tab_groups::TabGroupId tab_group_id) {
  if (!Contains(tab_group_id))
    return nullptr;

  return &saved_tab_groups_[GetIndexOf(tab_group_id)];
}

void SavedTabGroupModel::Remove(const tab_groups::TabGroupId tab_group_id) {
  if (!Contains(tab_group_id))
    return;

  const int index = GetIndexOf(tab_group_id);
  saved_tab_groups_.erase(saved_tab_groups_.begin() + index);

  // Notify before the saved tab group is removed to give observers a chance to
  // update their views respectively.
  for (auto& observer : observers_)
    observer.SavedTabGroupRemoved(index);
}

void SavedTabGroupModel::Add(const SavedTabGroup& saved_group) {
  if (Contains(saved_group.group_id))
    return;

  saved_tab_groups_.emplace_back(std::move(saved_group));
  const int index = Count() - 1;
  for (auto& observer : observers_)
    observer.SavedTabGroupAdded(saved_tab_groups_[index], index);
}

void SavedTabGroupModel::Update(
    tab_groups::TabGroupId tab_group_id,
    const tab_groups::TabGroupVisualData* visual_data) {
  if (!Contains(tab_group_id))
    return;

  const int index = GetIndexOf(tab_group_id);
  SavedTabGroup& saved_group = saved_tab_groups_[index];
  if (saved_group.title == visual_data->title() &&
      saved_group.color == visual_data->color())
    return;

  saved_group.title = visual_data->title();
  saved_group.color = visual_data->color();
  for (auto& observer : observers_)
    observer.SavedTabGroupUpdated(saved_group, index);
}

void SavedTabGroupModel::AddObserver(SavedTabGroupModelObserver* observer) {
  observers_.AddObserver(observer);
}

void SavedTabGroupModel::RemoveObserver(SavedTabGroupModelObserver* observer) {
  observers_.RemoveObserver(observer);
}
