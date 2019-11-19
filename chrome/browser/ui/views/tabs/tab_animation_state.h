// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_ANIMATION_STATE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_ANIMATION_STATE_H_

#include <vector>
#include "chrome/browser/ui/tabs/tab_types.h"

// Contains the data necessary to determine the bounds of a tab even while
// it's in the middle of animating between states.  Immutable (except for
// replacement via assignment).
class TabAnimationState {
 public:
  // Returns the TabAnimationState that expresses the provided
  // ideal tab state. These correspond to the endpoints of animations.
  // |open| controls whether the returned TabAnimationState is fully open or
  // closed. |pinned| and |active| are analogous. |tab_index_offset| is the
  // distance, in tab indices, away from its current model position the tab
  // should be drawn at. It may be negative.
  static TabAnimationState ForIdealTabState(TabOpen open,
                                            TabPinned pinned,
                                            TabActive active,
                                            int tab_index_offset);

  // Interpolates from |origin| to |target| by |value|.
  // |value| should be in the [0, 1] interval, where 0
  // corresponds to |origin| and 1 to |target|.
  static TabAnimationState Interpolate(float value,
                                       TabAnimationState origin,
                                       TabAnimationState target);

  float openness() const { return openness_; }
  float pinnedness() const { return pinnedness_; }
  float activeness() const { return activeness_; }

  TabAnimationState WithOpen(TabOpen open) const;
  TabAnimationState WithPinned(TabPinned pinned) const;
  TabAnimationState WithActive(TabActive active) const;

  int GetLeadingEdgeOffset(std::vector<int> tab_widths, int my_index) const;

  bool IsFullyClosed() const;

 private:
  TabAnimationState(float openness,
                    float pinnedness,
                    float activeness,
                    float normalized_leading_edge_x)
      : openness_(openness),
        pinnedness_(pinnedness),
        activeness_(activeness),
        normalized_leading_edge_x_(normalized_leading_edge_x) {}

  // The degree to which the tab is open. 1 if it is, 0 if it is not, and in
  // between if it's in the process of animating between open and closed.
  float openness_;

  // The degree to which the tab is pinned. 1 if it is, 0 if it is not, and in
  // between if it's in the process of animating between pinned and unpinned.
  float pinnedness_;

  // The degree to which the tab is active. 1 if it is, 0 if it is not, and in
  // between if it's in the process of animating between active and inactive.
  float activeness_;

  // The offset, in number of tab slots, of the tab's bounds relative to the
  // space dedicated for it in the tabstrip.
  float normalized_leading_edge_x_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_ANIMATION_STATE_H_
