// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tree_builder/walker_factory.h"

namespace tabs_api {

WalkerFactory::WalkerFactory(const TabStripModel* model,
                             base::PassKey<MojoTreeBuilder> pass_key)
    : model_(model), pass_key_(pass_key) {}

TabWalker WalkerFactory::WalkerForTab(const tabs::TabInterface* tab) const {
  return TabWalker(model_, tab);
}

TabCollectionWalker WalkerFactory::WalkerForCollection(
    const tabs::TabCollection* collection) const {
  return TabCollectionWalker(this, pass_key_, collection);
}

}  // namespace tabs_api
