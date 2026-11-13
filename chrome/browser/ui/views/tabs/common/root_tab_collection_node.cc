// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/root_tab_collection_node.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/stl_util.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_collection_types.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"

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
    CustomRemoveChildViewCallback remove_node_view_from_parent,
    TabStripOrientation orientation)
    : TabCollectionNode(tab_strip_model->Root(), orientation),
      tab_strip_model_(tab_strip_model),
      add_node_view_to_parent_(add_node_view_to_parent),
      remove_node_view_from_parent_(remove_node_view_from_parent) {}

RootTabCollectionNode::~RootTabCollectionNode() = default;

void RootTabCollectionNode::Init() {
  tab_strip_model_->Root()->AddObserver(this);
  tab_strip_model_->SetTabStripUI(this);
  std::unique_ptr<views::View> view = Initialize();
  // Index the tree before its views join the view hierarchy, which looks nodes
  // up through this root as views are added (see AddNewChild()).
  RegisterSubtree(this);
  add_node_view_to_parent_.Run(std::move(view));
  UpdateAccessibleTabIndices();
}

void RootTabCollectionNode::Reset() {
  tab_strip_model_->Root()->RemoveObserver(this);
  tab_strip_model_->RemoveObserver(this);
  Deinitialize();
  views::View* view = std::exchange(node_view_, nullptr);
  remove_node_view_from_parent_.Run(view);
}

void RootTabCollectionNode::Deinitialize() {
  // The nodes are about to be destroyed; drop the index first so that it never
  // holds a dangling pointer.
  handle_to_node_map_.clear();
  TabCollectionNode::Deinitialize();
}

TabCollectionNode* RootTabCollectionNode::GetNodeForHandle(
    const tabs::TabCollectionNodeHandle& handle) {
  return const_cast<TabCollectionNode*>(
      std::as_const(*this).GetNodeForHandle(handle));
}

const TabCollectionNode* RootTabCollectionNode::GetNodeForHandle(
    const tabs::TabCollectionNodeHandle& handle) const {
  // The index is filled in Init() and dropped in Deinitialize(); nothing looks
  // nodes up outside that window.
  CHECK(!handle_to_node_map_.empty());
  const auto it = handle_to_node_map_.find(handle);
  return it != handle_to_node_map_.end() ? it->second.get() : nullptr;
}

