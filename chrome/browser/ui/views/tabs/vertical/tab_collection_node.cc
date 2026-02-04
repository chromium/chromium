// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_pinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_split_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_unpinned_tab_container_view.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_collection_types.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/views/view.h"

namespace {

TabCollectionNode::Type GetTypeFromNode(tabs::ConstChildPtr node_data_) {
  if (std::holds_alternative<const tabs::TabCollection*>(node_data_)) {
    switch (std::get<const tabs::TabCollection*>(node_data_)->type()) {
      case tabs::TabCollection::Type::TABSTRIP:
        return TabCollectionNode::Type::TABSTRIP;
      case tabs::TabCollection::Type::PINNED:
        return TabCollectionNode::Type::PINNED;
      case tabs::TabCollection::Type::UNPINNED:
        return TabCollectionNode::Type::UNPINNED;
      case tabs::TabCollection::Type::GROUP:
        return TabCollectionNode::Type::GROUP;
      case tabs::TabCollection::Type::SPLIT:
        return TabCollectionNode::Type::SPLIT;
    }
  }
  CHECK(std::holds_alternative<const tabs::TabInterface*>(node_data_));
  return TabCollectionNode::Type::TAB;
}

class CollectionTestViewImpl : public views::View {
 public:
  explicit CollectionTestViewImpl(TabCollectionNode* node) {
    node->set_add_child_to_node(
        base::BindRepeating<TabCollectionNode::CustomAddChildView>(
            &views::View::AddChildView, base::Unretained(this)));
  }
  ~CollectionTestViewImpl() override = default;
};

tabs::TabCollectionNodeHandle GetHandleFromNode(tabs::ConstChildPtr node_data) {
  if (std::holds_alternative<const tabs::TabCollection*>(node_data)) {
    const tabs::TabCollection* collection =
        std::get<const tabs::TabCollection*>(node_data);
    return collection->GetHandle();
  } else {
    CHECK(std::holds_alternative<const tabs::TabInterface*>(node_data));
    const tabs::TabInterface* tab =
        std::get<const tabs::TabInterface*>(node_data);
    return tab->GetHandle();
  }
}

}  // anonymous namespace

