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

TabCollectionNode::ViewFactory& GetViewFactory() {
  static base::NoDestructor<TabCollectionNode::ViewFactory> factory;
  return *factory;
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

// static
void TabCollectionNode::SetViewFactoryForTesting(ViewFactory factory) {
  GetViewFactory() = std::move(factory);
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
  if (GetViewFactory()) {
    return GetViewFactory().Run(node_for_view);
  }
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

// TODO(crbug.com/450976282): Consider having a map at the root level.
TabCollectionNode* TabCollectionNode::GetNodeForHandle(
    const tabs::TabCollectionNodeHandle& handle) {
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
  AddChildNodeView(std::move(child_node_view));
}

std::pair<std::unique_ptr<views::View>, std::unique_ptr<TabCollectionNode>>
TabCollectionNode::RemoveChild(base::PassKey<TabCollectionNode> pass_key,
                               const tabs::TabCollectionNodeHandle& handle) {
  std::pair<std::unique_ptr<views::View>, std::unique_ptr<TabCollectionNode>>
      removed_view_and_node;

  for (auto it = children_.begin(); it != children_.end(); ++it) {
    TabCollectionNode* child_node = it->get();

    if (child_node->GetHandle() != handle) {
      continue;
    }

    if (remove_child_from_node_) {
      removed_view_and_node.first =
          remove_child_from_node_.Run(child_node->node_view_);
    } else {
      removed_view_and_node.first =
          node_view_->RemoveChildViewT(child_node->node_view_);
    }
    removed_view_and_node.second = std::move(*it);
    children_.erase(it);
    return removed_view_and_node;
  }

  // The node to remove should be a direct child of this.
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

void TabCollectionNode::AddChild(std::unique_ptr<views::View> child_node_view,
                                 std::unique_ptr<TabCollectionNode> child_node,
                                 size_t model_index) {
  AddChildNode(std::move(child_node), model_index);

  // Add child view after inserting the child node into children_, as adding the
  // view may depend on the order of the node in children_.
  AddChildNodeView(std::move(child_node_view));
}

void TabCollectionNode::AddChildNode(
    std::unique_ptr<TabCollectionNode> child_node,
    size_t model_index) {
  child_node->SetController(tab_strip_controller_);
  children_.insert(children_.begin() + model_index, std::move(child_node));
}

void TabCollectionNode::AddChildNodeView(
    std::unique_ptr<views::View> child_node_view) {
  if (add_child_to_node_) {
    add_child_to_node_.Run(std::move(child_node_view));
  } else {
    node_view_->AddChildView(std::move(child_node_view));
  }
}
