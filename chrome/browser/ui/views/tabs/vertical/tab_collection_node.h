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
#include "ui/gfx/geometry/rect.h"

namespace views {
class View;
}

class VerticalTabStripController;

// The individual collection node of the view model. The root
// node also inherits from this class. It provides extensibility hooks for the
// views to use to implement things like animations.
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
  typedef base::RepeatingCallback<void(views::View* view_to_remove)>
      CustomRemoveChildViewCallback;
  typedef base::RepeatingCallback<void(std::unique_ptr<views::View>,
                                       const gfx::Rect&)>
      CustomAttachChildViewCallback;
  typedef base::RepeatingCallback<std::unique_ptr<views::View>(views::View*)>
      CustomDetachChildViewCallback;
  typedef std::vector<std::unique_ptr<TabCollectionNode>> NodeChildren;

  explicit TabCollectionNode(tabs::ConstChildPtr node_data);
  virtual ~TabCollectionNode();

  // Creates the view for this node. Then, for each child container in children,
  // creates a TabCollectionNode, calls Initialize on it, and then adds the node
  // as a child of this.
  std::unique_ptr<views::View> Initialize();

  // Deinitializes all the child nodes in a recursive manner.
  void Deinitialize();

  // Gets the collection under this subtree that has the associated handle.
  // Returns nullptr if no such node exists.
  TabCollectionNode* GetNodeForHandle(
      const tabs::TabCollectionNodeHandle& handle);
  const TabCollectionNode* GetNodeForHandle(
      const tabs::TabCollectionNodeHandle& handle) const;
  TabCollectionNode* GetParentNodeForHandle(
      const tabs::TabCollectionNodeHandle& handle);
  const TabCollectionNode* GetParentNodeForHandle(
      const tabs::TabCollectionNodeHandle& handle) const;

  // Gets the first direct child of this node that has the associated type.
  // Returns nullptr if no such node exists.
  TabCollectionNode* GetChildNodeOfType(const Type type);

  // Creates a new child and adds it at model_index. If |perform_initialization|
  // is true, then the entire subtree of the node data will be constructed as
  // well, if not, then only the view for the child is constructed and added.
  void AddNewChild(base::PassKey<TabCollectionNode> pass_key,
                   tabs::ConstChildPtr node_data,
                   size_t model_index,
                   bool perform_initialization);

  // Removes the child and removes and destroys the view.
  void RemoveChild(base::PassKey<TabCollectionNode> pass_key,
                   const tabs::TabCollectionNodeHandle& handle,
                   bool perform_deinitialization);

  // Moves the node to the new index within the same parent. Also updates the
  // z-order of the moved child to the highest to ensure it shows over other
  // tabs when animating.
  void MoveChild(base::PassKey<TabCollectionNode> pass_key,
                 const tabs::TabCollectionNodeHandle& handle,
                 int new_index);

  // Moves the node to the new index across different parent nodes.
  static void MoveChild(base::PassKey<TabCollectionNode> pass_key,
                        const tabs::TabCollectionNodeHandle& handle,
                        int new_index,
                        TabCollectionNode* src_parent_node,
                        TabCollectionNode* dst_parent_node);

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

  void set_attach_child_to_node(
      CustomAttachChildViewCallback attach_child_to_node) {
    attach_child_to_node_ = std::move(attach_child_to_node);
  }

  void set_detach_child_from_node(
      CustomDetachChildViewCallback detach_child_from_node) {
    detach_child_from_node_ = std::move(detach_child_from_node);
  }

  base::CallbackListSubscription RegisterWillDestroyCallback(
      base::OnceClosure callback);

  base::CallbackListSubscription RegisterDataChangedCallback(
      base::RepeatingClosure callback);

  void NotifyDataChanged();

  void SetController(VerticalTabStripController* controller);
  VerticalTabStripController* GetController() { return tab_strip_controller_; }
  const VerticalTabStripController* GetController() const {
    return tab_strip_controller_;
  }

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

  // Creates node_view_, then returns the unique_ptr to the view.
  std::unique_ptr<views::View> CreateAndSetView();

  // Updates the focus order of the child view at the specified `child_index`.
  // This ensures that follows the logical order of the tab collection by
  // re-linking the view's focus neighbors based on its current position in the
  // children list.
  void EnsureFocusOrder(size_t child_index);

  base::OnceClosureList on_will_destroy_callback_list_;
  base::RepeatingClosureList on_data_changed_callback_list_;

  const Type type_;
  const tabs::TabCollectionNodeHandle handle_;
  tabs::ConstChildPtr node_data_;

  // 1:1 mapping of the collections children.
  NodeChildren children_;

  // add_child_to_node_ must be assigned when constructing the node_view in
  // Initialize so that the children that are created know how to be added to
  // the View Hierarchy. Used when the default AddChildView behavior needs to be
  // overridden.
  CustomAddChildViewCallback add_child_to_node_;

  // Custom callback to remove a child view, used when the default
  // RemoveChildViewT behavior needs to be overridden if the view hierarchy does
  // not match the view model hierarchy.
  CustomRemoveChildViewCallback remove_child_from_node_;

  // Custom callback invoked when reparent an existing view as child to another
  // container. Used when the default AddChildView behavior needs to be
  // overridden.
  CustomAttachChildViewCallback attach_child_to_node_;

  // Custom callback invoked when reparent an existing view as child to another
  // container. Used when the default RemoveChildViewT behavior needs to be
  // overridden.
  CustomDetachChildViewCallback detach_child_from_node_;

  // The view created for this node. (for tab:tabview, for unpinned: the
  // unpinned_container_view).
  raw_ptr<views::View> node_view_ = nullptr;

  // Allows views to create the Tab Context Menu.
  raw_ptr<VerticalTabStripController> tab_strip_controller_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_TAB_COLLECTION_NODE_H_
