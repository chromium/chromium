// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_TOY_TAB_STRIP_MODEL_ADAPTER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_TOY_TAB_STRIP_MODEL_ADAPTER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip.h"

namespace tabs_api::testing {

class ToyTabStripModelAdapter : public TabStripModelAdapter {
 public:
  explicit ToyTabStripModelAdapter(ToyTabStrip* tab_strip);
  ToyTabStripModelAdapter(const ToyTabStripModelAdapter&) = delete;
  ToyTabStripModelAdapter operator=(const ToyTabStripModelAdapter&&) = delete;
  ~ToyTabStripModelAdapter() override = default;

  void AddObserver(TabStripModelObserver* observer) override;
  void RemoveObserver(TabStripModelObserver* observer) override;
  std::vector<tabs::TabHandle> GetTabs() const override;
  TabRendererData GetTabRendererData(int index) const override;
  void CloseTab(size_t tab_index) override;
  std::optional<int> GetIndexForHandle(tabs::TabHandle tab_handle) override;
  void ActivateTab(size_t index) override;
  void MoveTab(tabs::TabHandle handle, Position position) override;
  mojom::TabCollectionContainerPtr GetTabStripTopology() override;

 private:
  raw_ptr<ToyTabStrip> tab_strip_;
};

}  // namespace tabs_api::testing

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_TOY_TAB_STRIP_MODEL_ADAPTER_H_
