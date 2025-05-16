// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_BROWSER_ADAPTER_IMPL_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_BROWSER_ADAPTER_IMPL_H_

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/browser_adapter.h"

namespace tabs_api {

// A simple forwarder proxy for the browser. Avoid adding logic to this class.
// It should *only* forward requests to the browser window.
class BrowserAdapterImpl : public BrowserAdapter {
 public:
  explicit BrowserAdapterImpl(BrowserWindowInterface* browser)
      : browser_(browser) {}
  BrowserAdapterImpl(const BrowserAdapterImpl&) = delete;
  BrowserAdapterImpl operator=(const BrowserAdapterImpl&) = delete;
  ~BrowserAdapterImpl() override = default;

  tabs::TabHandle AddTabAt(const GURL& url, std::optional<int> index) override;

 private:
  raw_ptr<BrowserWindowInterface> browser_;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ADAPTERS_BROWSER_ADAPTER_IMPL_H_
