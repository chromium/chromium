// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

#include <utility>

#include "base/check_op.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

using content::WebContents;

TabStripModelChange::RemovedTab::RemovedTab(content::WebContents* contents,
                                            int index,
                                            RemoveReason remove_reason,
                                            std::optional<SessionID> session_id)
    : contents(contents),
      index(index),
      remove_reason(remove_reason),
      session_id(session_id) {}
TabStripModelChange::RemovedTab::~RemovedTab() = default;
TabStripModelChange::RemovedTab::RemovedTab(RemovedTab&& other) = default;

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

TabStripModelChange::TabStripModelChange(Type type,
                                         std::unique_ptr<Delta> delta)
    : type_(type), delta_(std::move(delta)) {}

void TabStripModelChange::RemovedTab::WriteIntoTrace(
    perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("contents", contents);
  dict.Add("index", index);
  dict.Add("remove_reason", remove_reason);
}

void TabStripModelChange::ContentsWithIndex::WriteIntoTrace(
    perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("contents", contents);
  dict.Add("index", index);
}

void TabStripModelChange::Insert::WriteIntoTrace(
    perfetto::TracedValue context) const {
  perfetto::WriteIntoTracedValue(std::move(context), contents);
}

void TabStripModelChange::Remove::WriteIntoTrace(
    perfetto::TracedValue context) const {
  perfetto::WriteIntoTracedValue(std::move(context), contents);
}

void TabStripModelChange::Move::WriteIntoTrace(
    perfetto::TracedValue context) const {
  perfetto::WriteIntoTracedValue(std::move(context), contents);
}

void TabStripModelChange::Replace::WriteIntoTrace(
    perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("old_contents", old_contents);
  dict.Add("new_contents", new_contents);
  dict.Add("index", index);
}

void TabStripModelChange::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("type", type_);
  dict.Add("delta", delta_);
}

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
// TabGroupChange
//
TabGroupChange::TabGroupChange(TabStripModel* model,
                               tab_groups::TabGroupId group,
                               Type type,
                               std::unique_ptr<Delta> deltap)
    : group(group), model(model), type(type), delta(std::move(deltap)) {}

TabGroupChange::~TabGroupChange() = default;

TabGroupChange::VisualsChange::VisualsChange() = default;
TabGroupChange::VisualsChange::~VisualsChange() = default;

const TabGroupChange::VisualsChange* TabGroupChange::GetVisualsChange() const {
  DCHECK_EQ(type, Type::kVisualsChanged);
  return static_cast<const VisualsChange*>(delta.get());
}

TabGroupChange::TabGroupChange(TabStripModel* model,
                               tab_groups::TabGroupId group,
                               VisualsChange deltap)
    : TabGroupChange(model,
                     group,
                     Type::kVisualsChanged,
                     std::make_unique<VisualsChange>(std::move(deltap))) {}

////////////////////////////////////////////////////////////////////////////////
// TabStripModelObserver
//
TabStripModelObserver::TabStripModelObserver() {}

TabStripModelObserver::~TabStripModelObserver() {
  std::set<raw_ptr<TabStripModel, SetExperimental>> models(
      observed_models_.begin(), observed_models_.end());
  for (TabStripModel* model : models) {
    model->RemoveObserver(this);
  }
}

void TabStripModelObserver::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {}

void TabStripModelObserver::OnTabWillBeAdded() {}

void TabStripModelObserver::OnTabWillBeRemoved(content::WebContents* contents,
                                               int index) {}

void TabStripModelObserver::OnTabGroupChanged(const TabGroupChange& change) {}

void TabStripModelObserver::OnTabGroupAdded(
    const tab_groups::TabGroupId& group_id) {}

void TabStripModelObserver::OnTabGroupWillBeRemoved(
    const tab_groups::TabGroupId& group_id) {}

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

void TabStripModelObserver::TabGroupedStateChanged(
    std::optional<tab_groups::TabGroupId> group,
    tabs::TabModel* tab,
    int index) {}

void TabStripModelObserver::TabStripEmpty() {
}

void TabStripModelObserver::TabCloseCancelled(
    const content::WebContents* contents) {}

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
  // TODO(crbug.com/40639200): Add this DCHECK here. This DCHECK enforces
  // that a given TabStripModelObserver only observes a given TabStripModel
  // once.
  // DCHECK_EQ(observed_models_.count(model), 0U);
  observed_models_.insert(model);
}

void TabStripModelObserver::StoppedObserving(
    TabStripModelObserver::ModelPasskey,
    TabStripModel* model) {
  // TODO(crbug.com/40639200): Add this DCHECK here. This DCHECK enforces
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
