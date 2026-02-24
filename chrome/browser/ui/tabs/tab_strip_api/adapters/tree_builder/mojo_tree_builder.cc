// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tree_builder/mojo_tree_builder.h"

#include "base/types/pass_key.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tree_builder/walker_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_api/converters/tab_converters.h"

namespace tabs_api {

MojoTreeBuilder::MojoTreeBuilder(const TabStripModel* model,
                                 std::string window_id)
    : model_(model), window_id_(std::move(window_id)) {}

mojom::ContainerPtr MojoTreeBuilder::Build(
    tabs::TabCollection::Handle collection_root) const {
  auto factory = WalkerFactory(model_, base::PassKey<MojoTreeBuilder>());

  auto window_container = mojom::Container::New();
  auto window_data = mojom::Window::New();

  window_data->id = NodeId::FromWindowId(window_id_);
  window_container->data = mojom::Data::NewWindow(std::move(window_data));

  window_container->children.emplace_back(
      factory.WalkerForCollection(collection_root.Get()).Walk());

  return window_container;
}

}  // namespace tabs_api
