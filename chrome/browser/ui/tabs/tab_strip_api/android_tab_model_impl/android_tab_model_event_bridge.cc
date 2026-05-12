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
  last_selected_tab_ = active_tab->GetHandle();
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
  tabs::TabHandle new_selected_tab = tab->GetHandle();
  // TODO(crbug.com/509647569): Add additional plumbing to propagate the
  // previously selected tabs.
  if (last_selected_tab_ == new_selected_tab) {
    return;
  }

  // Last selected tab might've been closed.
  if (last_selected_tab_.Get()) {
    auto old_tab_or_error = translation_adapter_->ToMojoTab(last_selected_tab_);
    CHECK(old_tab_or_error.has_value()) << old_tab_or_error.error()->message;

    auto deselection_change = mojom::TabChange::New();
    deselection_change->data = old_tab_or_error.value().Clone();
    deselection_change->mask = mojom::TabFieldMask::New();
    deselection_change->mask->is_active = true;
    deselection_change->mask->is_selected = true;
    Notify(mojom::OnDataChangedEvent::NewTab(std::move(deselection_change)));
  }

  last_selected_tab_ = new_selected_tab;

  auto tab_or_error = translation_adapter_->ToMojoTab(new_selected_tab);
  CHECK(tab_or_error.has_value()) << tab_or_error.error()->message;

  auto tab_change = mojom::TabChange::New();
  tab_change->data = tab_or_error.value().Clone();
  tab_change->mask = mojom::TabFieldMask::New();
  tab_change->mask->is_active = true;
  tab_change->mask->is_selected = true;
  Notify(mojom::OnDataChangedEvent::NewTab(std::move(tab_change)));
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
