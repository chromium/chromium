// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/adapters/browser_adapter_impl.h"

#include "chrome/browser/ui/browser_tabstrip.h"

namespace tabs_api {

content::WebContents* BrowserAdapterImpl::AddTabAt(const GURL& url, int index) {
  return chrome::AddAndReturnTabAt(browser_->GetBrowserForMigrationOnly(), url,
                                   index, true);
}

}  // namespace tabs_api
