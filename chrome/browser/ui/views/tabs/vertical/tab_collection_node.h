// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_TAB_COLLECTION_NODE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_TAB_COLLECTION_NODE_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api_data_model.mojom.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api_types.mojom.h"

namespace views {
class View;
}

class TabCollectionNode {
 public:
  typedef base::RepeatingCallback<views::View*(std::unique_ptr<views::View>)>
      CustomAddChildView;
  typedef tabs_api::mojom::Data::Tag Type;
  typedef std::vector<std::unique_ptr<TabCollectionNode>> Children;

  using ViewFactory =
      base::RepeatingCallback<std::unique_ptr<views::View>(TabCollectionNode*)>;

  TabCollectionNode();
  explicit TabCollectionNode(tabs_api::mojom::DataPtr data);
  explicit TabCollectionNode(CustomAddChildView add_node_to_parent_callback);
  virtual ~TabCollectionNode();

  // A TabCollectionNode will be created for each of the children
  // 'Container' The container which holds children information and Data.
  // TODO May need a BrowserWindow Interface
  void Initialize(tabs_api::mojom::ContainerPtr container,
                  views::View* parent_view,
                  CustomAddChildView add_node_to_parent_callback);

  // Gets the collection under this subtree that has the associated node_id.
  // Returns nullptr if no such node exists.
  TabCollectionNode* GetNodeForId(const tabs_api::NodeId& node_id);

  // Creates a new child with data and adds it at index.
  void AddNewChild(tabs_api::mojom::DataPtr data, size_t index);

  const tabs_api::mojom::DataPtr& data() const { return data_; }
  const Children& children() const { return children_; }
  std::vector<views::View*> GetDirectChildren() const;

  Type GetType() const { return data_->which(); }

  void set_add_child_to_node(CustomAddChildView add_child_to_node) {
    add_child_to_node_ = std::move(add_child_to_node);
  }

  base::CallbackListSubscription RegisterWillDestroyCallback(
      base::OnceClosure callback);

  static void SetViewFactoryForTesting(ViewFactory factory);
  views::View* get_view_for_testing() { return node_view_; }

 protected:
  static std::unique_ptr<views::View> CreateViewForNode(
      TabCollectionNode* node_for_view);

  // Creates node_view_, then returns the unique_ptr to the view.
  std::unique_ptr<views::View> CreateAndSetView();

  // Adds child_node_view to node_view_ and child_node to children_.
  void AddChild(std::unique_ptr<views::View> child_node_view,
                std::unique_ptr<TabCollectionNode> child_node,
                size_t index);

  base::OnceClosureList on_will_destroy_callback_list_;

  // the current collection_data object. provided by snapshot and updated
  // through TabObserver.
  tabs_api::mojom::DataPtr data_;

  // 1:1 mapping of the collections children.
  Children children_;

  // parent view (for tab, unpinned_container, for unpinned, the
  // tab_strip_container_view) parent view function for adding child
  CustomAddChildView add_node_to_parent_;
  raw_ptr<views::View> parent_view_ = nullptr;

  // The view created for this node. (for tab:tabview, for unpinned: the
  // unpinned_container_view).
  // add_child_to_node_ must be assigned when constructing the node_view in
  // Initialize so that the children that are created know how to be added to
  // the View Hierarchy.
  CustomAddChildView add_child_to_node_;
  raw_ptr<views::View> node_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_TAB_COLLECTION_NODE_H_
