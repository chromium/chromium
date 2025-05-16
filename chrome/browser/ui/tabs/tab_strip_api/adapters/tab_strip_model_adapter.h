// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_TAB_STRIP_MODEL_ADAPTER_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_TAB_STRIP_MODEL_ADAPTER_H_

#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs_api {

// Tab strip has a large API service that is difficult to implement under test.
// We only need a subset of the AP, so an adapter is used to proxy those
// methods. This makes it easier to swap in a fake for test.
class TabStripModelAdapter {
 public:
  virtual ~TabStripModelAdapter() {}

  virtual void AddObserver(TabStripModelObserver* observer) = 0;
  virtual void RemoveObserver(TabStripModelObserver* observer) = 0;
  virtual std::vector<tabs::TabHandle> GetTabs() = 0;
  virtual TabRendererData GetTabRendererData(int index) = 0;
  virtual void CloseTab(size_t tab_index) = 0;
  virtual std::optional<int> GetIndexForHandle(tabs::TabHandle tab_handle) = 0;
  virtual void ActivateTab(size_t index) = 0;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_TAB_STRIP_MODEL_ADAPTER_H_
