// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

#include <utility>

#include "base/logging.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

using content::WebContents;

TabStripModelChange::Insert::Insert() = default;
TabStripModelChange::Insert::Insert(Insert&& other) = default;
TabStripModelChange::Insert& TabStripModelChange::Insert::operator=(Insert&&) =
    default;
TabStripModelChange::Insert::~Insert() = default;

TabStripModelChange::Remove::Remove() = default;
TabStripModelChange::Remove::Remove(Remove&& other) = default;
TabStripModelChange::Remove& TabStripModelChange::Remove::operator=(Remove&&) =
    default;
TabStripModelChange::Remove::~Remove() = default;

////////////////////////////////////////////////////////////////////////////////
// TabStripModelChange
//
TabStripModelChange::TabStripModelChange() = default;

TabStripModelChange::GroupChange::GroupChange() = default;

TabStripModelChange::GroupChange::GroupChange(const GroupChange& other) =
    default;

TabStripModelChange::GroupChange& TabStripModelChange::GroupChange::operator=(
    const GroupChange& other) = default;

TabStripModelChange::GroupChange::~GroupChange() = default;

TabStripModelChange::TabStripModelChange(Insert delta)
    : TabStripModelChange(Type::kInserted,
                          std::make_unique<Insert>(std::move(delta))) {}

TabStripModelChange::TabStripModelChange(Remove delta)
    : TabStripModelChange(Type::kRemoved,
                          std::make_unique<Remove>(std::move(delta))) {}

TabStripModelChange::TabStripModelChange(Move delta)
    : TabStripModelChange(Type::kMoved,
                          std::make_unique<Move>(std::move(delta))) {}

TabStripModelChange::TabStripModelChange(Replace delta)
    : TabStripModelChange(Type::kReplaced,
                          std::make_unique<Replace>(std::move(delta))) {}

TabStripModelChange::TabStripModelChange(GroupChange delta)
    : TabStripModelChange(Type::kGroupChanged,
                          std::make_unique<GroupChange>(std::move(delta))) {}

TabStripModelChange::~TabStripModelChange() = default;

const TabStripModelChange::Insert* TabStripModelChange::GetInsert() const {
  DCHECK_EQ(type_, Type::kInserted);
  return static_cast<const Insert*>(delta_.get());
}

const TabStripModelChange::Remove* TabStripModelChange::GetRemove() const {
  DCHECK_EQ(type_, Type::kRemoved);
  return static_cast<const Remove*>(delta_.get());
}

const TabStripModelChange::Move* TabStripModelChange::GetMove() const {
  DCHECK_EQ(type_, Type::kMoved);
  return static_cast<const Move*>(delta_.get());
}

const TabStripModelChange::Replace* TabStripModelChange::GetReplace() const {
  DCHECK_EQ(type_, Type::kReplaced);
  return static_cast<const Replace*>(delta_.get());
}

const TabStripModelChange::GroupChange* TabStripModelChange::GetGroupChange()
    const {
  DCHECK_EQ(type_, Type::kGroupChanged);
  return static_cast<const GroupChange*>(delta_.get());
}

TabStripModelChange::TabStripModelChange(Type type,
                                         std::unique_ptr<Delta> delta)
    : type_(type), delta_(std::move(delta)) {}

////////////////////////////////////////////////////////////////////////////////
// TabStripSelectionChange
//
TabStripSelectionChange::TabStripSelectionChange() = default;

TabStripSelectionChange::TabStripSelectionChange(
    content::WebContents* contents,
    const ui::ListSelectionModel& selection_model)
    : old_contents(contents),
      new_contents(contents),
      old_model(selection_model),
      new_model(selection_model),
      reason(0) {}

TabStripSelectionChange::~TabStripSelectionChange() = default;

TabStripSelectionChange::TabStripSelectionChange(
    const TabStripSelectionChange& other) = default;

TabStripSelectionChange& TabStripSelectionChange::operator=(
    const TabStripSelectionChange& other) = default;

////////////////////////////////////////////////////////////////////////////////
// TabStripModelObserver
//
TabStripModelObserver::TabStripModelObserver() {}

TabStripModelObserver::~TabStripModelObserver() {
  std::set<TabStripModel*> models(observed_models_.begin(),
                                  observed_models_.end());
  for (auto* model : models) {
    model->RemoveObserver(this);
  }
}

void TabStripModelObserver::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {}

void TabStripModelObserver::OnTabGroupVisualDataChanged(
    TabStripModel* tab_strip_model,
    TabGroupId group,
    const TabGroupVisualData* visual_data) {}

void TabStripModelObserver::TabChangedAt(WebContents* contents,
                                         int index,
                                         TabChangeType change_type) {
}

void TabStripModelObserver::TabPinnedStateChanged(
    TabStripModel* tab_strip_model,
    WebContents* contents,
    int index) {
}

void TabStripModelObserver::TabBlockedStateChanged(WebContents* contents,
                                                   int index) {
}

void TabStripModelObserver::TabStripEmpty() {
}

void TabStripModelObserver::WillCloseAllTabs(TabStripModel* tab_strip_model) {}

void TabStripModelObserver::CloseAllTabsStopped(TabStripModel* tab_strip_model,
                                                CloseAllStoppedReason reason) {}
void TabStripModelObserver::SetTabNeedsAttentionAt(int index, bool attention) {}
void TabStripModelObserver::OnTabStripModelDestroyed(TabStripModel* model) {}

// static
void TabStripModelObserver::StopObservingAll(TabStripModelObserver* observer) {
  while (!observer->observed_models_.empty()) {
    (*observer->observed_models_.begin())->RemoveObserver(observer);
  }
}

// static
bool TabStripModelObserver::IsObservingAny(TabStripModelObserver* observer) {
  return !observer->observed_models_.empty();
}

// static
int TabStripModelObserver::CountObservedModels(
    TabStripModelObserver* observer) {
  return observer->observed_models_.size();
}

void TabStripModelObserver::StartedObserving(
    TabStripModelObserver::ModelPasskey,
    TabStripModel* model) {
  // TODO(https://crbug.com/991308): Add this DCHECK here. This DCHECK enforces
  // that a given TabStripModelObserver only observes a given TabStripModel
  // once.
  // DCHECK_EQ(observed_models_.count(model), 0U);
  observed_models_.insert(model);
}

void TabStripModelObserver::StoppedObserving(
    TabStripModelObserver::ModelPasskey,
    TabStripModel* model) {
  // TODO(https://crbug.com/991308): Add this DCHECK here. This DCHECK enforces
  // that a given TabStripModelObserver is only removed from a given
  // TabStripModel once.
  // DCHECK_EQ(observed_models_.count(model), 1U);
  observed_models_.erase(model);
}

void TabStripModelObserver::ModelDestroyed(TabStripModelObserver::ModelPasskey,
                                           TabStripModel* model) {
  model->RemoveObserver(this);
  OnTabStripModelDestroyed(model);
}
