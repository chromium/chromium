// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api_data_model.mojom.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_register.h"

RootTabCollectionNode::RootTabCollectionNode(
    TabStripServiceRegister* service_register,
    views::View* parent_view,
    CustomAddChildView add_node_to_parent_callback)
    : TabCollectionNode(std::move(add_node_to_parent_callback)) {
  CHECK(service_register);
  service_register->Accept(remote_.BindNewPipeAndPassReceiver());
  remote_->GetTabs(base::BindOnce(&RootTabCollectionNode::OnGetTabs,
                                  weak_ptr_factory_.GetWeakPtr(), parent_view));
}

RootTabCollectionNode::~RootTabCollectionNode() = default;

void RootTabCollectionNode::OnTabEvents(
    std::vector<tabs_api::mojom::TabsEventPtr> events) {}

void RootTabCollectionNode::OnGetTabs(
    views::View* parent_view,
    base::expected<tabs_api::mojom::TabsSnapshotPtr, mojo_base::mojom::ErrorPtr>
        result) {
  // Tab Strip Service can not be instantiated
  CHECK(result.has_value());

  tabs_api::mojom::TabsSnapshotPtr snapshot = std::move(result.value());
  Initialize(std::move(snapshot->tab_strip), parent_view, add_node_to_parent_);

  tabs_observer_.Bind(std::move(snapshot->stream));
}
