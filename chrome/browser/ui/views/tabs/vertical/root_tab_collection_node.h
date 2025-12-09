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

// The viewmodel for the VerticalTabStrip.
class RootTabCollectionNode : public TabCollectionNode,
                              public tabs::TabCollectionObserver,
                              public TabStripModelObserver {
 public:
  explicit RootTabCollectionNode(
      TabStripModel* tab_strip_model,
      CustomAddChildViewCallback add_node_view_to_parent);
  ~RootTabCollectionNode() override;

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
  void TabChangedAt(content::WebContents* contents,
                    int model_index,
                    TabChangeType change_type) override;
  void TabPinnedStateChanged(TabStripModel* tab_strip_model,
                             content::WebContents* contents,
                             int model_index) override;
  void TabBlockedStateChanged(content::WebContents* contents,
                              int model_index) override;

 private:
  using SelectionHandles = base::flat_set<tabs::TabHandle>;
  void UpdateTabData(content::WebContents* contents, int model_index);

  raw_ptr<TabStripModel> tab_strip_model_;
  SelectionHandles selected_tabs_;
  base::WeakPtrFactory<RootTabCollectionNode> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ROOT_TAB_COLLECTION_NODE_H_
