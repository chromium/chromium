// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ROOT_TAB_COLLECTION_NODE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ROOT_TAB_COLLECTION_NODE_H_

#include "base/types/expected.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/views/view.h"

namespace tabs_api::mojom {
class TabStripService;
}

class TabStripServiceRegister;

// The RootTabCollectionNode is the entry point for the TabStripAPI. It is
// responsible for fetching the initial tab state and listening for updates.
class RootTabCollectionNode : public TabCollectionNode,
                              public tabs_api::mojom::TabsObserver {
 public:
  explicit RootTabCollectionNode(TabStripServiceRegister* service_register,
                                 views::View* parent_view,
                                 CustomAddChildView add_node_to_parent_);
  ~RootTabCollectionNode() override;

  // tabs_api::mojom::TabsObserver:
  void OnTabEvents(std::vector<tabs_api::mojom::TabsEventPtr> events) override;

 private:
  void OnGetTabs(views::View* parent_view,
                 base::expected<tabs_api::mojom::TabsSnapshotPtr,
                                mojo_base::mojom::ErrorPtr> result);

  mojo::Remote<tabs_api::mojom::TabStripService> remote_;
  mojo::AssociatedReceiver<tabs_api::mojom::TabsObserver> tabs_observer_{this};
  base::WeakPtrFactory<RootTabCollectionNode> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_ROOT_TAB_COLLECTION_NODE_H_
