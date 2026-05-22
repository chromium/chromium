// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_STRIP_ADAPTERS_BROWSER_ADAPTER_H_
#define COMPONENTS_BROWSER_APIS_TAB_STRIP_ADAPTERS_BROWSER_ADAPTER_H_

#include <memory>
#include <vector>

#include "components/browser_apis/tab_strip/adapters/tab_strip_model_adapter.h"
#include "components/tabs/public/tab_interface.h"
#include "url/gurl.h"

namespace tabs_api {

// Pull out a subset of browser APIs into an adapter object. This allows us
// to more easily control dependencies when testing.
class BrowserAdapter {
 public:
  virtual ~BrowserAdapter() {}

  virtual std::vector<std::unique_ptr<TabStripModelAdapter>>
  CreateAllTabStripModelAdaptersForProfile() = 0;

  // TabHandle could potentially be null to indicate that tab creation.
  virtual tabs::TabHandle AddTabAt(const GURL& url,
                                   std::optional<int> index,
                                   std::optional<tab_groups::TabGroupId> group,
                                   bool pinned) = 0;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_STRIP_ADAPTERS_BROWSER_ADAPTER_H_
