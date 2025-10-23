// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ROOT_TAB_COLLECTION_NODE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ROOT_TAB_COLLECTION_NODE_H_

#include "base/scoped_observation.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/tabs/tab_strip_api/observation/tab_strip_api_observer.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "ui/views/view.h"

namespace tabs_api {
class TabStripService;
}

// The RootTabCollectionNode is the entry point for the TabStripAPI. It is
// responsible for fetching the initial tab state and listening for updates.
class RootTabCollectionNode
    : public TabCollectionNode,
      public tabs_api::observation::TabStripApiObserver {
 public:
  explicit RootTabCollectionNode(
      tabs_api::TabStripService* service_register,
      CustomAddChildViewCallback add_node_view_to_parent);
  ~RootTabCollectionNode() override;

  // tabs_api::observation::TabStripApiObserver
  void OnTabsCreated(const tabs_api::mojom::OnTabsCreatedEventPtr&
                         tabs_created_event) override;
  void OnTabsClosed(
      const tabs_api::mojom::OnTabsClosedEventPtr& tabs_closed_event) override;
  void OnNodeMoved(
      const tabs_api::mojom::OnNodeMovedEventPtr& node_moved_event) override;
  void OnDataChanged(const tabs_api::mojom::OnDataChangedEventPtr&
                         data_changed_event) override;
  void OnCollectionCreated(const tabs_api::mojom::OnCollectionCreatedEventPtr&
                               collection_created_event) override;

 private:
  // TabCollectionNode needs to be initialized with data, however we need the
  // container's children later in the constructor of RootTabCollectionNode.
  // Use this helper so that we only have to call GetTabs once.
  explicit RootTabCollectionNode(
      tabs_api::TabStripService* tab_strip_service,
      tabs_api::mojom::ContainerPtr container,
      CustomAddChildViewCallback add_node_view_to_parent);

  base::ScopedObservation<tabs_api::TabStripService,
                          tabs_api::observation::TabStripApiObserver>
      service_observer_{this};
  base::WeakPtrFactory<RootTabCollectionNode> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ROOT_TAB_COLLECTION_NODE_H_
