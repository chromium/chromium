// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_TYPES_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_TYPES_H_

// Sizing info for individual tabs.
struct TabSizeInfo {
  // The width of pinned tabs.
  int pinned_tab_width;

  // The min width of active/inactive tabs.
  int min_active_width;
  int min_inactive_width;

  // The width of a standard tab, which is the largest size active or inactive
  // tabs ever have.
  int standard_width;
};

// Sizing info global to the tabstrip.
struct TabLayoutConstants {
  // The height of tabs.
  int tab_height;

  // The amount adjacent tabs overlap each other.
  int tab_overlap;
};

// Inactive tabs have a smaller minimum width than the active tab. Layout has
// different behavior when inactive tabs are smaller than the active tab
// than it does when they are the same size.
enum class LayoutDomain {
  // There is not enough space for inactive tabs to match the active tab's
  // width.
  kInactiveWidthBelowActiveWidth,
  // There is enough space for inactive tabs to match the active tab's width.
  kInactiveWidthEqualsActiveWidth
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_TYPES_H_
