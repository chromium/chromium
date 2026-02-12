// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"

#include <algorithm>

#include "base/stl_util.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_collection_types.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"

namespace {

tabs::ConstChildPtr GetNodeFromHandle(
    const tabs::TabCollection::NodeHandle& handle) {
  if (std::holds_alternative<tabs::TabCollection::Handle>(handle)) {
    const tabs::TabCollection* collection =
        std::get<tabs::TabCollection::Handle>(handle).Get();
    return collection;
  } else {
    CHECK(std::holds_alternative<tabs::TabInterface::Handle>(handle));
    const tabs::TabInterface* tab =
        std::get<tabs::TabInterface::Handle>(handle).Get();
    return tab;
  }
}

}  // namespace

RootTabCollectionNode::RootTabCollectionNode(
    TabStripModel* tab_strip_model,
    CustomAddChildViewCallback add_node_view_to_parent,
    CustomRemoveChildViewCallback remove_node_view_from_parent)
    : TabCollectionNode(tab_strip_model->Root()),
      tab_strip_model_(tab_strip_model),
      add_node_view_to_parent_(add_node_view_to_parent),
      remove_node_view_from_parent_(remove_node_view_from_parent) {}

RootTabCollectionNode::~RootTabCollectionNode() = default;

void RootTabCollectionNode::Init() {
  tab_strip_model_->Root()->AddObserver(this);
  tab_strip_model_->SetTabStripUI(this);
  add_node_view_to_parent_.Run(Initialize());
}

void RootTabCollectionNode::Reset() {
  tab_strip_model_->Root()->RemoveObserver(this);
  tab_strip_model_->RemoveObserver(this);
  Deinitialize();
  views::View* view = std::exchange(node_view_, nullptr);
  remove_node_view_from_parent_.Run(view);
}

base::CallbackListSubscription
RootTabCollectionNode::RegisterOnChildrenAddedCallback(
    base::RepeatingClosure callback) {
  return on_children_added_callback_list_.Add(std::move(callback));
}

void RootTabCollectionNode::OnChildrenAdded(
    const tabs::TabCollection::Position& position,
    const tabs::TabCollectionNodes& handles,
    bool insert_from_detached) {
  for (auto handle : handles) {
    tabs::ConstChildPtr child = GetNodeFromHandle(handle);
    GetNodeForHandle(position.parent_handle)
        ->AddNewChild(GetPassKey(), child, position.index,
                      /*perform_initialization=*/insert_from_detached);
  }
  on_children_added_callback_list_.Notify();
}

void RootTabCollectionNode::OnChildrenRemoved(
    const tabs::TabCollection::Position& position,
    const tabs::TabCollectionNodes& handles) {
  TabCollectionNode* parent_node = GetNodeForHandle(position.parent_handle);
  if (!parent_node) {
    return;
  }

  for (auto& handle : handles) {
    parent_node->RemoveChild(GetPassKey(), handle,
                             /*perform_deinitialization=*/false);
  }
}

void RootTabCollectionNode::OnChildMoved(
    const tabs::TabCollection::Position& to_position,
    const tabs::TabCollectionObserver::NodeData& node_data) {
  const tabs::TabCollection::Position& from_position = node_data.position;
  const tabs::TabCollection::NodeHandle& moved_node_handle = node_data.handle;

  TabCollectionNode* src_parent_node =
      GetNodeForHandle(from_position.parent_handle);
  TabCollectionNode* dst_parent_node =
      GetNodeForHandle(to_position.parent_handle);

  TabCollectionNode* pinned_node = GetChildNodeOfType(Type::PINNED);
  CHECK(pinned_node);

  const bool src_pinned =
      pinned_node->GetNodeForHandle(src_parent_node->GetHandle()) != nullptr;
  const bool dst_pinned =
      pinned_node->GetNodeForHandle(dst_parent_node->GetHandle()) != nullptr;

  bool pin_state_changed = src_pinned != dst_pinned;

  if (pin_state_changed) {
    // Pin state change is treated as a remove and add instead of an attach and
    // detach since we have separate concurrent animations in each container.
    src_parent_node->RemoveChild(GetPassKey(), moved_node_handle,
                                 /*perform_deinitialization=*/false);
    dst_parent_node->AddNewChild(
        GetPassKey(), GetNodeFromHandle(moved_node_handle), to_position.index,
        /*perform_initialization=*/true);
  } else if (src_parent_node == dst_parent_node) {
    // Moves within the same container treated as a reorder of views e.g. within
    // unpinned or group containers.
    TabCollectionNode* parent_node =
        GetNodeForHandle(to_position.parent_handle);
    parent_node->MoveChild(GetPassKey(), moved_node_handle, to_position.index);
  } else {
    // Moves across different containers typically within the unpinned container
    // e.g. unpinned to group, unpinned to split etc.
    TabCollectionNode::MoveChild(GetPassKey(), moved_node_handle,
                                 to_position.index, src_parent_node,
                                 dst_parent_node);
  }
}

