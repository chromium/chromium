// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_VIEWS_H_

#include <memory>

#include "components/tab_groups/tab_group_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

class Tab;
class TabGroupHeader;
class TabGroupHighlight;
class TabGroupUnderline;
class TabStrip;

// The manager of all views associated with a tab group. This handles visual
// calculations and updates. Painting is done in TabStrip.
class TabGroupViews {
 public:
  // Creates the various views representing a tab group and adds them to
  // |tab_strip| as children.  Assumes these views are not destroyed before
  // |this|.
  TabGroupViews(TabStrip* tab_strip, const tab_groups::TabGroupId& group);

  // Destroys the views added during the constructor.
  ~TabGroupViews();

  tab_groups::TabGroupId group() const { return group_; }
  TabGroupHeader* header() { return header_; }
  TabGroupHighlight* highlight() { return highlight_; }
  TabGroupUnderline* underline() { return underline_; }

  // Updates bounds of all elements not explicitly positioned by the tab strip.
  // This currently includes both the underline and highlight.
  void UpdateBounds();

  // Updates the group title and color and ensures that all elements that might
  // need repainting are repainted.
  void OnGroupVisualsChanged();

  // Returns the bounds of the entire group, including the header and all tabs.
  gfx::Rect GetBounds() const;

  // Returns the last tab in the group. Used for some visual calculations.
  const Tab* GetLastTabInGroup() const;

  // Returns the group color.
  SkColor GetGroupColor() const;

  // Returns the tab highlight background color. Needed to layer painting for
  // the group background highlight.
  SkColor GetTabBackgroundColor() const;

  // Returns the group background color, which matches the non-active selected
  // tab color. Needed to layer painting for the group background highlight.
  SkColor GetGroupBackgroundColor() const;

 private:
  TabStrip* const tab_strip_;
  const tab_groups::TabGroupId group_;
  TabGroupHeader* header_;
  TabGroupHighlight* highlight_;
  TabGroupUnderline* underline_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_VIEWS_H_
