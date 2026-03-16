// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/tab_strip_model_event_bridge.h"

#include "base/notimplemented.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/converters/event_transformation.h"

namespace tabs_api::tab_strip_model {

TabStripModelEventBridge::TabStripModelEventBridge(
    TabStripModelAdapterImpl& tab_strip_model_adapter)
    : tab_strip_model_adapter_(tab_strip_model_adapter) {}

TabStripModelEventBridge::~TabStripModelEventBridge() = default;

void TabStripModelEventBridge::AddObserver(events::EventObserver* observer) {
  CHECK(!observer_to_bridge_.contains(observer))
      << "cannot re-add the same observer";
  observer_to_bridge_.emplace(
      observer,
      std::make_unique<BridgeInstance>(*tab_strip_model_adapter_, observer));
}

void TabStripModelEventBridge::RemoveObserver(events::EventObserver* observer) {
  CHECK(observer_to_bridge_.contains(observer))
      << "observer has not been registered";
  observer_to_bridge_.erase(observer);
}

BridgeInstance::BridgeInstance(
    TabStripModelAdapterImpl& tab_strip_model_adapter,
    events::EventObserver* observer)
    : tab_strip_model_adapter_(tab_strip_model_adapter), observer_(observer) {
  tab_strip_model_adapter_->AddModelObserver(this);
  tab_strip_model_adapter_->AddCollectionObserver(this);
}

BridgeInstance::~BridgeInstance() {
  tab_strip_model_adapter_->RemoveModelObserver(this);
  tab_strip_model_adapter_->RemoveCollectionObserver(this);
}

void BridgeInstance::OnChildrenAdded(
    const tabs::TabCollection::Position& position,
    const tabs::TabCollectionNodes& handles,
    bool insert_from_detached) {
  for (const auto& handle : handles) {
    if (const auto* tab_handle_ptr = std::get_if<tabs::TabHandle>(&handle)) {
      if (!tab_handle_ptr->Get()) {
        continue;
      }
      ForwardToObserver(events::ToEvent(*tab_handle_ptr, position,
                                        *tab_strip_model_adapter_));
    } else if (const auto* collection_handle_ptr =
                   std::get_if<tabs::TabCollectionHandle>(&handle)) {
      if (!collection_handle_ptr->Get()) {
        continue;
      }
      ForwardToObserver(events::ToEvent(*collection_handle_ptr, position,
                                        *tab_strip_model_adapter_,
                                        insert_from_detached));
    }
  }
}

void BridgeInstance::OnChildrenRemoved(
    const tabs::TabCollection::Position& position,
    const tabs::TabCollectionNodes& handles) {
  ForwardToObserver(events::ToEvent(handles));
}

void BridgeInstance::OnChildMoved(
    const tabs::TabCollection::Position& to_position,
    const NodeData& node_data) {
  const tabs::TabCollection::Position& from_position = node_data.position;
  const tabs::TabCollection::NodeHandle node_handle = node_data.handle;

  ForwardToObserver(events::ToEvent(to_position, from_position, node_handle,
                                    *tab_strip_model_adapter_));
}

void BridgeInstance::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // Avoid listening to add, remove and move changes as this is handled by the
  // TabCollection observation methods.
  if (change.type() == TabStripModelChange::Type::kReplaced) {
    NOTIMPLEMENTED();
  }

  if (selection.active_tab_changed() || selection.selection_changed()) {
    ForwardToObserver(events::ToEvent(selection, *tab_strip_model_adapter_));
  }
}

void BridgeInstance::OnTabChangedAt(tabs::TabInterface* tab,
                                    int index,
                                    TabChangeType change_type) {
  ForwardToObserver(
      events::ToEvent(*tab_strip_model_adapter_, index, change_type));
}

void BridgeInstance::OnTabGroupChanged(const TabGroupChange& change) {
  if (change.type == TabGroupChange::Type::kEditorOpened) {
    NOTIMPLEMENTED();
    return;
  }

  if (change.type == TabGroupChange::Type::kVisualsChanged) {
    ForwardToObserver(events::ToEvent(change, *tab_strip_model_adapter_));
    return;
  }
}

void BridgeInstance::OnSplitTabChanged(const SplitTabChange& change) {
  if (change.type == SplitTabChange::Type::kVisualsChanged) {
    ForwardToObserver(events::ToEvent(change, *tab_strip_model_adapter_));
  }
}

void BridgeInstance::ForwardToObserver(events::Event event) {
  observer_->OnEvent(std::move(event));
}

void BridgeInstance::ForwardToObserver(std::vector<events::Event> event) {
  observer_->OnEvents(std::move(event));
}

}  // namespace tabs_api::tab_strip_model
