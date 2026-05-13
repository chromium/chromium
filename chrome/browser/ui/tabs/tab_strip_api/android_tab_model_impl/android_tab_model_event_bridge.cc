// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_tab_model_event_bridge.h"

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/translation_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_tab_strip_model_adapter.h"

namespace tabs_api {

AndroidTabModelEventBridge::AndroidTabModelEventBridge(
    TabModel* model,
    AndroidTabStripModelAdapter& adapter,
    TranslationAdapter& translation_adapter)
    : model_(model),
      adapter_(adapter),
      translation_adapter_(translation_adapter) {
  auto* active_tab = model_->GetActiveTab();
  CHECK(active_tab)
      << "TabStrip API assumes that there is always at least one selected tab.";
  last_active_tab_ = active_tab->GetHandle();
  last_selection_ = model_->GetOrderedMultiSelectedTabs();
  model_->AddObserver(this);
  adapter_->GetRoot()->AddObserver(this);
}

AndroidTabModelEventBridge::~AndroidTabModelEventBridge() {
  model_->RemoveObserver(this);
  adapter_->GetRoot()->RemoveObserver(this);
}

void AndroidTabModelEventBridge::AddObserver(events::EventObserver* observer) {
  observers_.AddObserver(observer);
}

void AndroidTabModelEventBridge::RemoveObserver(
    events::EventObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AndroidTabModelEventBridge::Notify(events::Event event) const {
  for (auto& observer : observers_) {
    std::visit([&observer](const auto& e) { observer.OnEvent(e.Clone()); },
               event);
  }
}

void AndroidTabModelEventBridge::DidSelectTab(TabAndroid* tab,
                                              TabModel::TabSelectionType type) {
  // Note that the inputs for this event are dropped. The API does not
  // currently need it, but this may change in the future.
  HandleSelectionAndActivationChange();
}

void AndroidTabModelEventBridge::OnTabsSelectionsChanged() {
  HandleSelectionAndActivationChange();
}

void AndroidTabModelEventBridge::HandleSelectionAndActivationChange() {
  tabs::TabHandle new_active_tab = model_->GetActiveTab()->GetHandle();
  bool active_tab_changed = new_active_tab != last_active_tab_;

  // Note that the API currently doesn't care about ordering, but this may
  // change in the future.
  auto new_selection = model_->GetOrderedMultiSelectedTabs();
  auto new_selection_set = std::set(new_selection.begin(), new_selection.end());
  auto old_selection_set =
      std::set(last_selection_.begin(), last_selection_.end());
  std::vector<tabs::TabHandle> diff;
  std::set_symmetric_difference(
      new_selection_set.begin(), new_selection_set.end(),
      old_selection_set.begin(), old_selection_set.end(),
      std::back_inserter(diff));
  bool selection_changed = !diff.empty();

  if (!active_tab_changed && !selection_changed) {
    return;
  }

  std::vector<tabs::TabHandle> tabs_changed;
  tabs_changed.reserve(diff.size() + /* activation change */ 2);
  std::set<tabs::TabHandle> inserted;
  // Technically, it is probably ok to have the deactivation in the end, because
  // we only guarantee that the world is in a good state after the list of
  // events have been applied, but we'll put it in the front anyway.
  if (active_tab_changed) {
    tabs_changed.push_back(last_active_tab_);
    inserted.insert(last_active_tab_);
    tabs_changed.push_back(new_active_tab);
    inserted.insert(new_active_tab);
  }
  for (auto& tab : diff) {
    if (!inserted.contains(tab)) {
      tabs_changed.push_back(tab);
      inserted.insert(tab);
    }
  }

  for (auto& tab_changed : tabs_changed) {
    // Tab might have been changed since the last time we grabbed the handle.
    if (tab_changed.Get()) {
      auto tab_or_error = translation_adapter_->ToMojoTab(tab_changed);
      CHECK(tab_or_error.has_value()) << tab_or_error.error()->message;

      auto change_event = mojom::TabChange::New();
      change_event->data = tab_or_error.value().Clone();
      change_event->mask = mojom::TabFieldMask::New();
      // Technically this might not have changed. The selected state might not
      // have been changed even if the activation state changed. But the general
      // semantic of the field mask is that the masked fields are meaningful. If
      // this is super confusing using git blame to find out the fool that did
      // this.
      change_event->mask->is_active = true;
      change_event->mask->is_selected = true;
      Notify(mojom::OnDataChangedEvent::NewTab(std::move(change_event)));
    }
  }

  // Now we update our internal book keeping.
  last_active_tab_ = new_active_tab;
  last_selection_ = new_selection;
}

void AndroidTabModelEventBridge::DidAddTab(TabAndroid* tab,
                                           TabModel::TabLaunchType type) {
  auto tab_or_error = translation_adapter_->ToMojoTab(tab->GetHandle());
  CHECK(tab_or_error.has_value()) << tab_or_error.error()->message;

  auto event = mojom::OnTabsCreatedEvent::New();
  auto created_container = mojom::TabCreatedContainer::New();
  created_container->tab = tab_or_error.value().Clone();
  int index = model_->GetIndexOfTab(tab->GetHandle());
  created_container->position = adapter_->GetPositionForAbsoluteIndex(index);
  event->tabs.push_back(std::move(created_container));
  Notify(std::move(event));
}

void AndroidTabModelEventBridge::OnChildMoved(
    const tabs::TabCollection::Position& to_position,
    const NodeData& node_data) {
  auto event = mojom::OnNodeMovedEvent::New();

  if (std::holds_alternative<tabs::TabHandle>(node_data.handle)) {
    tabs::TabHandle tab_handle = std::get<tabs::TabHandle>(node_data.handle);
    event->id = NodeId::FromTabHandle(tab_handle);
  } else {
    tabs::TabCollection::Handle collection_handle =
        std::get<tabs::TabCollection::Handle>(node_data.handle);
    event->id = NodeId::FromTabCollectionHandle(collection_handle);
  }

  event->from = tabs_api::Position(
      node_data.position.index,
      adapter_->GetPathForCollection(node_data.position.parent_handle));
  event->to = tabs_api::Position(
      to_position.index,
      adapter_->GetPathForCollection(to_position.parent_handle));

  Notify(std::move(event));
}

void AndroidTabModelEventBridge::DidRemoveTabForClosure(TabAndroid* tab) {
  auto event = mojom::OnNodesClosedEvent::New();
  event->node_ids.push_back(NodeId::FromTabHandle(tab->GetHandle()));
  Notify(std::move(event));
}

void AndroidTabModelEventBridge::TabRemoved(TabAndroid* tab) {
  auto event = mojom::OnNodesClosedEvent::New();
  event->node_ids.push_back(NodeId::FromTabHandle(tab->GetHandle()));
  Notify(std::move(event));
}

void AndroidTabModelEventBridge::OnTabGroupCreated(
    tab_groups::TabGroupId group_id) {
  auto collection_handle = adapter_->GetCollectionHandleForTabGroupId(group_id);
  auto data_or_error = translation_adapter_->ToMojoData(collection_handle);
  CHECK(data_or_error.has_value()) << data_or_error.error()->message;

  auto event = mojom::OnCollectionCreatedEvent::New();

  auto* parent = collection_handle.Get()->GetParentCollection();
  CHECK(parent);

  auto relative_idx = parent->GetIndexOfCollection(collection_handle.Get());
  CHECK(relative_idx.has_value());

  event->position =
      tabs_api::Position(relative_idx.value(),
                         adapter_->GetPathForCollection(parent->GetHandle()));
  event->collection = mojom::Container::New();
  event->collection->data = data_or_error.value().Clone();
  Notify(std::move(event));
}

void AndroidTabModelEventBridge::OnTabGroupRemoving(
    tab_groups::TabGroupId group_id) {
  auto event = mojom::OnNodesClosedEvent::New();
  auto collection_handle = adapter_->GetCollectionHandleForTabGroupId(group_id);
  event->node_ids.push_back(NodeId::FromTabCollectionHandle(collection_handle));
  Notify(std::move(event));
}

void AndroidTabModelEventBridge::OnTabGroupVisualsChanged(
    tab_groups::TabGroupId group_id) {
  auto collection_handle = adapter_->GetCollectionHandleForTabGroupId(group_id);
  auto data_or_error = translation_adapter_->ToMojoData(collection_handle);
  CHECK(data_or_error.has_value()) << data_or_error.error()->message;

  auto group_change = mojom::TabGroupChange::New();
  group_change->data = data_or_error.value()->get_tab_group().Clone();
  Notify(mojom::OnDataChangedEvent::NewTabGroup(std::move(group_change)));
}

}  // namespace tabs_api
