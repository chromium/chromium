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
}

RootTabCollectionNode::~RootTabCollectionNode() = default;
