// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_TAB_STRIP_MODEL_ADAPTER_IMPL_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_TAB_STRIP_MODEL_ADAPTER_IMPL_H_

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

namespace tabs_api {

// A simple forwarder proxy for the tab strip model. Avoid adding logic to this
// class. It should *only* forward requests to the tab strip model.
class TabStripModelAdapterImpl : public TabStripModelAdapter {
 public:
  explicit TabStripModelAdapterImpl(TabStripModel* tab_strip_model)
      : tab_strip_model_(tab_strip_model) {}
  TabStripModelAdapterImpl(const TabStripModelAdapterImpl&) = delete;
  TabStripModelAdapterImpl operator=(const TabStripModelAdapterImpl&) = delete;
  ~TabStripModelAdapterImpl() override {}

  void AddObserver(TabStripModelObserver* observer) override;
  void RemoveObserver(TabStripModelObserver* observer) override;
  std::vector<tabs::TabHandle> GetTabs() override;
  TabRendererData GetTabRendererData(int index) override;
  void CloseTab(size_t tab_index) override;
  std::optional<int> GetIndexForHandle(tabs::TabHandle tab_handle) override;
  void ActivateTab(size_t index) override;

 private:
  raw_ptr<TabStripModel> tab_strip_model_;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_TAB_STRIP_MODEL_ADAPTER_IMPL_H_
