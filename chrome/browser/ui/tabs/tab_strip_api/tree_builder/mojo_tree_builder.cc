// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tree_builder/mojo_tree_builder.h"

#include <variant>

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/converters/tab_converters.h"

namespace tabs_api {

MojoTreeBuilder::MojoTreeBuilder(
    base::PassKey<TabStripServiceImpl> passkey,
    tabs_api::TabStripModelAdapter* tab_strip_model_adapter)
    : passkey_(passkey), tab_strip_model_adapter_(tab_strip_model_adapter) {}

tabs_api::mojom::TabCollectionContainerPtr MojoTreeBuilder::BuildTree(
    const tabs::TabCollection* root) {
  if (!root) {
    return nullptr;
  }
  auto it = root->collection_begin(passkey_);
  auto end_it = root->collection_end(passkey_);

  return BuildRecursive(it, end_it);
}

tabs_api::mojom::TabCollectionContainerPtr MojoTreeBuilder::BuildRecursive(
    tabs::TabCollection::Iterator& it,
    tabs::TabCollection::Iterator& end_it) {
  if (it == end_it) {
    return nullptr;
  }
  const auto& root_variant = *it;
  if (!std::holds_alternative<const tabs::TabCollection*>(root_variant)) {
    return nullptr;
  }

  auto* tab_collection = std::get<const tabs::TabCollection*>(root_variant);

  // First create the root TabCollectionContainer object.
  tabs_api::mojom::TabCollectionContainerPtr result_container =
      BuildTabCollectionContainer(tab_collection);

  // Then create the child elements if any
  for (size_t i = 0; i < tab_collection->ChildCount(); i++) {
    // The tab collection iterator is a pre-order traversal over the entire tree
    // Increment only if there are child elements.
    ++it;
    const auto& current_child = *it;
    tabs_api::mojom::ContainerPtr container_wrapper;
    if (std::holds_alternative<const tabs::TabInterface*>(current_child)) {
      auto mojo_tab_container =
          BuildTab(std::get<const tabs::TabInterface*>(current_child));
      container_wrapper = tabs_api::mojom::Container::NewTabContainer(
          std::move(mojo_tab_container));
    } else if (std::holds_alternative<const tabs::TabCollection*>(
                   current_child)) {
      auto mojo_tab_collection_container = BuildRecursive(it, end_it);
      container_wrapper = tabs_api::mojom::Container::NewTabCollectionContainer(
          std::move(mojo_tab_collection_container));
    }
    // It is possible that there is no child in the tab collection tree. (i.e.
    // Pinned can exist in the tree but may have no tabs).
    if (container_wrapper) {
      result_container->elements.push_back(std::move(container_wrapper));
    }
  }
  return result_container;
}

tabs_api::mojom::TabContainerPtr MojoTreeBuilder::BuildTab(
    const tabs::TabInterface* tab_interface) {
  auto mojo_tab_container = tabs_api::mojom::TabContainer::New();
  auto tab_index =
      tab_strip_model_adapter_->GetIndexForHandle(tab_interface->GetHandle());
  if (tab_index.has_value()) {
    auto mojo_tab = tabs_api::converters::BuildMojoTab(
        tab_interface->GetHandle(),
        tab_strip_model_adapter_->GetTabRendererData(tab_index.value()));
    mojo_tab_container->tab = std::move(mojo_tab);
  }
  return mojo_tab_container;
}

tabs_api::mojom::TabCollectionContainerPtr
MojoTreeBuilder::BuildTabCollectionContainer(
    const tabs::TabCollection* tab_collection) {
  auto mojo_tab_collection = tabs_api::converters::BuildMojoTabCollection(
      tab_collection->GetHandle(), tab_collection->type());
  auto mojo_tab_collection_container =
      tabs_api::mojom::TabCollectionContainer::New();
  mojo_tab_collection_container->collection = std::move(mojo_tab_collection);
  return mojo_tab_collection_container;
}

}  // namespace tabs_api
