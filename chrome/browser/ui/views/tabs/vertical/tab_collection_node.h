// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_TAB_COLLECTION_NODE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_TAB_COLLECTION_NODE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_collection_types.h"

namespace views {
class View;
}

class VerticalTabStripController;

class TabCollectionNode {
 public:
  // Keep the same as TabCollection::Type with a tab.
  // LINT.IfChange(TYPE)
  enum class Type { TABSTRIP, PINNED, UNPINNED, GROUP, SPLIT, TAB };
  // LINT.ThenChange(components/tabs/public/tab_collection.h:TYPE)

  // Helper type for creating CustomAddChildViewCallbacks with
  // base::BindRepeating.
  typedef views::View* (views::View::*CustomAddChildView)(
      std::unique_ptr<views::View>);
  typedef base::RepeatingCallback<views::View*(std::unique_ptr<views::View>)>
      CustomAddChildViewCallback;
  typedef base::RepeatingCallback<std::unique_ptr<views::View>(
      views::View* view_to_remove)>
      CustomRemoveChildViewCallback;
  typedef std::vector<std::unique_ptr<TabCollectionNode>> NodeChildren;

  using ViewFactory =
      base::RepeatingCallback<std::unique_ptr<views::View>(TabCollectionNode*)>;

  explicit TabCollectionNode(tabs::ConstChildPtr node_data);
  virtual ~TabCollectionNode();

  // Creates the view for this node. Then, for each child container in children,
  // creates a TabCollectionNode, calls Initialize on it, and then adds the node
  // as a child of this.
  std::unique_ptr<views::View> Initialize();

  // Gets the collection under this subtree that has the associated handle.
  // Returns nullptr if no such node exists.
  TabCollectionNode* GetNodeForHandle(
      const tabs::TabCollectionNodeHandle& handle);
  TabCollectionNode* GetParentNodeForHandle(
      const tabs::TabCollectionNodeHandle& handle);

  // Creates a new child and adds it at model_index. If |perform_initialization|
  // is true, then the entire subtree of the node data will be constructed as
  // well, if not, then only the view for the child is constructed and added.
  void AddNewChild(base::PassKey<TabCollectionNode> pass_key,
                   tabs::ConstChildPtr node_data,
                   size_t model_index,
                   bool perform_initialization);

  // Removes the node and the view associated with it. In the case of move, its
  // ownership is transferred to the destination node. In the case of remove it
  // is freed.
  std::pair<std::unique_ptr<views::View>, std::unique_ptr<TabCollectionNode>>
  RemoveChild(base::PassKey<TabCollectionNode> pass_key,
              const tabs::TabCollectionNodeHandle& handle);

  // Moves the node to the new index within the same parent. Also updates the
  // z-order of the moved child to the highest to ensure it shows over other
  // tabs when animating.
  void MoveChild(base::PassKey<TabCollectionNode> pass_key,
                 const tabs::TabCollectionNodeHandle& handle,
                 int new_index);

  // Adds child_node_view to node_view_ and child_node to children_.
  void AddChild(std::unique_ptr<views::View> child_node_view,
                std::unique_ptr<TabCollectionNode> child_node,
                size_t model_index);

  tabs::ConstChildPtr GetNodeData() const { return node_data_; }
  tabs::TabCollectionNodeHandle GetHandle() const;
  Type type() const { return type_; }
  const NodeChildren& children() const { return children_; }
  views::View* view() const { return node_view_; }
  std::vector<views::View*> GetDirectChildren() const;

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

  void NotifyDataChanged();

  static void SetViewFactoryForTesting(ViewFactory factory);
  views::View* get_view_for_testing() { return node_view_; }
  void SetController(VerticalTabStripController* controller);
  VerticalTabStripController* GetController() { return tab_strip_controller_; }

 protected:
  // Returns the pass key to be used by derived classes so that methods such as
  // SetData may only be performed by a `TabCollectionNode`.
  base::PassKey<TabCollectionNode> GetPassKey() const {
    return base::PassKey<TabCollectionNode>();
  }

  static std::unique_ptr<views::View> CreateViewForNode(
      TabCollectionNode* node_for_view);

  // Adds `child_node` to `children_`.
  void AddChildNode(std::unique_ptr<TabCollectionNode> child_node,
                    size_t model_index);
  // Adds `child_node_view` to `node_view_`.
  void AddChildNodeView(std::unique_ptr<views::View> child_node_view);

  // Creates node_view_, then returns the unique_ptr to the view.
  std::unique_ptr<views::View> CreateAndSetView();

  base::OnceClosureList on_will_destroy_callback_list_;
  base::RepeatingClosureList on_data_changed_callback_list_;

  const Type type_;
  const tabs::TabCollectionNodeHandle handle_;
  tabs::ConstChildPtr node_data_;

  // 1:1 mapping of the collections children.
  NodeChildren children_;

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

  // Allows views to create the Tab Context Menu.
  raw_ptr<VerticalTabStripController> tab_strip_controller_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_TAB_COLLECTION_NODE_H_
