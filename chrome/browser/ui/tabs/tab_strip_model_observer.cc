// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

#include <utility>
#include <variant>

#include "base/check_op.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

using content::WebContents;

TabStripModelChange::RemovedTab::RemovedTab(
    tabs::TabInterface* tab,
    int index,
    RemoveReason remove_reason,
    tabs::TabInterface::DetachReason tab_detach_reason,
    std::optional<SessionID> session_id)
    : tab(tab),
      contents(tab ? tab->GetContents() : nullptr),
      index(index),
      remove_reason(remove_reason),
      tab_detach_reason(tab_detach_reason),
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
    : TabStripModelChange(Type::kInserted, std::move(delta)) {}

TabStripModelChange::TabStripModelChange(Remove delta)
    : TabStripModelChange(Type::kRemoved, std::move(delta)) {}

TabStripModelChange::TabStripModelChange(Move delta)
    : TabStripModelChange(Type::kMoved, std::move(delta)) {}

TabStripModelChange::TabStripModelChange(Replace delta)
    : TabStripModelChange(Type::kReplaced, std::move(delta)) {}

TabStripModelChange::~TabStripModelChange() = default;

const TabStripModelChange::Insert* TabStripModelChange::GetInsert() const {
  CHECK_EQ(type_, Type::kInserted);
  return &std::get<Insert>(delta_);
}

const TabStripModelChange::Remove* TabStripModelChange::GetRemove() const {
  CHECK_EQ(type_, Type::kRemoved);
  return &std::get<Remove>(delta_);
}

const TabStripModelChange::Move* TabStripModelChange::GetMove() const {
  CHECK_EQ(type_, Type::kMoved);
  return &std::get<Move>(delta_);
}

const TabStripModelChange::Replace* TabStripModelChange::GetReplace() const {
  CHECK_EQ(type_, Type::kReplaced);
  return &std::get<Replace>(delta_);
}

TabStripModelChange::TabStripModelChange(Type type, Delta delta)
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
  std::visit([&dict](auto&& delta) { dict.Add("delta", delta); }, delta_);
}

////////////////////////////////////////////////////////////////////////////////
// TabStripSelectionChange
//
TabStripSelectionChange::TabStripSelectionChange() = default;

TabStripSelectionChange::TabStripSelectionChange(
    tabs::TabInterface* tab,
    const ui::ListSelectionModel& selection_model)
    : old_tab(tab),
      new_tab(tab),
      old_contents(tab ? tab->GetContents() : nullptr),
      new_contents(tab ? tab->GetContents() : nullptr),
      old_model(selection_model),
      new_model(selection_model) {}

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

TabGroupChange::CreateChange::CreateChange(
    TabGroupChange::TabGroupCreationReason reason,
    tabs::TabGroupTabCollection* detached_group)
    : reason_(reason), detached_group_(detached_group) {}
TabGroupChange::CreateChange::~CreateChange() = default;

TabGroupChange::CloseChange::CloseChange(
    TabGroupChange::TabGroupClosureReason reason,
    tabs::TabGroupTabCollection* detached_group)
    : reason_(reason), detached_group_(detached_group) {}
TabGroupChange::CloseChange::~CloseChange() = default;

const TabGroupChange::VisualsChange* TabGroupChange::GetVisualsChange() const {
  DCHECK_EQ(type, Type::kVisualsChanged);
  return static_cast<const VisualsChange*>(delta.get());
}

const TabGroupChange::CreateChange* TabGroupChange::GetCreateChange() const {
  DCHECK_EQ(type, Type::kCreated);
  return static_cast<const CreateChange*>(delta.get());
}

std::vector<tabs::TabInterface*> TabGroupChange::CreateChange::GetDetachedTabs()
    const {
  CHECK(detached_group_);
  return detached_group_->GetTabsRecursive();
}

std::vector<tabs::TabInterface*> TabGroupChange::CloseChange::GetDetachedTabs()
    const {
  CHECK(detached_group_);
  return detached_group_->GetTabsRecursive();
}

const TabGroupChange::CloseChange* TabGroupChange::GetCloseChange() const {
  DCHECK_EQ(type, Type::kClosed);
  return static_cast<const CloseChange*>(delta.get());
}

TabGroupChange::TabGroupChange(TabStripModel* model,
                               tab_groups::TabGroupId group,
                               VisualsChange deltap)
    : TabGroupChange(model,
                     group,
                     Type::kVisualsChanged,
                     std::make_unique<VisualsChange>(std::move(deltap))) {}

TabGroupChange::TabGroupChange(TabStripModel* model,
                               tab_groups::TabGroupId group,
                               CreateChange deltap)
    : TabGroupChange(model,
                     group,
                     Type::kCreated,
                     std::make_unique<CreateChange>(std::move(deltap))) {}

TabGroupChange::TabGroupChange(TabStripModel* model,
                               tab_groups::TabGroupId group,
                               CloseChange deltap)
    : TabGroupChange(model,
                     group,
                     Type::kClosed,
                     std::make_unique<CloseChange>(std::move(deltap))) {}

////////////////////////////////////////////////////////////////////////////////
// SplitTabChange
//
SplitTabChange::SplitTabChange(TabStripModel* model,
                               split_tabs::SplitTabId split_id,
                               Type type,
                               std::unique_ptr<Delta> deltap)
    : split_id(split_id), model(model), type(type), delta(std::move(deltap)) {}

SplitTabChange::AddedChange::AddedChange(
    const std::vector<std::pair<tabs::TabInterface*, int>>& tabs,
    SplitTabAddReason reason,
    const split_tabs::SplitTabVisualData& visual_data)
    : tabs_(tabs), reason_(reason), visual_data_(visual_data) {}
