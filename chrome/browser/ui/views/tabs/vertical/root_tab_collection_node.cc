// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service.h"
#include "chrome/browser/ui/tabs/tab_strip_api/utilities/tab_strip_api_utilities.h"
#include "components/browser_apis/tab_strip/tab_strip_api_data_model.mojom.h"

RootTabCollectionNode::RootTabCollectionNode(
    tabs_api::TabStripService* tab_strip_service,
    CustomAddChildViewCallback add_node_view_to_parent)
    : RootTabCollectionNode(tab_strip_service,
                            tab_strip_service->GetTabs().value(),
                            add_node_view_to_parent) {}

RootTabCollectionNode::RootTabCollectionNode(
    tabs_api::TabStripService* tab_strip_service,
    tabs_api::mojom::ContainerPtr container,
    CustomAddChildViewCallback add_node_view_to_parent)
    : TabCollectionNode(std::move(container->data)) {
  add_node_view_to_parent.Run(Initialize(std::move(container->children)));
  service_observer_.Observe(tab_strip_service);
}

RootTabCollectionNode::~RootTabCollectionNode() = default;

void RootTabCollectionNode::OnTabsCreated(
    const tabs_api::mojom::OnTabsCreatedEventPtr& tabs_created_event) {
  for (const auto& tab_created : tabs_created_event->tabs) {
    TabCollectionNode* parent =
        GetNodeForId(tab_created->position.parent_id().value());

    tabs_api::mojom::ContainerPtr container = tabs_api::mojom::Container::New();
    container->data =
        tabs_api::mojom::Data::NewTab(std::move(tab_created->tab));

    parent->AddNewChild(GetPassKey(), std::move(container),
                        tab_created->position.index());
  }
}

void RootTabCollectionNode::OnTabsClosed(
    const tabs_api::mojom::OnTabsClosedEventPtr& tabs_closed_event) {
  for (auto& node_id : tabs_closed_event->tabs) {
    TabCollectionNode* parent_node = GetParentNodeForId(node_id);
    auto removed_view_and_node =
        parent_node->RemoveChild(GetPassKey(), node_id);
  }
}

void RootTabCollectionNode::OnNodeMoved(
    const tabs_api::mojom::OnNodeMovedEventPtr& node_moved_event) {
  auto node_id = node_moved_event->id;
  auto from_position = node_moved_event->from;
  auto to_position = node_moved_event->to;

  TabCollectionNode* src_parent_node =
      GetNodeForId(from_position.parent_id().value());
  TabCollectionNode* dst_parent_node =
      GetNodeForId(to_position.parent_id().value());

  auto [view, node] = src_parent_node->RemoveChild(GetPassKey(), node_id);
  dst_parent_node->AddChild(std::move(view), std::move(node),
                            to_position.index());
}

void RootTabCollectionNode::OnDataChanged(
    const tabs_api::mojom::OnDataChangedEventPtr& data_changed_event) {
  TabCollectionNode* node =
      GetNodeForId(tabs_api::utils::GetNodeId(*data_changed_event->data));
  if (node) {
    node->SetData(GetPassKey(), std::move(data_changed_event->data));
  }
}

void RootTabCollectionNode::OnCollectionCreated(
    const tabs_api::mojom::OnCollectionCreatedEventPtr&
        collection_created_event) {
  tabs_api::mojom::ContainerPtr container =
      std::move(collection_created_event->collection);
  TabCollectionNode* parent =
      GetNodeForId(collection_created_event->position.parent_id().value());

  parent->AddNewChild(GetPassKey(), std::move(container),
                      collection_created_event->position.index());
}
