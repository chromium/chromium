// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_list_enumerator.h"

#include <algorithm>
#include <utility>

#include "base/check.h"

BrowserListEnumerator::BrowserListEnumerator(bool enumerate_new_browser)
    : enumerate_new_browser_(enumerate_new_browser),
      browsers_(BrowserList::GetInstance()->deprecated_begin(),
                BrowserList::GetInstance()->deprecated_end()) {
  BrowserList::GetInstance()->AddObserver(this);
}

BrowserListEnumerator::BrowserListEnumerator(
    BrowserList::BrowserVector browser_list,
    bool enumerate_new_browser)
    : enumerate_new_browser_(enumerate_new_browser),
      browsers_(std::move(browser_list)) {
  BrowserList::GetInstance()->AddObserver(this);
}

BrowserListEnumerator::~BrowserListEnumerator() {
  BrowserList::GetInstance()->RemoveObserver(this);
}

void BrowserListEnumerator::OnBrowserAdded(Browser* browser) {
  DCHECK(!std::ranges::contains(browsers_, browser));
  if (enumerate_new_browser_) {
    browsers_.push_back(browser);
  }
}

void BrowserListEnumerator::OnBrowserRemoved(Browser* browser) {
  std::erase(browsers_, browser);
}

Browser* BrowserListEnumerator::Next() {
  Browser* browser = browsers_.front();
  browsers_.erase(browsers_.begin());
  bool found = false;
  for (auto it = BrowserList::GetInstance()->deprecated_begin();
       it != BrowserList::GetInstance()->deprecated_end(); ++it) {
    if (*it == browser) {
      found = true;
      break;
    }
  }
  DCHECK(found);
  return browser;
}
