// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/tab_strip_model_event_bridge.h"

#include "base/notimplemented.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/converters/event_transformation.h"

namespace tabs_api::tab_strip_model {

TabStripModelEventBridge::TabStripModelEventBridge(
    TabStripModelAdapterImpl& tab_strip_model_adapter)
    : tab_strip_model_adapter_(tab_strip_model_adapter) {
  tab_strip_model_adapter_->AddModelObserver(this);
  tab_strip_model_adapter_->AddCollectionObserver(this);
}

TabStripModelEventBridge::~TabStripModelEventBridge() {
  tab_strip_model_adapter_->RemoveModelObserver(this);
  tab_strip_model_adapter_->RemoveCollectionObserver(this);
}

void TabStripModelEventBridge::AddObserver(events::EventObserver* observer) {
  observers_.AddObserver(observer);
}

void TabStripModelEventBridge::RemoveObserver(events::EventObserver* observer) {
  observers_.RemoveObserver(observer);
}

void TabStripModelEventBridge::Notify(const events::Event& event) const {
  for (auto& observer : observers_) {
    std::visit([&observer](const auto& e) { observer.OnEvent(e.Clone()); },
               event);
  }
}

void TabStripModelEventBridge::Notify(
    const std::vector<events::Event>& events) const {
  for (auto& event : events) {
    Notify(event);
  }
}

void TabStripModelEventBridge::OnChildrenAdded(
    const tabs::TabCollection::Position& position,
    const tabs::TabCollectionNodes& handles,
    bool insert_from_detached) {
  for (const auto& handle : handles) {
    if (const auto* tab_handle_ptr = std::get_if<tabs::TabHandle>(&handle)) {
      if (!tab_handle_ptr->Get()) {
        continue;
      }
      Notify(events::ToEvent(*tab_handle_ptr, position,
                             *tab_strip_model_adapter_));
    } else if (const auto* collection_handle_ptr =
                   std::get_if<tabs::TabCollectionHandle>(&handle)) {
      if (!collection_handle_ptr->Get()) {
        continue;
      }
      Notify(events::ToEvent(*collection_handle_ptr, position,
                             *tab_strip_model_adapter_, insert_from_detached));
    }
  }
}

void TabStripModelEventBridge::OnChildrenRemoved(
    const tabs::TabCollection::Position& position,
    const tabs::TabCollectionNodes& handles) {
  tabs::TabCollectionNodes filtered_handles;
  for (const auto& handle : handles) {
    // We only forward TabCollectionHandles because tab removal events do not
    // bubble up to the root when their parent collection is detached from the
    // tree (e.g. closing the last tab in a group removes the group first).
    // Tab removals are instead handled via OnTabStripModelChanged.
    if (std::holds_alternative<tabs::TabCollectionHandle>(handle)) {
      filtered_handles.push_back(handle);
    }
  }
  Notify(events::ToEvent(filtered_handles));
}

void TabStripModelEventBridge::OnChildMoved(
    const tabs::TabCollection::Position& to_position,
    const NodeData& node_data) {
  const tabs::TabCollection::Position& from_position = node_data.position;
  const tabs::TabCollection::NodeHandle node_handle = node_data.handle;

  Notify(events::ToEvent(to_position, from_position, node_handle,
                         *tab_strip_model_adapter_));
}

void TabStripModelEventBridge::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::Type::kRemoved) {
    const TabStripModelChange::Remove* remove = change.GetRemove();
    tabs::TabCollectionNodes removed_nodes;
    for (const auto& contents : remove->contents) {
      removed_nodes.push_back(contents.tab->GetHandle());
    }
    Notify(events::ToEvent(removed_nodes));
  }
  // Avoid listening to add and move changes as this is handled by the
  // TabCollection observation methods.
  if (change.type() == TabStripModelChange::Type::kReplaced) {
    NOTIMPLEMENTED();
  }

  if (selection.active_tab_changed() || selection.selection_changed()) {
    Notify(events::ToEvent(selection, *tab_strip_model_adapter_));
  }
}

void TabStripModelEventBridge::OnTabChangedAt(tabs::TabInterface* tab,
                                              int index,
                                              TabChangeType change_type) {
  Notify(events::ToEvent(*tab_strip_model_adapter_, index, change_type));
}

void TabStripModelEventBridge::OnTabGroupChanged(const TabGroupChange& change) {
  if (change.type == TabGroupChange::Type::kEditorOpened) {
    NOTIMPLEMENTED();
    return;
  }

  if (change.type == TabGroupChange::Type::kVisualsChanged) {
    Notify(events::ToEvent(change, *tab_strip_model_adapter_));
    return;
  }
}

void TabStripModelEventBridge::OnSplitTabChanged(const SplitTabChange& change) {
  if (change.type == SplitTabChange::Type::kVisualsChanged) {
    Notify(events::ToEvent(change, *tab_strip_model_adapter_));
  }
}

}  // namespace tabs_api::tab_strip_model
