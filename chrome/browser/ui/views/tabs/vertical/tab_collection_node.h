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
#include "base/types/pass_key.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/browser_apis/tab_strip/tab_strip_api_data_model.mojom.h"
#include "components/browser_apis/tab_strip/tab_strip_api_types.mojom.h"

namespace views {
class View;
}

// TODO(crbug.com/459824840): Animate views based on operation.
class TabCollectionNode {
 public:
  // Helper type for creating CustomAddChildViewCallbacks with
  // base::BindRepeating.
  typedef views::View* (views::View::*CustomAddChildView)(
      std::unique_ptr<views::View>);
  typedef base::RepeatingCallback<views::View*(std::unique_ptr<views::View>)>
      CustomAddChildViewCallback;
  typedef tabs_api::mojom::Data::Tag Type;
  typedef base::RepeatingCallback<std::unique_ptr<views::View>(
      views::View* view_to_remove)>
      CustomRemoveChildViewCallback;
  typedef std::vector<std::unique_ptr<TabCollectionNode>> Children;

  using ViewFactory =
      base::RepeatingCallback<std::unique_ptr<views::View>(TabCollectionNode*)>;

  explicit TabCollectionNode(tabs_api::mojom::DataPtr data);
  virtual ~TabCollectionNode();

  // Creates the view for this node. Then, for each child container in children,
  // creates a TabCollectionNode, calls Initialize on it, and then adds the node
  // as a child of this.
  // TODO May need a BrowserWindow Interface
  std::unique_ptr<views::View> Initialize(
      std::vector<tabs_api::mojom::ContainerPtr> child_containers);

  void SetData(base::PassKey<TabCollectionNode> pass_key,
               tabs_api::mojom::DataPtr data);

  // Gets the collection under this subtree that has the associated node_id.
  // Returns nullptr if no such node exists.
  TabCollectionNode* GetNodeForId(const tabs_api::NodeId& node_id);
  TabCollectionNode* GetParentNodeForId(const tabs_api::NodeId& node_id);

  // Creates a new child and adds it at model_index.
  void AddNewChild(base::PassKey<TabCollectionNode> pass_key,
                   tabs_api::mojom::ContainerPtr container,
                   size_t model_index);

  // Removes the node and the view associated with it. In the case of move, its
  // ownership is transferred to the destination node. In the case of remove it
  // is freed.
  std::pair<std::unique_ptr<views::View>, std::unique_ptr<TabCollectionNode>>
  RemoveChild(base::PassKey<TabCollectionNode> pass_key,
              const tabs_api::NodeId& node_id);

  // Adds child_node_view to node_view_ and child_node to children_.
  void AddChild(std::unique_ptr<views::View> child_node_view,
                std::unique_ptr<TabCollectionNode> child_node,
                size_t model_index);

  const tabs_api::mojom::DataPtr& data() const { return data_; }
  const Children& children() const { return children_; }
  std::vector<views::View*> GetDirectChildren() const;

  Type GetType() const { return data_->which(); }

  void set_add_child_to_node(CustomAddChildViewCallback add_child_to_node) {
    add_child_to_node_ = std::move(add_child_to_node);
  }

  void set_remove_child_from_node(
      CustomRemoveChildViewCallback remove_child_from_node) {
    remove_child_from_node_ = std::move(remove_child_from_node);
  }

  base::CallbackListSubscription RegisterWillDestroyCallback(
      base::OnceClosure callback);

  base::CallbackListSubscription RegisterDataChangedCallback(
      base::RepeatingClosure callback);

  static void SetViewFactoryForTesting(ViewFactory factory);
  views::View* get_view_for_testing() { return node_view_; }

 protected:
  // Returns the pass key to be used by derived classes so that methods such as
  // SetData may only be performed by a `TabCollectionNode`.
  base::PassKey<TabCollectionNode> GetPassKey() const {
    return base::PassKey<TabCollectionNode>();
  }

  static std::unique_ptr<views::View> CreateViewForNode(
      TabCollectionNode* node_for_view);

  // Creates node_view_, then returns the unique_ptr to the view.
  std::unique_ptr<views::View> CreateAndSetView();

  base::OnceClosureList on_will_destroy_callback_list_;
  base::RepeatingClosureList on_data_changed_callback_list_;

  // the current collection_data object. provided by snapshot and updated
  // through TabObserver.
  tabs_api::mojom::DataPtr data_;

  // 1:1 mapping of the collections children.
  Children children_;

  // add_child_to_node_ must be assigned when constructing the node_view in
  // Initialize so that the children that are created know how to be added to
  // the View Hierarchy.
  // The type of add_child_to_node_ is
  // views::View*(std::unique_ptr<views::View>, size_t)
  // where the first argument is the child view to be added, the second argument
  // is the model index (this is so that we don't have to recalculate it during
  // add_child_to_node_, and if add_child_to_node_ doesn't care about the model
  // index, it can ignore this argument), and the return value is a raw pointer
  // to the child view that was added.
  CustomAddChildViewCallback add_child_to_node_;

  // Custom callback to remove a child view, used when the default
  // RemoveChildViewT behavior needs to be overridden if the view hierarchy does
  // not match the view model hierarchy.
  CustomRemoveChildViewCallback remove_child_from_node_;

  // The view created for this node. (for tab:tabview, for unpinned: the
  // unpinned_container_view).
  raw_ptr<views::View> node_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_TAB_COLLECTION_NODE_H_
