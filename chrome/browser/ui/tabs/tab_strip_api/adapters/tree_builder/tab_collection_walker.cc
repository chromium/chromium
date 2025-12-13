// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tree_builder/tab_collection_walker.h"

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tree_builder/walker_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_api/converters/tab_converters.h"

namespace tabs_api {

TabCollectionWalker::TabCollectionWalker(
    const WalkerFactory* factory,
    base::PassKey<MojoTreeBuilder> pass_key,
    const tabs::TabCollection* collection)
    : factory_(factory), pass_key_(pass_key), target_(collection) {}

mojom::ContainerPtr TabCollectionWalker::Walk() const {
  auto node = tabs_api::mojom::Container::New();
  node->data =
      tabs_api::converters::BuildMojoTabCollectionData(target_->GetHandle());

  for (const auto& child : target_->GetChildren(pass_key_)) {
    if (std::holds_alternative<std::unique_ptr<tabs::TabInterface>>(child)) {
      auto* tab = std::get<std::unique_ptr<tabs::TabInterface>>(child).get();
      auto result = factory_->WalkerForTab(tab).Walk();
      node->children.push_back(std::move(result));
    } else if (std::holds_alternative<std::unique_ptr<tabs::TabCollection>>(
                   child)) {
      auto* collection =
          std::get<std::unique_ptr<tabs::TabCollection>>(child).get();
      auto result = factory_->WalkerForCollection(collection).Walk();
      node->children.push_back(std::move(result));
    } else {
      NOTREACHED() << "unknown node type";
    }
  }

  return node;
}

}  // namespace tabs_api
