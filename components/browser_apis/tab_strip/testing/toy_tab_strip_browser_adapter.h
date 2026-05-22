// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_STRIP_TESTING_TOY_TAB_STRIP_BROWSER_ADAPTER_H_
#define COMPONENTS_BROWSER_APIS_TAB_STRIP_TESTING_TOY_TAB_STRIP_BROWSER_ADAPTER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/browser_apis/tab_strip/adapters/browser_adapter.h"
#include "components/browser_apis/tab_strip/testing/toy_tab_strip.h"
#include "components/browser_apis/tab_strip/testing/toy_tab_strip_model_adapter.h"

namespace tabs_api::testing {

class ToyTabStripBrowserAdapter : public BrowserAdapter {
 public:
  explicit ToyTabStripBrowserAdapter(ToyTabStrip* tab_strip);
  ToyTabStripBrowserAdapter(const ToyTabStripBrowserAdapter&) = delete;
  ToyTabStripBrowserAdapter operator=(const ToyTabStripBrowserAdapter&&) =
      delete;
  ~ToyTabStripBrowserAdapter() = default;

  std::vector<std::unique_ptr<TabStripModelAdapter>>
  CreateAllTabStripModelAdaptersForProfile() override {
    std::vector<std::unique_ptr<TabStripModelAdapter>> results;
    results.push_back(
        std::make_unique<ToyTabStripModelAdapter>(tab_strip_.get()));
    return results;
  }

  tabs::TabHandle AddTabAt(const GURL& url,
                           std::optional<int> index,
                           std::optional<tab_groups::TabGroupId> group,
                           bool pinned) override;

 private:
  raw_ptr<ToyTabStrip> tab_strip_;
};

}  // namespace tabs_api::testing

#endif  // COMPONENTS_BROWSER_APIS_TAB_STRIP_TESTING_TOY_TAB_STRIP_BROWSER_ADAPTER_H_
