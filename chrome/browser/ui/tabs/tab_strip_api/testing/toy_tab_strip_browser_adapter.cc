// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip_browser_adapter.h"

namespace tabs_api::testing {

ToyTabStripBrowserAdapter::ToyTabStripBrowserAdapter(ToyTabStrip* tab_strip)
    : tab_strip_(tab_strip) {}

tabs::TabHandle ToyTabStripBrowserAdapter::AddTabAt(
    const GURL& url,
    std::optional<int> index,
    std::optional<tab_groups::TabGroupId> group,
    bool pinned) {
  return tab_strip_->AddTabAt(url, index);
}

}  // namespace tabs_api::testing
