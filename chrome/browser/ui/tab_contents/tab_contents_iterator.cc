// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"

#include "base/check.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

// This does not create a useful iterator, but providing a default constructor
// is required for forward iterators by the C++ spec.
AllTabContentsesList::Iterator::Iterator() : Iterator(true) {}

AllTabContentsesList::Iterator::Iterator(bool is_end_iter)
    : tab_index_(-1),
      cur_(nullptr),
      browser_iterator_(BrowserList::GetInstance()->begin()) {
  if (!is_end_iter) {
    // Load the first WebContents into |cur_|.
    Next();
  }
}

AllTabContentsesList::Iterator::Iterator(const Iterator& iterator) = default;

AllTabContentsesList::Iterator::~Iterator() = default;

void AllTabContentsesList::Iterator::Next() {
  // The current WebContents should be valid unless we are at the beginning.
  DCHECK(cur_ || tab_index_ == -1) << "Trying to advance past the end";

  // Update |cur_| to the next WebContents in the list.
  while (browser_iterator_ != BrowserList::GetInstance()->end()) {
    if (++tab_index_ >= (*browser_iterator_)->tab_strip_model()->count()) {
      // Advance to the next Browser in the list.
      ++browser_iterator_;
      tab_index_ = -1;
      continue;
    }

    auto* next_tab =
        (*browser_iterator_)->tab_strip_model()->GetWebContentsAt(tab_index_);
    if (next_tab) {
      cur_ = next_tab;
      return;
    }
  }

  // Reached the end.
  cur_ = nullptr;
}

const AllTabContentsesList& AllTabContentses() {
  static const base::NoDestructor<AllTabContentsesList> all_tabs;
  return *all_tabs;
}
