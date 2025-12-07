// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_TREE_BUILDER_TAB_WALKER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_TREE_BUILDER_TAB_WALKER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"

namespace tabs_api {

class TabWalker {
 public:
  TabWalker(const TabStripModel* model, const tabs::TabInterface* tab);

  mojom::ContainerPtr Walk();

 private:
  raw_ptr<const TabStripModel> model_;
  raw_ptr<const tabs::TabInterface> target_;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_TREE_BUILDER_TAB_WALKER_H_
