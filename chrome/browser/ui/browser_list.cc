// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_list.h"

#include <algorithm>

#include "base/check.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"

// static
BrowserList* BrowserList::instance_ = nullptr;

////////////////////////////////////////////////////////////////////////////////
// BrowserList, public:

// static
BrowserList* BrowserList::GetInstance() {
  BrowserList** list = &instance_;
  if (!*list) {
    *list = new BrowserList;
  }
  return *list;
}

// static
void BrowserList::AddBrowser(Browser* browser) {
  DCHECK(browser);
  DCHECK(browser->window()) << "Browser should not be added to BrowserList "
                               "until it is fully constructed.";
  GetInstance()->browsers_.push_back(browser);

  AddBrowserToActiveList(browser);
}

// static
void BrowserList::RemoveBrowser(Browser* browser) {
  // Remove |browser| from the appropriate list instance.
  BrowserList* browser_list = GetInstance();
  RemoveBrowserFrom(browser, &browser_list->browsers_ordered_by_activation_);

  RemoveBrowserFrom(browser, &browser_list->browsers_);
}

// static
void BrowserList::AddBrowserToActiveList(Browser* browser) {
  if (browser->IsActive()) {
    SetLastActive(browser);
    return;
  }

  // |BrowserList::browsers_ordered_by_activation_| should contain every
  // browser, so prepend any inactive browsers to it.
  BrowserVector* active_browsers =
      &GetInstance()->browsers_ordered_by_activation_;
  RemoveBrowserFrom(browser, active_browsers);
  active_browsers->insert(active_browsers->begin(), browser);
}

// static
void BrowserList::SetLastActive(Browser* browser) {
  BrowserList* instance = GetInstance();
  DCHECK(std::ranges::contains(instance->browsers_, browser))
      << "SetLastActive called for a browser before the browser was added to "
         "the BrowserList.";
  DCHECK(browser->window())
      << "SetLastActive called for a browser with no window set.";

  RemoveBrowserFrom(browser, &instance->browsers_ordered_by_activation_);
  instance->browsers_ordered_by_activation_.push_back(browser);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserList, private:

BrowserList::BrowserList() = default;

BrowserList::~BrowserList() = default;

// static
void BrowserList::RemoveBrowserFrom(Browser* browser,
                                    BrowserVector* browser_list) {
  auto remove_browser = std::ranges::find(*browser_list, browser);
  if (remove_browser != browser_list->end()) {
    browser_list->erase(remove_browser);
  }
}