base::CallbackListSubscription
RootTabCollectionNode::RegisterOnChildrenAddedCallback(
    ChildrenAddedCallback callback) {
  return on_children_added_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
RootTabCollectionNode::RegisterOnChildRemovedCallback(
    base::RepeatingClosure callback) {
  return on_children_removed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
RootTabCollectionNode::RegisterOnChildMovedCallback(
    ChildMovedCallback callback) {
  return on_child_moved_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
RootTabCollectionNode::RegisterOnChildWillBeRemovedCallback(
    RootTabCollectionNode::ChildWillBeRemovedCallback callback) {
  return on_child_will_be_removed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
RootTabCollectionNode::RegisterOnActiveTabChangedCallback(
    RootTabCollectionNode::ActiveTabChangedCallback callback) {
  return on_active_tab_changed_callback_list_.Add(std::move(callback));
}

void RootTabCollectionNode::RegisterSubtree(TabCollectionNode* node) {
  const bool inserted =
      handle_to_node_map_.try_emplace(node->GetHandle(), node).second;
  // Two live nodes never share a handle.
  CHECK(inserted);
  for (const auto& child : node->children()) {
    RegisterSubtree(child.get());
  }
}

void RootTabCollectionNode::UnregisterSubtree(TabCollectionNode* node) {
  for (const auto& child : node->children()) {
    UnregisterSubtree(child.get());
  }
  const size_t erased = handle_to_node_map_.erase(node->GetHandle());
  CHECK_EQ(erased, 1u);
}

void RootTabCollectionNode::OnChildrenAdded(
    const tabs::TabCollection::Position& position,
    const tabs::TabCollectionNodes& handles,
    bool insert_from_detached) {
  auto animate_layout = GetController()->ScopedAnimateLayout();
  for (auto handle : handles) {
    tabs::ConstChildPtr child = GetNodeFromHandle(handle);
    GetNodeForHandle(position.parent_handle)
        ->AddNewChild(GetPassKey(), child, position.index,
                      /*perform_initialization=*/insert_from_detached,
                      base::BindOnce(&RootTabCollectionNode::RegisterSubtree,
                                     base::Unretained(this)));
  }
  UpdateAccessibleTabIndices();
  on_children_added_callback_list_.Notify(handles);
}

void RootTabCollectionNode::OnChildrenRemoved(
    const tabs::TabCollection::Position& position,
    const tabs::TabCollectionNodes& handles,
    bool remove_for_detach) {
  TabCollectionNode* parent_node = GetNodeForHandle(position.parent_handle);
  if (!parent_node) {
    return;
  }

  auto animate_layout = GetController()->ScopedAnimateLayout();
  for (auto& handle : handles) {
    TabCollectionNode* child_node = GetNodeForHandle(handle);
    if (child_node) {
      on_child_will_be_removed_callback_list_.Notify(child_node);
      // RemoveChild() destroys the node and its subtree; drop them from the
      // index first so that no lookup can return a dying node.
      UnregisterSubtree(child_node);
    }
    parent_node->RemoveChild(GetPassKey(), handle,
                             /*perform_deinitialization=*/false,
                             remove_for_detach);
  }
  UpdateAccessibleTabIndices();
  on_children_removed_callback_list_.Notify();
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

  const bool src_pinned = pinned_node->Contains(src_parent_node->GetHandle());
  const bool dst_pinned = pinned_node->Contains(dst_parent_node->GetHandle());

  bool pin_state_changed = src_pinned != dst_pinned;

  auto animate_layout = GetController()->ScopedAnimateLayout();

  if (pin_state_changed) {
    // Pin state change is treated as a remove and add instead of an attach and
    // detach since we have separate concurrent animations in each container.
    // The index follows: the old node leaves it, the new one enters it.
    TabCollectionNode* moved_node = GetNodeForHandle(moved_node_handle);
    CHECK(moved_node);
    UnregisterSubtree(moved_node);
    src_parent_node->RemoveChild(GetPassKey(), moved_node_handle,
                                 /*perform_deinitialization=*/false);
    dst_parent_node->AddNewChild(
        GetPassKey(), GetNodeFromHandle(moved_node_handle), to_position.index,
        /*perform_initialization=*/true,
        base::BindOnce(&RootTabCollectionNode::RegisterSubtree,
                       base::Unretained(this)));
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

  TabCollectionNode* final_moved_node = GetNodeForHandle(moved_node_handle);
  UpdateAccessibleTabIndices();
  on_child_moved_callback_list_.Notify(final_moved_node);
}

void RootTabCollectionNode::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (tab_strip_model->closing_all()) {
    on_active_tab_changed_callback_list_.Notify(nullptr);
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
    on_active_tab_changed_callback_list_.Notify(selection.new_tab);
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

  NotifyTabSelectionChanged(changed_tabs);
}

void RootTabCollectionNode::OnTabWillBeAdded() {
  GetController()->GetDragHandler().OnTabWillBeAdded();
}

void RootTabCollectionNode::OnTabWillBeRemoved(tabs::TabHandle tab) {
  if (tabs::TabInterface* tab_interface = tab.Get()) {
    GetController()->GetDragHandler().OnTabWillBeRemoved(
        tab_interface->GetContents());
  }
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

  if (change.type == TabGroupChange::kEditorOpened) {
    group_node->GetController()->ShowGroupEditorBubble(group_node);
  } else if (change.type == TabGroupChange::kVisualsChanged) {
    // If the group whose visual data (e.g., color) changed is currently
    // focused, update the focus mode theme color.
    if (tab_strip_model_->GetFocusedGroup() == change.group) {
      if (auto* controller = group_node->GetController()) {
        controller->UpdateFocusModeTheme(change.group);
      }
    }
  }
}

void RootTabCollectionNode::OnTabGroupFocusChanged(
    std::optional<tab_groups::TabGroupId> new_focused_group_id,
    std::optional<tab_groups::TabGroupId> old_focused_group_id) {
  if (tab_strip_model_->closing_all()) {
    return;
  }

  auto animate_layout = GetController()->ScopedAnimateLayout();
  tab_strip_controller_->TabGroupFocusChanged(new_focused_group_id,
                                              old_focused_group_id);

  if (view()) {
    view()->InvalidateLayout();
  }
}

void RootTabCollectionNode::NotifyTabSelectionChanged(
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
    node->NotifyTabSelectionChanged();
  }
}

void RootTabCollectionNode::UpdateAccessibleTabIndices() {
  if (tab_strip_model_->closing_all()) {
    return;
  }

  const int num_tabs = tab_strip_model_->count();
  int pos_in_set = 0;
  auto update_indices = [&](this const auto& self,
                            TabCollectionNode& node) -> void {
    if (node.type() == TabCollectionNode::Type::TAB) {
      node.UpdateAccessibleTabIndex(++pos_in_set, num_tabs);
      return;
    }
    for (const auto& child : node.children()) {
      self(*child);
    }
  };
  update_indices(*this);
}
