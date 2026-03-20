// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_browser_adapter_impl.h"

#include "base/notreached.h"

namespace tabs_api {

std::vector<std::unique_ptr<TabStripModelAdapter>>
AndroidBrowserAdapterImpl::CreateAllTabStripModelAdaptersForProfile() {
  NOTREACHED() << "not implemented";
}

tabs::TabHandle AndroidBrowserAdapterImpl::AddTabAt(
    const GURL& url,
    std::optional<int> index,
    std::optional<tab_groups::TabGroupId> group,
    bool pinned) {
  NOTREACHED() << "not implemented";
}

}  //  namespace tabs_api
