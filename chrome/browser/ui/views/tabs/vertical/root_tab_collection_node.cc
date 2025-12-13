// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"

#include <algorithm>

#include "base/stl_util.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_view.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_collection_types.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"

RootTabCollectionNode::RootTabCollectionNode(
    TabStripModel* tab_strip_model,
    CustomAddChildViewCallback add_node_view_to_parent)
    : TabCollectionNode(tab_strip_model->Root()),
      tab_strip_model_(tab_strip_model) {
  tab_strip_model_->Root()->AddObserver(this);
  tab_strip_model_->AddObserver(this);
  add_node_view_to_parent.Run(Initialize());
}

RootTabCollectionNode::~RootTabCollectionNode() {
  if (tab_strip_model_) {
    tab_strip_model_->Root()->RemoveObserver(this);
    tab_strip_model_->RemoveObserver(this);
  }
}

void RootTabCollectionNode::OnChildrenAdded(
    const tabs::TabCollection::Position& position,
    const tabs::TabCollectionNodes& handles,
    bool insert_from_detached) {
  for (auto handle : handles) {
    tabs::ConstChildPtr child;
    if (std::holds_alternative<tabs::TabCollection::Handle>(handle)) {
      const tabs::TabCollection* collection =
          std::get<tabs::TabCollection::Handle>(handle).Get();
      child = collection;
    } else {
      CHECK(std::holds_alternative<tabs::TabInterface::Handle>(handle));
      const tabs::TabInterface* tab =
          std::get<tabs::TabInterface::Handle>(handle).Get();
      child = tab;
    }

    GetNodeForHandle(position.parent_handle)
        ->AddNewChild(GetPassKey(), child, position.index,
                      insert_from_detached);
  }
}

void RootTabCollectionNode::OnChildrenRemoved(
    const tabs::TabCollection::Position& position,
    const tabs::TabCollectionNodes& handles) {
  TabCollectionNode* parent_node = GetNodeForHandle(position.parent_handle);
  if (!parent_node) {
    return;
  }

  for (auto& handle : handles) {
    parent_node->RemoveChild(GetPassKey(), handle);
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

  auto [view, node] =
      src_parent_node->RemoveChild(GetPassKey(), moved_node_handle);
  dst_parent_node->AddChild(std::move(view), std::move(node),
                            to_position.index);
}

void RootTabCollectionNode::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (tab_strip_model->closing_all()) {
    return;
  }

  std::set<TabCollectionNode*> selection_changes;

  if (selection.active_tab_changed()) {
    if (selection.old_tab) {
      TabCollectionNode* old_tab_node =
          GetNodeForHandle(selection.old_tab->GetHandle());
      if (old_tab_node) {
        selection_changes.insert(old_tab_node);
      }
    }
    if (selection.new_tab) {
      TabCollectionNode* new_tab_node =
          GetNodeForHandle(selection.new_tab->GetHandle());
      if (new_tab_node) {
        selection_changes.insert(new_tab_node);
      }
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

    for (auto tab_handle :
         base::STLSetUnion<SelectionHandles>(old_selections, new_selections)) {
      TabCollectionNode* tab_node = GetNodeForHandle(tab_handle);
      if (tab_node) {
        selection_changes.insert(tab_node);
      }
    }
    selected_tabs_ = selected_tabs;
  }

  for (auto* tab_node : selection_changes) {
    tab_node->NotifyDataChanged();
  }
}

void RootTabCollectionNode::OnTabGroupChanged(const TabGroupChange& change) {
  if (tab_strip_model_->closing_all()) {
    return;
  }

  if (change.type != TabGroupChange::kVisualsChanged) {
    return;
  }

  TabCollectionNode* group_node =
      GetNodeForHandle(change.model->group_model()
                           ->GetTabGroup(change.group)
                           ->GetCollectionHandle());
  if (group_node) {
    group_node->NotifyDataChanged();
  }
}

void RootTabCollectionNode::TabChangedAt(content::WebContents* contents,
                                         int model_index,
                                         TabChangeType change_type) {
  if (tab_strip_model_->closing_all()) {
    return;
  }

  UpdateTabData(contents, model_index);
}

void RootTabCollectionNode::TabPinnedStateChanged(
    TabStripModel* tab_strip_model,
    content::WebContents* contents,
    int model_index) {
  CHECK_EQ(tab_strip_model, tab_strip_model_);
  UpdateTabData(contents, model_index);
}

void RootTabCollectionNode::TabBlockedStateChanged(
    content::WebContents* contents,
    int model_index) {
  UpdateTabData(contents, model_index);
}

void RootTabCollectionNode::UpdateTabData(content::WebContents* contents,
                                          int model_index) {
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(contents);
  TabCollectionNode* tab_node = GetNodeForHandle(tab->GetHandle());
  if (tab_node) {
    tab_node->NotifyDataChanged();
  }
}
