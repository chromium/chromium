// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_LAYOUT_STATE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_LAYOUT_STATE_H_

#include "chrome/browser/ui/tabs/tab_types.h"

// Contains the data necessary to determine the bounds of a tab even while
// it's in the middle of animating between states.  Immutable (except for
// replacement via assignment).
class TabLayoutState {
 public:
  TabLayoutState() = default;

  TabLayoutState(TabOpen openness, TabPinned pinnedness, TabActive activeness)
      : openness_(openness), pinnedness_(pinnedness), activeness_(activeness) {}

  TabOpen open() const { return openness_; }
  TabPinned pinned() const { return pinnedness_; }
  TabActive active() const { return activeness_; }

  TabLayoutState WithOpen(TabOpen open) const;
  TabLayoutState WithPinned(TabPinned pinned) const;
  TabLayoutState WithActive(TabActive active) const;

  bool IsClosed() const;

 private:
  // Whether this tab is open or closed.
  TabOpen openness_ = TabOpen::kOpen;

  // Whether this tab is pinned or not.
  TabPinned pinnedness_ = TabPinned::kUnpinned;

  // Whether this tab is active or inactive.
  TabActive activeness_ = TabActive::kActive;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_LAYOUT_STATE_H_
