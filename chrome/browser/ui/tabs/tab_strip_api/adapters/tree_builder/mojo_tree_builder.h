// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_TREE_BUILDER_MOJO_TREE_BUILDER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_TREE_BUILDER_MOJO_TREE_BUILDER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/tabs/public/tab_collection.h"

namespace tabs_api {

class MojoTreeBuilder {
 public:
  explicit MojoTreeBuilder(const TabStripModel* model);
  MojoTreeBuilder(const MojoTreeBuilder&&) = delete;
  MojoTreeBuilder& operator=(const MojoTreeBuilder&) = delete;
  ~MojoTreeBuilder() = default;

  mojom::ContainerPtr Build(tabs::TabCollection::Handle root) const;

 private:
  const raw_ptr<const TabStripModel> model_;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_TREE_BUILDER_MOJO_TREE_BUILDER_H_
