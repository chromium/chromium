// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_collection_node.h"

#include <memory>
#include <variant>

#include "base/check.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_collection_node_interface.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

TabCollectionNode::TabCollectionNode(
    std::unique_ptr<TabInterface> tab_interface)
    : holder_(std::move(tab_interface)) {}

TabCollectionNode::TabCollectionNode(
    std::unique_ptr<TabCollection> tab_collection)
    : holder_(std::move(tab_collection)) {}

TabCollectionNode::~TabCollectionNode() = default;

TabCollectionNode::Type TabCollectionNode::GetType() const {
  return std::holds_alternative<std::unique_ptr<TabInterface>>(holder_)
             ? TabCollectionNode::Type::kTabInterface
             : TabCollectionNode::Type::kTabCollection;
}

TabInterface* TabCollectionNode::GetTabInterface() const {
  CHECK_EQ(GetType(), Type::kTabInterface);
  return std::get<std::unique_ptr<TabInterface>>(holder_).get();
}

TabCollection* TabCollectionNode::GetTabCollection() const {
  CHECK_EQ(GetType(), Type::kTabCollection);
  return std::get<std::unique_ptr<TabCollection>>(holder_).get();
}

}  // namespace tabs