base::CallbackListSubscription TabCollectionNode::RegisterWillDestroyCallback(
    base::OnceClosure callback) {
  return on_will_destroy_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription TabCollectionNode::RegisterDataChangedCallback(
    base::RepeatingClosure callback) {
  return on_data_changed_callback_list_.Add(std::move(callback));
}

void TabCollectionNode::NotifyDataChanged() {
  on_data_changed_callback_list_.Notify();
}

void TabCollectionNode::SetController(VerticalTabStripController* controller) {
  tab_strip_controller_ = controller;
  for (const auto& child : children_) {
    child->SetController(controller);
  }
}

// static
std::unique_ptr<views::View> TabCollectionNode::CreateViewForNode(
    TabCollectionNode* node_for_view) {
  switch (node_for_view->type()) {
    case Type::TABSTRIP:
      return std::make_unique<VerticalTabStripView>(node_for_view);
    case Type::PINNED:
      return std::make_unique<VerticalPinnedTabContainerView>(node_for_view);
    case Type::UNPINNED:
      return std::make_unique<VerticalUnpinnedTabContainerView>(node_for_view);
    case Type::SPLIT:
      return std::make_unique<VerticalSplitTabView>(node_for_view);
    case Type::GROUP:
      return std::make_unique<VerticalTabGroupView>(node_for_view);
    case Type::TAB:
      return std::make_unique<VerticalTabView>(node_for_view);
  }
  return std::make_unique<CollectionTestViewImpl>(node_for_view);
}

TabCollectionNode::TabCollectionNode(tabs::ConstChildPtr node_data)
    : type_(GetTypeFromNode(node_data)),
      handle_(GetHandleFromNode(node_data)),
      node_data_(node_data) {}

TabCollectionNode::~TabCollectionNode() {
  on_will_destroy_callback_list_.Notify();
}

tabs::TabCollectionNodeHandle TabCollectionNode::GetHandle() const {
  return handle_;
}

std::unique_ptr<views::View> TabCollectionNode::Initialize() {
  std::unique_ptr<views::View> node_view = CreateAndSetView();

  if (std::holds_alternative<const tabs::TabCollection*>(node_data_)) {
    const tabs::TabCollection* collection =
        std::get<const tabs::TabCollection*>(node_data_);
    for (const auto& child_data : collection->GetChildren()) {
      tabs::ConstChildPtr child_ptr;
      if (std::holds_alternative<std::unique_ptr<tabs::TabCollection>>(
              child_data)) {
        child_ptr =
            std::get<std::unique_ptr<tabs::TabCollection>>(child_data).get();
      } else {
        CHECK(std::holds_alternative<std::unique_ptr<tabs::TabInterface>>(
            child_data));
        child_ptr =
            std::get<std::unique_ptr<tabs::TabInterface>>(child_data).get();
      }
      AddNewChild(GetPassKey(), child_ptr, children_.size(),
                  /*perform_initialization=*/true);
    }
  } else {
    CHECK(std::holds_alternative<const tabs::TabInterface*>(node_data_));
  }

  return node_view;
}

void TabCollectionNode::Deinitialize() {
  if (std::holds_alternative<const tabs::TabCollection*>(node_data_)) {
    const tabs::TabCollection* collection =
        std::get<const tabs::TabCollection*>(node_data_);
    for (const auto& child_data : collection->GetChildren()) {
      tabs::TabCollectionNodeHandle child_handle;
      if (std::holds_alternative<std::unique_ptr<tabs::TabCollection>>(
              child_data)) {
        child_handle =
            std::get<std::unique_ptr<tabs::TabCollection>>(child_data)
                ->GetHandle();
      } else {
        CHECK(std::holds_alternative<std::unique_ptr<tabs::TabInterface>>(
            child_data));
        child_handle = std::get<std::unique_ptr<tabs::TabInterface>>(child_data)
                           ->GetHandle();
      }
      RemoveChild(GetPassKey(), child_handle,
                  /*perform_deinitialization=*/true);
    }
  } else {
    CHECK(std::holds_alternative<const tabs::TabInterface*>(node_data_));
  }
}

TabCollectionNode* TabCollectionNode::GetNodeForHandle(
    const tabs::TabCollectionNodeHandle& handle) {
  return const_cast<TabCollectionNode*>(
      std::as_const(*this).GetNodeForHandle(handle));
}

// TODO(crbug.com/450976282): Consider having a map at the root level.
const TabCollectionNode* TabCollectionNode::GetNodeForHandle(
    const tabs::TabCollectionNodeHandle& handle) const {
  if (GetHandle() == handle) {
    return this;
  }

  for (const auto& child : children_) {
    if (TabCollectionNode* node = child->GetNodeForHandle(handle)) {
      return node;
    }
  }

  return nullptr;
}

TabCollectionNode* TabCollectionNode::GetParentNodeForHandle(
    const tabs::TabCollectionNodeHandle& handle) {
  return const_cast<TabCollectionNode*>(
      std::as_const(*this).GetParentNodeForHandle(handle));
}

const TabCollectionNode* TabCollectionNode::GetParentNodeForHandle(
    const tabs::TabCollectionNodeHandle& handle) const {
  for (auto& child_node : children_) {
    if (child_node->GetHandle() == handle) {
      return this;
    }
  }

  for (auto& child_node : children_) {
    TabCollectionNode* parent = child_node->GetParentNodeForHandle(handle);
    if (parent) {
      return parent;
    }
  }

  return nullptr;
}

TabCollectionNode* TabCollectionNode::GetChildNodeOfType(const Type type) {
  if (type_ == type) {
    return this;
  }

  const auto it = std::ranges::find_if(
      children_, [type](const auto& child) { return child->type() == type; });
  return it != children_.end() ? it->get() : nullptr;
}

void TabCollectionNode::AddNewChild(base::PassKey<TabCollectionNode> pass_key,
                                    tabs::ConstChildPtr node_data,
                                    size_t model_index,
                                    bool perform_initialization) {
  auto child_node = std::make_unique<TabCollectionNode>(node_data);
  auto* child_node_ptr = child_node.get();
  AddChildNode(std::move(child_node), model_index);

  std::unique_ptr<views::View> child_node_view;
  if (perform_initialization) {
    child_node_view = child_node_ptr->Initialize();
  } else {
    child_node_view = child_node_ptr->CreateAndSetView();
  }

  if (add_child_to_node_) {
    add_child_to_node_.Run(std::move(child_node_view));
  } else {
    node_view_->AddChildView(std::move(child_node_view));
  }

  EnsureFocusOrder(model_index);
}

void TabCollectionNode::RemoveChild(base::PassKey<TabCollectionNode> pass_key,
                                    const tabs::TabCollectionNodeHandle& handle,
                                    bool perform_deinitialization) {
  for (auto it = children_.begin(); it != children_.end(); ++it) {
    TabCollectionNode* child_node = it->get();

    if (child_node->GetHandle() != handle) {
      continue;
    }

    if (perform_deinitialization) {
      child_node->Deinitialize();
    }

    views::View* node_to_remove = child_node->node_view_;
    children_.erase(it);
    if (remove_child_from_node_) {
      remove_child_from_node_.Run(node_to_remove);
    } else {
      node_view_->RemoveChildViewT(node_to_remove);
    }
    return;
  }

  // The node to remove should be a direct child of this.
  NOTREACHED();
}

void TabCollectionNode::MoveChild(base::PassKey<TabCollectionNode> pass_key,
                                  const tabs::TabCollectionNodeHandle& handle,
                                  int new_index) {
  auto it = std::find_if(
      children_.begin(), children_.end(),
      [&handle](const auto& node) { return node->GetHandle() == handle; });
  CHECK(it != children_.end());

  const size_t old_index = std::distance(children_.begin(), it);
  const size_t target_index = static_cast<size_t>(
      std::clamp(new_index, 0, static_cast<int>(children_.size() - 1)));

  if (old_index == target_index) {
    return;
  }

  if (old_index < target_index) {
    std::rotate(children_.begin() + old_index,
                children_.begin() + old_index + 1,
                children_.begin() + target_index + 1);
  } else {
    std::rotate(children_.begin() + target_index, children_.begin() + old_index,
                children_.begin() + old_index + 1);
  }

  // Move the child view to the top of the z-order to ensure the moved child
  // appears over the other tabs in its parent container.
  TabCollectionNode* moved_node = children_[target_index].get();
  node_view_->ReorderChildView(moved_node->node_view_,
                               static_cast<int>(children_.size() - 1));
  node_view_->InvalidateLayout();

  EnsureFocusOrder(target_index);
}

// static
void TabCollectionNode::MoveChild(base::PassKey<TabCollectionNode> pass_key,
                                  const tabs::TabCollectionNodeHandle& handle,
                                  int new_index,
                                  TabCollectionNode* src_parent_node,
                                  TabCollectionNode* dst_parent_node) {
  for (auto it = src_parent_node->children_.begin();
       it != src_parent_node->children_.end(); ++it) {
    TabCollectionNode* child_node = it->get();
    if (child_node->GetHandle() != handle) {
      continue;
    }

    const gfx::Rect previous_bounds_in_screen =
        child_node->node_view_->GetBoundsInScreen();

    std::unique_ptr<views::View> removed_view =
        src_parent_node->detach_child_from_node_
            ? src_parent_node->detach_child_from_node_.Run(
                  child_node->node_view_)
            : src_parent_node->node_view_->RemoveChildViewT(
                  child_node->node_view_);
    std::unique_ptr<TabCollectionNode> removed_node = std::move(*it);
    src_parent_node->children_.erase(it);

    dst_parent_node->AddChildNode(std::move(removed_node), new_index);
    if (dst_parent_node->attach_child_to_node_) {
      dst_parent_node->attach_child_to_node_.Run(std::move(removed_view),
                                                 previous_bounds_in_screen);
    } else {
      dst_parent_node->node_view_->AddChildView(std::move(removed_view));
    }
    dst_parent_node->EnsureFocusOrder(new_index);
    return;
  }

  // The node to remove should be a direct child of src_parent_node.
  NOTREACHED();
}

std::vector<views::View*> TabCollectionNode::GetDirectChildren() const {
  std::vector<views::View*> child_views;
  child_views.reserve(children_.size());
  for (const auto& child : children_) {
    child_views.push_back(child->node_view_);
  }
  return child_views;
}

std::unique_ptr<views::View> TabCollectionNode::CreateAndSetView() {
  auto node_view = CreateViewForNode(this);
  node_view_ = node_view.get();
  return node_view;
}

void TabCollectionNode::AddChildNode(
    std::unique_ptr<TabCollectionNode> child_node,
    size_t model_index) {
  child_node->SetController(tab_strip_controller_);
  children_.insert(children_.begin() + model_index, std::move(child_node));
}

void TabCollectionNode::EnsureFocusOrder(size_t child_index) {
  if (type() == Type::TABSTRIP) {
    return;
  }

  if (child_index > 0) {
    children_[child_index]->node_view_->InsertAfterInFocusList(
        children_[child_index - 1]->node_view_);
  } else if (children_.size() > 1) {
    children_[child_index]->node_view_->InsertBeforeInFocusList(
        children_[1]->node_view_);
  }
}
