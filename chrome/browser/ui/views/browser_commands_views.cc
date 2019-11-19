// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"

namespace chrome {

base::Optional<int> GetKeyboardFocusedTabIndex(const Browser* browser) {
  BrowserView* view = BrowserView::GetBrowserViewForBrowser(browser);
  if (view && view->tabstrip())
    return view->tabstrip()->GetFocusedTabIndex();
  return base::nullopt;
}

}  // namespace chrome
