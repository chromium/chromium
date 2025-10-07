// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_TREE_BUILDER_WALKER_FACTORY_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_TREE_BUILDER_WALKER_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tree_builder/tab_collection_walker.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tree_builder/tab_walker.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

namespace tabs_api {

class MojoTreeBuilder;

class WalkerFactory {
 public:
  WalkerFactory(const TabStripModel* model,
                base::PassKey<MojoTreeBuilder> pass_key);

  TabWalker WalkerForTab(const tabs::TabInterface* tab) const;
  TabCollectionWalker WalkerForCollection(
      const tabs::TabCollection* collection) const;

 private:
  raw_ptr<const TabStripModel> model_;
  base::PassKey<MojoTreeBuilder> pass_key_;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_TREE_BUILDER_WALKER_FACTORY_H_