SplitTabChange::AddedChange::~AddedChange() = default;
SplitTabChange::AddedChange::AddedChange(const SplitTabChange::AddedChange&) =
    default;

SplitTabChange::VisualsChange::VisualsChange(
    const split_tabs::SplitTabVisualData& old_visual_data,
    const split_tabs::SplitTabVisualData& new_visual_data,
    SplitVisualChangeReason reason)
    : old_visual_data_(old_visual_data),
      new_visual_data_(new_visual_data),
      reason_(reason) {}
SplitTabChange::VisualsChange::~VisualsChange() = default;

SplitTabChange::ContentsChange::ContentsChange(
    const std::vector<std::pair<tabs::TabInterface*, int>>& prev_tabs,
    const std::vector<std::pair<tabs::TabInterface*, int>>& new_tabs)
    : prev_tabs_(prev_tabs), new_tabs_(new_tabs) {}
SplitTabChange::ContentsChange::~ContentsChange() = default;
SplitTabChange::ContentsChange::ContentsChange(
    const SplitTabChange::ContentsChange&) = default;

SplitTabChange::RemovedChange::RemovedChange(
    const std::vector<std::pair<tabs::TabInterface*, int>>& tabs,
    SplitTabRemoveReason reason)
    : tabs_(tabs), reason_(reason) {}
SplitTabChange::RemovedChange::~RemovedChange() = default;
SplitTabChange::RemovedChange::RemovedChange(
    const SplitTabChange::RemovedChange&) = default;

SplitTabChange::SplitTabChange(TabStripModel* model,
                               split_tabs::SplitTabId split_id,
                               AddedChange deltap)
    : SplitTabChange(model,
                     split_id,
                     Type::kAdded,
                     std::make_unique<AddedChange>(std::move(deltap))) {}

SplitTabChange::SplitTabChange(TabStripModel* model,
                               split_tabs::SplitTabId split_id,
                               VisualsChange deltap)
    : SplitTabChange(model,
                     split_id,
                     Type::kVisualsChanged,
                     std::make_unique<VisualsChange>(std::move(deltap))) {}

SplitTabChange::SplitTabChange(TabStripModel* model,
                               split_tabs::SplitTabId split_id,
                               ContentsChange deltap)
    : SplitTabChange(model,
                     split_id,
                     Type::kContentsChanged,
                     std::make_unique<ContentsChange>(std::move(deltap))) {}

SplitTabChange::SplitTabChange(TabStripModel* model,
                               split_tabs::SplitTabId split_id,
                               RemovedChange deltap)
    : SplitTabChange(model,
                     split_id,
                     Type::kRemoved,
                     std::make_unique<RemovedChange>(std::move(deltap))) {}

SplitTabChange::~SplitTabChange() = default;

const SplitTabChange::AddedChange* SplitTabChange::GetAddedChange() const {
  DCHECK_EQ(type, Type::kAdded);
  return static_cast<const AddedChange*>(delta.get());
}

const SplitTabChange::VisualsChange* SplitTabChange::GetVisualsChange() const {
  DCHECK_EQ(type, Type::kVisualsChanged);
  return static_cast<const VisualsChange*>(delta.get());
}

const SplitTabChange::ContentsChange* SplitTabChange::GetContentsChange()
    const {
  DCHECK_EQ(type, Type::kContentsChanged);
  return static_cast<const ContentsChange*>(delta.get());
}

const SplitTabChange::RemovedChange* SplitTabChange::GetRemovedChange() const {
  DCHECK_EQ(type, Type::kRemoved);
  return static_cast<const RemovedChange*>(delta.get());
}

////////////////////////////////////////////////////////////////////////////////
// TabStripModelObserver
//
TabStripModelObserver::TabStripModelObserver() = default;

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

void TabStripModelObserver::OnTabGroupFocusChanged(
    std::optional<tab_groups::TabGroupId> new_focused_group_id,
    std::optional<tab_groups::TabGroupId> old_focused_group_id) {}

void TabStripModelObserver::OnTabGroupAdded(
    const tab_groups::TabGroupId& group_id) {}

void TabStripModelObserver::OnTabGroupWillBeRemoved(
    const tab_groups::TabGroupId& group_id) {}

void TabStripModelObserver::OnSplitTabChanged(const SplitTabChange& change) {}

void TabStripModelObserver::TabChangedAt(WebContents* contents,
                                         int index,
                                         TabChangeType change_type) {}

void TabStripModelObserver::TabPinnedStateChanged(
    TabStripModel* tab_strip_model,
    WebContents* contents,
    int index) {}

void TabStripModelObserver::TabBlockedStateChanged(WebContents* contents,
                                                   int index) {}

void TabStripModelObserver::TabGroupedStateChanged(
    TabStripModel* tab_strip_model,
    std::optional<tab_groups::TabGroupId> old_group,
    std::optional<tab_groups::TabGroupId> new_group,
    tabs::TabInterface* tab,
    int index) {}

void TabStripModelObserver::TabStripEmpty() {}

void TabStripModelObserver::TabCloseCancelled(
    const content::WebContents* contents) {}

void TabStripModelObserver::WillCloseAllTabs(TabStripModel* tab_strip_model) {}

void TabStripModelObserver::CloseAllTabsStopped(TabStripModel* tab_strip_model,
                                                CloseAllStoppedReason reason) {}
void TabStripModelObserver::SetTabNeedsAttentionAt(int index, bool attention) {}
void TabStripModelObserver::SetTabGroupNeedsAttention(
    const tab_groups::TabGroupId& group,
    bool attention) {}
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
