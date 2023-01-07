// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_tab_menu_model_delegate.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tab_strip_model_delegate.h"

namespace chrome {

BrowserTabMenuModelDelegate::BrowserTabMenuModelDelegate(Browser* browser)
    : browser_(browser) {}

BrowserTabMenuModelDelegate::~BrowserTabMenuModelDelegate() = default;

std::vector<Browser*>
BrowserTabMenuModelDelegate::GetExistingWindowsForMoveMenu() {
  std::vector<Browser*> browsers;

  const BrowserList* browser_list = BrowserList::GetInstance();
  for (BrowserList::const_reverse_iterator it =
           browser_list->begin_browsers_ordered_by_activation();
       it != browser_list->end_browsers_ordered_by_activation(); ++it) {
    Browser* browser = *it;

    // We can only move into a tabbed view of the same profile, and not the same
    // window we're currently in.
    if (browser != browser_ && browser->is_type_normal() &&
        browser->profile() == browser_->profile()) {
      browsers.push_back(browser);
    }
  }
  return browsers;
}

}  // namespace chrome
