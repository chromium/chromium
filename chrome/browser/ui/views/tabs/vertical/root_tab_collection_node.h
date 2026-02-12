// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ROOT_TAB_COLLECTION_NODE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ROOT_TAB_COLLECTION_NODE_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "components/tabs/public/tab_collection_observer.h"
#include "ui/views/view.h"

class TabStripModel;

// The view model for the tab strip. It is responsible for observing
// the tab collection hierarchy changes and tab selection and activation changes
// and creates the view hierarchy.
class RootTabCollectionNode : public TabCollectionNode,
                              public tabs::TabCollectionObserver,
                              public TabStripModelObserver {
 public:
  explicit RootTabCollectionNode(
      TabStripModel* tab_strip_model,
      CustomAddChildViewCallback add_node_view_to_parent,
      CustomRemoveChildViewCallback remove_node_view_from_parent);
  ~RootTabCollectionNode() override;

  void Init();
  void Reset();

  base::CallbackListSubscription RegisterOnChildrenAddedCallback(
      base::RepeatingClosure callback);

 private:
  using SelectionHandles = base::flat_set<tabs::TabHandle>;

  // tabs::TabCollectionObserver
  void OnChildrenAdded(const tabs::TabCollection::Position& position,
                       const tabs::TabCollectionNodes& handles,
                       bool insert_from_detached) override;
  void OnChildrenRemoved(const tabs::TabCollection::Position& position,
                         const tabs::TabCollectionNodes& handles) override;
  void OnChildMoved(const tabs::TabCollection::Position& to_position,
                    const NodeData& node_data) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabGroupChanged(const TabGroupChange& change) override;
  void OnTabGroupFocusChanged(
      std::optional<tab_groups::TabGroupId> new_focused_group_id,
      std::optional<tab_groups::TabGroupId> old_focused_group_id) override;
  void OnTabChangedAt(tabs::TabInterface* tab,
                      int model_index,
                      TabChangeType change_type) override;

  void UpdateTabsData(const std::set<tabs::TabInterface*>& changed_tabs);

  void NotifyOnChildrenAdded();

  raw_ptr<TabStripModel> tab_strip_model_;
  SelectionHandles selected_tabs_;
  CustomAddChildViewCallback add_node_view_to_parent_;
  CustomRemoveChildViewCallback remove_node_view_from_parent_;
  base::RepeatingClosureList on_children_added_callback_list_;
  base::WeakPtrFactory<RootTabCollectionNode> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ROOT_TAB_COLLECTION_NODE_H_
