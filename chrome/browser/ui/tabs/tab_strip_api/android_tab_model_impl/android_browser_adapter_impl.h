// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ANDROID_TAB_MODEL_IMPL_ANDROID_BROWSER_ADAPTER_IMPL_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ANDROID_TAB_MODEL_IMPL_ANDROID_BROWSER_ADAPTER_IMPL_H_

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/browser_adapter.h"

namespace tabs_api {

class AndroidBrowserAdapterImpl : public BrowserAdapter {
 public:
  AndroidBrowserAdapterImpl() = default;
  AndroidBrowserAdapterImpl(const AndroidBrowserAdapterImpl&&) = delete;
  AndroidBrowserAdapterImpl operator=(const AndroidBrowserAdapterImpl&) =
      delete;
  ~AndroidBrowserAdapterImpl() override = default;

  std::vector<std::unique_ptr<TabStripModelAdapter>>
  CreateAllTabStripModelAdaptersForProfile() override;

  // TabHandle could potentially be null to indicate that tab creation.
  tabs::TabHandle AddTabAt(const GURL& url,
                           std::optional<int> index,
                           std::optional<tab_groups::TabGroupId> group,
                           bool pinned) override;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ANDROID_TAB_MODEL_IMPL_ANDROID_BROWSER_ADAPTER_IMPL_H_
