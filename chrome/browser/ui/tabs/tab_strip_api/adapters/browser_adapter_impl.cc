// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/browser_adapter_impl.h"

#include "chrome/browser/ui/browser_tabstrip.h"

namespace tabs_api {

// Magic number to signal new tab should be appended.
constexpr int kAppendNewTab = -1;

tabs::TabHandle BrowserAdapterImpl::AddTabAt(const GURL& url,
                                             std::optional<int> index) {
  auto* contents =
      chrome::AddAndReturnTabAt(browser_->GetBrowserForMigrationOnly(), url,
                                index.value_or(kAppendNewTab), true);
  return contents ? tabs::TabInterface::GetFromContents(contents)->GetHandle()
                  : tabs::TabHandle::Null();
}

}  // namespace tabs_api
