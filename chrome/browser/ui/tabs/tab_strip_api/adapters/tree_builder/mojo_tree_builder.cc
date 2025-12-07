// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tree_builder/mojo_tree_builder.h"

#include "base/types/pass_key.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tree_builder/walker_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_api/converters/tab_converters.h"

namespace tabs_api {

MojoTreeBuilder::MojoTreeBuilder(const TabStripModel* model) : model_(model) {}

mojom::ContainerPtr MojoTreeBuilder::Build(
    tabs::TabCollection::Handle root) const {
  auto factory = WalkerFactory(model_, base::PassKey<MojoTreeBuilder>());

  return factory.WalkerForCollection(root.Get()).Walk();
}

}  // namespace tabs_api
