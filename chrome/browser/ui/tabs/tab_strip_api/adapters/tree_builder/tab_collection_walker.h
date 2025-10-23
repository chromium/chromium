// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_TREE_BUILDER_TAB_COLLECTION_WALKER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_TREE_BUILDER_TAB_COLLECTION_WALKER_H_

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/tabs/public/tab_collection.h"

namespace tabs_api {

class MojoTreeBuilder;
class WalkerFactory;

class TabCollectionWalker {
 public:
  explicit TabCollectionWalker(const WalkerFactory* factory,
                               base::PassKey<MojoTreeBuilder> pass_key,
                               const tabs::TabCollection* collection);

  mojom::ContainerPtr Walk() const;

 private:
  raw_ptr<const WalkerFactory> factory_;
  base::PassKey<MojoTreeBuilder> pass_key_;
  raw_ptr<const tabs::TabCollection> target_;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_TREE_BUILDER_TAB_COLLECTION_WALKER_H_
