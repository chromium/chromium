// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

void ForEachTabInterface(base::FunctionRef<bool(tabs::TabInterface*)> on_tab) {
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        // Store initial tab list as weak pointers to handle tab destruction
        // during iteration.
        std::vector<base::WeakPtr<tabs::TabInterface>> tabs_weak;
        std::ranges::transform(browser->GetAllTabInterfaces(),
                               std::back_inserter(tabs_weak),
                               &tabs::TabInterface::GetWeakPtr);

        for (auto tab_weak : tabs_weak) {
          if (tab_weak && !on_tab(tab_weak.get())) {
            return false;  // stop iteration.
          }
        }
        return true;  // continue iteration.
      });
}

}  // namespace tabs