void RootTabCollectionNode::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (tab_strip_model->closing_all()) {
    return;
  }

  std::set<tabs::TabInterface*> changed_tabs;

  if (selection.active_tab_changed()) {
    if (selection.old_tab) {
      changed_tabs.insert(selection.old_tab);
    }
    if (selection.new_tab) {
      changed_tabs.insert(selection.new_tab);
    }
  }

  if (selection.selection_changed()) {
    SelectionHandles selected_tabs;
    for (auto index : selection.new_model.selected_indices()) {
      tabs::TabInterface* tab = tab_strip_model->GetTabAtIndex(index);
      selected_tabs.insert(tab->GetHandle());
    }
    auto old_selections =
        base::STLSetDifference<SelectionHandles>(selected_tabs_, selected_tabs);
    auto new_selections =
        base::STLSetDifference<SelectionHandles>(selected_tabs, selected_tabs_);

    for (tabs::TabHandle tab_handle :
         base::STLSetUnion<SelectionHandles>(old_selections, new_selections)) {
      if (auto* tab = tab_handle.Get()) {
        changed_tabs.insert(tab);
      }
    }
    selected_tabs_ = selected_tabs;
  }

  if (change.type() == TabStripModelChange::kReplaced) {
    // Discarding a tab causes a replace change notification to be sent. Add any
    // replaced tab to the list of tabs to update.
    auto* replace = change.GetReplace();
    changed_tabs.insert(tab_strip_model->GetTabAtIndex(replace->index));
  }

  UpdateTabsData(changed_tabs);
}

void RootTabCollectionNode::OnTabGroupChanged(const TabGroupChange& change) {
  if (tab_strip_model_->closing_all()) {
    return;
  }

  TabCollectionNode* group_node =
      GetNodeForHandle(change.model->group_model()
                           ->GetTabGroup(change.group)
                           ->GetCollectionHandle());
  if (!group_node) {
    return;
  }

  if (change.type == TabGroupChange::kVisualsChanged) {
    group_node->NotifyDataChanged();
  } else if (change.type == TabGroupChange::kEditorOpened) {
    group_node->GetController()->ShowGroupEditorBubble(group_node);
  }
}

void RootTabCollectionNode::OnTabGroupFocusChanged(
    std::optional<tab_groups::TabGroupId> new_focused_group_id,
    std::optional<tab_groups::TabGroupId> old_focused_group_id) {
  if (tab_strip_model_->closing_all()) {
    return;
  }

  tab_strip_controller_->TabGroupFocusChanged(new_focused_group_id,
                                              old_focused_group_id);
}

void RootTabCollectionNode::OnTabChangedAt(tabs::TabInterface* tab,
                                           int model_index,
                                           TabChangeType change_type) {
  if (tab_strip_model_->closing_all()) {
    return;
  }

  UpdateTabsData({tab});
}

void RootTabCollectionNode::UpdateTabsData(
    const std::set<tabs::TabInterface*>& changed_tabs) {
  std::set<TabCollectionNode*> nodes_to_notify;

  for (auto* tab : changed_tabs) {
    auto* node = GetNodeForHandle(tab->GetHandle());
    if (!node) {
      continue;
    }

    nodes_to_notify.insert(node);

    // Include all tabs within a split when notifying data change to ensure
    // consistent visual state across the split.
    if (!tab->IsSplit() ||
        !tab_strip_model_->ContainsSplit(tab->GetSplit().value())) {
      continue;
    }

    const auto* split_data =
        tab_strip_model_->GetSplitData(tab->GetSplit().value());

    for (auto* sibling : split_data->ListTabs()) {
      if (auto* sibling_node = GetNodeForHandle(sibling->GetHandle())) {
        nodes_to_notify.insert(sibling_node);
      }
    }
  }

  for (auto* node : nodes_to_notify) {
    node->NotifyDataChanged();
  }
}

void RootTabCollectionNode::NotifyOnChildrenAdded() {
  on_children_added_callback_list_.Notify();
}
