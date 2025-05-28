// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TREE_BUILDER_MOJO_TREE_BUILDER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TREE_BUILDER_MOJO_TREE_BUILDER_H_

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "components/tabs/public/tab_collection.h"

class TabStripServiceImpl;

namespace tabs_api {
class TabStripModelAdapter;
}  // namespace tabs_api

namespace tabs_api {

class MojoTreeBuilder {
 public:
  explicit MojoTreeBuilder(
      base::PassKey<TabStripServiceImpl> passkey,
      tabs_api::TabStripModelAdapter* tab_strip_model_adapter);
  MojoTreeBuilder(const MojoTreeBuilder&) = delete;
  MojoTreeBuilder& operator=(const MojoTreeBuilder&) = delete;
  ~MojoTreeBuilder() = default;

  tabs_api::mojom::TabCollectionContainerPtr BuildTree(
      const tabs::TabCollection* root_collection);

 private:
  // Recursive method that iterates through a tab collection
  tabs_api::mojom::TabCollectionContainerPtr BuildRecursive(
      tabs::TabCollection::Iterator& it,
      tabs::TabCollection::Iterator& end_it);

  // Helper methods to build mojo container objects
  tabs_api::mojom::TabContainerPtr BuildTab(
      const tabs::TabInterface* tab_interface);
  tabs_api::mojom::TabCollectionContainerPtr BuildTabCollectionContainer(
      const tabs::TabCollection* tab_collection);

  const base::PassKey<TabStripServiceImpl> passkey_;
  const raw_ptr<tabs_api::TabStripModelAdapter> tab_strip_model_adapter_;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TREE_BUILDER_MOJO_TREE_BUILDER_H_
