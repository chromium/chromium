// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api_data_model.mojom.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service.h"

RootTabCollectionNode::RootTabCollectionNode(
    tabs_api::TabStripService* tab_strip_service,
    views::View* parent_view,
    CustomAddChildView add_node_to_parent_callback)
    : TabCollectionNode(std::move(add_node_to_parent_callback)) {
  CHECK(tab_strip_service);

  auto result = tab_strip_service->GetTabs();
  CHECK(result.has_value());
  Initialize(std::move(result.value()), parent_view, add_node_to_parent_);
  service_observer_.Observe(tab_strip_service);
}

RootTabCollectionNode::~RootTabCollectionNode() = default;

void RootTabCollectionNode::OnTabsCreated(
    const tabs_api::mojom::OnTabsCreatedEventPtr& tabs_created_event) {
  for (const auto& tab_created : tabs_created_event->tabs) {
    TabCollectionNode* parent =
        GetNodeForId(tab_created->position.parent_id().value());
    parent->AddNewChild(
        tabs_api::mojom::Data::NewTab(std::move(tab_created->tab)),
        tab_created->position.index());
  }
}

void RootTabCollectionNode::OnTabsClosed(
    const tabs_api::mojom::OnTabsClosedEventPtr& tabs_closed_event) {}

void RootTabCollectionNode::OnNodeMoved(
    const tabs_api::mojom::OnNodeMovedEventPtr& node_moved_event) {}

void RootTabCollectionNode::OnDataChanged(
    const tabs_api::mojom::OnDataChangedEventPtr& data_changed_event) {}

void RootTabCollectionNode::OnCollectionCreated(
    const tabs_api::mojom::OnCollectionCreatedEventPtr&
        collection_created_event) {}
