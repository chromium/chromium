// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api_types.mojom.h"
#include "chrome/browser/ui/tabs/tab_strip_api/utilities/tab_strip_api_utilities.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_pinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_split_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_unpinned_tab_container_view.h"
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
        base::BindRepeating(static_cast<views::View* (
                                views::View::*)(std::unique_ptr<views::View>)>(
                                &views::View::AddChildView),
                            base::Unretained(this)));
  }
  ~CollectionTestViewImpl() override = default;
};

}  // anonymous namespace

base::CallbackListSubscription TabCollectionNode::RegisterWillDestroyCallback(
    base::OnceClosure callback) {
  return on_will_destroy_callback_list_.Add(std::move(callback));
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
      // TODO(crbug.com/442567916): support tab groups.
      break;
    case Type::kTab:
      return std::make_unique<VerticalTabView>(node_for_view);
  }
  return std::make_unique<CollectionTestViewImpl>(node_for_view);
}

TabCollectionNode::TabCollectionNode() = default;
TabCollectionNode::TabCollectionNode(tabs_api::mojom::DataPtr data)
    : data_(std::move(data)) {}

// TODO(crbug.com/450304539): Get rid of this. Collections should only store
// add_child_to_node_ and use its parents add_child_to_node_ if need be, instead
// of storing a copy of it.
TabCollectionNode::TabCollectionNode(
    CustomAddChildView add_node_to_parent_callback)
    : add_node_to_parent_(std::move(add_node_to_parent_callback)) {}

TabCollectionNode::~TabCollectionNode() {
  on_will_destroy_callback_list_.Notify();
}

// TODO(crbug.com/450304539): change to take TabCollectionNode* parent_node,
// then use parent_node->node_view_ and parent_node->add_child_to_node_.
void TabCollectionNode::Initialize(
    tabs_api::mojom::ContainerPtr container,
    views::View* parent_view,
    CustomAddChildView add_node_to_parent_callback) {
  CHECK(children_.empty());
  children_.reserve(container->children.size());

  parent_view_ = parent_view;
  add_node_to_parent_ = std::move(add_node_to_parent_callback);
  data_ = std::move(container->data);

  std::unique_ptr<views::View> node_view = CreateViewForNode(this);
  // It is expected that the node_view_ constructor will add the method
  // add_child_to_node_ to the tab_collection_node_. If it does not,
  // AddChildView will be used for filling its children below.
  node_view_ = node_view.get();
  if (add_node_to_parent_) {
    add_node_to_parent_.Run(std::move(node_view));
  } else {
    parent_view->AddChildView(std::move(node_view));
  }

  for (auto& child_container : container->children) {
    auto child_node = std::make_unique<TabCollectionNode>();
    child_node->Initialize(std::move(child_container), node_view_,
                           add_child_to_node_);
    children_.push_back(std::move(child_node));
  }
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

void TabCollectionNode::AddNewChild(tabs_api::mojom::DataPtr data,
                                    size_t index) {
  auto child_node = std::make_unique<TabCollectionNode>(std::move(data));
  auto child_node_view = child_node->CreateAndSetView();
  AddChild(std::move(child_node_view), std::move(child_node), index);
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

// TODO(crbug.com/450304539): Actually use the index here, after refactoring
// add_parent_to_node_/add_child_to_node_.
void TabCollectionNode::AddChild(std::unique_ptr<views::View> child_node_view,
                                 std::unique_ptr<TabCollectionNode> child_node,
                                 size_t index) {
  if (add_child_to_node_) {
    add_child_to_node_.Run(std::move(child_node_view));
  } else {
    node_view_->AddChildView(std::move(child_node_view));
  }
  children_.push_back(std::move(child_node));
}
