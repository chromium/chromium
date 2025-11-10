// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"

#include <optional>

#include "base/check.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

// This does not create a useful iterator, but providing a default constructor
// is required for forward iterators by the C++ spec.
AllTabContentsesList::Iterator::Iterator() : Iterator(true) {}

AllTabContentsesList::Iterator::Iterator(bool is_end_iter)
    : cur_(nullptr),
      browser_iterator_(BrowserList::GetInstance()->begin()),
      tab_iterator_(std::nullopt) {
  if (!is_end_iter &&
      (browser_iterator_ != BrowserList::GetInstance()->end())) {
    // Load the first WebContents into |cur_|.
    Next();
  }
}

AllTabContentsesList::Iterator::Iterator(const Iterator& iterator) = default;

AllTabContentsesList::Iterator::~Iterator() = default;

void AllTabContentsesList::Iterator::Next() {
  // The current WebContents should be valid unless we are at the beginning.
  DCHECK(cur_ || (browser_iterator_ != BrowserList::GetInstance()->end()))
      << "Trying to advance past the end";

  // Update |cur_| to the next WebContents in the list.
  while (browser_iterator_ != BrowserList::GetInstance()->end()) {
    if (!tab_iterator_.has_value()) {
      tab_iterator_ = (*browser_iterator_)->tab_strip_model()->begin();
    }

    if (tab_iterator_ != (*browser_iterator_)->tab_strip_model()->end()) {
      cur_ = tab_iterator_.value()->GetContents();
      // Increment by reference has better performance.
      ++(tab_iterator_.value());
      return;
    } else {
      tab_iterator_ = std::nullopt;
      browser_iterator_++;
    }
  }

  // Reached the end.
  cur_ = nullptr;
}

const AllTabContentsesList& AllTabContentses() {
  static const AllTabContentsesList all_tabs;
  return all_tabs;
}

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
