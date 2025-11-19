// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "chrome/browser/ui/tabs/tab_strip_api/utilities/tab_strip_api_utilities.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_pinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_split_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_unpinned_tab_container_view.h"
#include "components/browser_apis/tab_strip/tab_strip_api_types.mojom.h"
#include "ui/views/view.h"

namespace {

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

}  // anonymous namespace

base::CallbackListSubscription TabCollectionNode::RegisterWillDestroyCallback(
    base::OnceClosure callback) {
  return on_will_destroy_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription TabCollectionNode::RegisterDataChangedCallback(
    base::RepeatingClosure callback) {
  return on_data_changed_callback_list_.Add(std::move(callback));
}

// static
void TabCollectionNode::SetViewFactoryForTesting(ViewFactory factory) {
  GetViewFactory() = std::move(factory);
}

// static
std::unique_ptr<views::View> TabCollectionNode::CreateViewForNode(
    TabCollectionNode* node_for_view) {
  if (GetViewFactory()) {
    return GetViewFactory().Run(node_for_view);
  }
  switch (node_for_view->GetType()) {
    case Type::kTabStrip:
      return std::make_unique<VerticalTabStripView>(node_for_view);
    case Type::kPinnedTabs:
      return std::make_unique<VerticalPinnedTabContainerView>(node_for_view);
    case Type::kUnpinnedTabs:
      return std::make_unique<VerticalUnpinnedTabContainerView>(node_for_view);
    case Type::kSplitTab:
      return std::make_unique<VerticalSplitTabView>(node_for_view);
    case Type::kTabGroup:
      return std::make_unique<VerticalTabGroupView>(node_for_view);
    case Type::kTab:
      return std::make_unique<VerticalTabView>(node_for_view);
  }
  return std::make_unique<CollectionTestViewImpl>(node_for_view);
}

TabCollectionNode::TabCollectionNode(tabs_api::mojom::DataPtr data)
    : data_(std::move(data)) {}

TabCollectionNode::~TabCollectionNode() {
  on_will_destroy_callback_list_.Notify();
}

std::unique_ptr<views::View> TabCollectionNode::Initialize(
    std::vector<tabs_api::mojom::ContainerPtr> child_containers) {
  CHECK(children_.empty());
  children_.reserve(child_containers.size());

  std::unique_ptr<views::View> node_view = CreateAndSetView();

  for (auto& child_container : child_containers) {
    auto child_node =
        std::make_unique<TabCollectionNode>(std::move(child_container->data));
    auto child_node_view =
        child_node->Initialize(std::move(child_container->children));
    AddChild(std::move(child_node_view), std::move(child_node),
             children_.size());
  }

  return node_view;
}

void TabCollectionNode::SetData(base::PassKey<TabCollectionNode> pass_key,
                                tabs_api::mojom::DataPtr data) {
  data_ = std::move(data);
  on_data_changed_callback_list_.Notify();
}

// TODO(crbug.com/450976282): Consider having a map at the root level, or using
// path in the API, in order to not have to iterate through the whole collection
// node structure.
TabCollectionNode* TabCollectionNode::GetNodeForId(
    const tabs_api::NodeId& node_id) {
  if (tabs_api::utils::GetNodeId(*data_) == node_id) {
    return this;
  }

  for (const auto& child : children_) {
    if (TabCollectionNode* node = child->GetNodeForId(node_id)) {
      return node;
    }
  }

  return nullptr;
}

TabCollectionNode* TabCollectionNode::GetParentNodeForId(
    const tabs_api::NodeId& node_id) {
  for (auto& child_node : children_) {
    if (tabs_api::utils::GetNodeId(*child_node->data()) == node_id) {
      return this;
    }
  }

  for (auto& child_node : children_) {
    TabCollectionNode* parent = child_node->GetParentNodeForId(node_id);
    if (parent) {
      return parent;
    }
  }

  return nullptr;
}

void TabCollectionNode::AddNewChild(base::PassKey<TabCollectionNode> pass_key,
                                    tabs_api::mojom::ContainerPtr container,
                                    size_t model_index) {
  auto child_node =
      std::make_unique<TabCollectionNode>(std::move(container->data));
  auto child_node_view = child_node->Initialize(std::move(container->children));
  AddChild(std::move(child_node_view), std::move(child_node), model_index);
}

std::pair<std::unique_ptr<views::View>, std::unique_ptr<TabCollectionNode>>
TabCollectionNode::RemoveChild(base::PassKey<TabCollectionNode> pass_key,
                               const tabs_api::NodeId& node_id) {
  std::pair<std::unique_ptr<views::View>, std::unique_ptr<TabCollectionNode>>
      removed_view_and_node;

  for (auto it = children_.begin(); it != children_.end(); ++it) {
    TabCollectionNode* child_node = it->get();

    if (tabs_api::utils::GetNodeId(*child_node->data()) == node_id) {
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
  children_.insert(children_.begin() + model_index, std::move(child_node));
  // Add child view after inserting the child node into children_, as adding the
  // view may depend on the order of the node in children_.
  if (add_child_to_node_) {
    add_child_to_node_.Run(std::move(child_node_view));
  } else {
    node_view_->AddChildView(std::move(child_node_view));
  }
}
