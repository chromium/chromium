// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_TOY_TAB_STRIP_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_TOY_TAB_STRIP_H_

#include "components/tabs/public/tab_interface.h"
#include "url/gurl.h"

namespace tabs_api::testing {

struct ToyTab {
  GURL gurl;
  tabs::TabHandle tab_handle;
  bool active = false;
};

class ToyTabStrip {
 public:
  ToyTabStrip() = default;
  ToyTabStrip(const ToyTabStrip&) = delete;
  ToyTabStrip& operator=(const ToyTabStrip&) = delete;
  ~ToyTabStrip() = default;

  void AddTab(ToyTab tab);
  std::vector<tabs::TabHandle> GetTabs();
  void CloseTab(size_t index);
  std::optional<int> GetIndexForHandle(tabs::TabHandle tab_handle);
  tabs::TabHandle AddTabAt(const GURL& url, std::optional<int> index);
  void ActivateTab(tabs::TabHandle handle);
  tabs::TabHandle FindActiveTab();

 private:
  std::vector<ToyTab> tabs_;
};

}  // namespace tabs_api::testing

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_TOY_TAB_STRIP_H_
