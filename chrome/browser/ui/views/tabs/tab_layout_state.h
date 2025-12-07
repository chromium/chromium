// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_LAYOUT_STATE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_LAYOUT_STATE_H_

#include <optional>

#include "chrome/browser/ui/tabs/tab_types.h"
#include "components/tabs/public/split_tab_id.h"

// Contains the data necessary to determine the bounds of a tab even while
// it's in the middle of animating between states.
class TabLayoutState {
 public:
  TabLayoutState() = default;

  TabLayoutState(TabOpen openness,
                 TabPinned pinnedness,
                 TabActive activeness,
                 std::optional<split_tabs::SplitTabId> splitness)
      : openness_(openness),
        pinnedness_(pinnedness),
        activeness_(activeness),
        splitness_(splitness) {}

  TabOpen open() const { return openness_; }
  TabPinned pinned() const { return pinnedness_; }
  TabActive active() const { return activeness_; }
  std::optional<split_tabs::SplitTabId> split() const { return splitness_; }

  void set_open(TabOpen open) { this->openness_ = open; }
  void set_pinned(TabPinned pinned) { this->pinnedness_ = pinned; }
  void set_active(TabActive active) { this->activeness_ = active; }

  bool IsClosed() const;

 private:
  // Whether this tab is open or closed.
  TabOpen openness_ = TabOpen::kOpen;

  // Whether this tab is pinned or not.
  TabPinned pinnedness_ = TabPinned::kUnpinned;

  // Whether this tab is active or inactive.
  TabActive activeness_ = TabActive::kActive;

  // Whether this tab is split.
  std::optional<split_tabs::SplitTabId> splitness_ = std::nullopt;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_LAYOUT_STATE_H_
