// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_VIEWS_H_

#include <memory>
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/tabs/tab_slot_controller.h"
#include "components/tab_groups/tab_group_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

class TabGroupHeader;
class TabGroupHighlight;
class TabGroupUnderline;
class TabStrip;
class TabGroupStyle;

// The manager of all views associated with a tab group. This handles visual
// calculations and updates. Painting is done in TabStrip.
class TabGroupViews {
 public:
  // Creates the various views representing a tab group and adds them to
  // |container_view| and |drag_container_view| as children.  Assumes these
  // views are not destroyed before |this|.
  TabGroupViews(views::View* container_view,
                views::View* drag_container_view,
                TabSlotController& tab_slot_controller,
                const tab_groups::TabGroupId& group);

  // Destroys the views added during the constructor.
  ~TabGroupViews();

  tab_groups::TabGroupId group() const { return group_; }
  TabGroupHeader* header() const { return header_; }
  TabGroupHighlight* highlight() const { return highlight_; }
  TabGroupUnderline* underline() const { return underline_; }
  TabGroupUnderline* drag_underline() const { return drag_underline_; }

  // Updates bounds of all elements not explicitly positioned by the tab strip.
  // This currently includes both the underline and highlight.
  void UpdateBounds();

  // Updates the group title and color and ensures that all elements that might
  // need repainting are repainted.
  void OnGroupVisualsChanged();

  // Returns the bounds of the entire group, including the header and all tabs.
  gfx::Rect GetBounds() const;

  // Returns the group color.
  SkColor GetGroupColor() const;

  // Finds the first and last tab or group header belonging to `group_` from the
  // whole Tabstrip.
  std::tuple<const views::View*, const views::View*>
  GetLeadingTrailingGroupViews() const;

 private:
  const raw_ref<TabSlotController> tab_slot_controller_;
  const tab_groups::TabGroupId group_;
  raw_ptr<TabGroupHeader> header_;
  raw_ptr<TabGroupHighlight> highlight_;
  raw_ptr<TabGroupUnderline> underline_;
  raw_ptr<TabGroupUnderline> drag_underline_;
  std::unique_ptr<const TabGroupStyle> style_;

  bool InTearDown() const;

  // Finds the first and last tab or group header belonging to |group_|, only
  // including views that are being dragged.
  std::tuple<views::View*, views::View*> GetLeadingTrailingDraggedGroupViews()
      const;

  // Finds the first and last tab or group header belonging to |group_| within
  // |children|.
  std::tuple<views::View*, views::View*> GetLeadingTrailingGroupViews(
      std::vector<raw_ptr<views::View, VectorExperimental>> children) const;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_VIEWS_H_
