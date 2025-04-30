// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_WIDTH_CONSTRAINTS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_WIDTH_CONSTRAINTS_H_

#include "chrome/browser/ui/views/tabs/tab_layout_state.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout_types.h"

// Provides width information for a single tab during layout.
class TabWidthConstraints {
 public:
  TabWidthConstraints(const TabLayoutState& state,
                      const TabSizeInfo& size_info);

  // The smallest width this tab should ever have.
  //
  // Split tabs will have a different min width. When `compensate_for_splits`
  // is set to true and the tab is split, return the split tab minimum width.
  float GetMinimumWidth(bool compensate_for_splits = false) const;

  // The width this tab should have at the crossover point between the
  // tabstrip's two layout domains. Above this width, inactive tabs have the
  // same width as active tabs.  Below this width, inactive tabs are smaller
  // than active tabs.
  //
  // Split tabs will have a different crossover width because they should stop
  // shrinking before they reach the crossover width. When
  // `compensate_for_splits` is set to true and the tab is split, return the
  // split tab layout crossover width.
  float GetLayoutCrossoverWidth(bool compensate_for_splits = false) const;

  // The width this tab would like to have, if space is available.
  float GetPreferredWidth() const;

  const TabLayoutState get_state() const { return state_; }

 private:
  // All widths are affected by pinnedness and activeness in the same way.
  float TransformForPinnednessAndOpenness(float width) const;

  TabLayoutState state_;
  TabSizeInfo size_info_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_WIDTH_CONSTRAINTS_H_
