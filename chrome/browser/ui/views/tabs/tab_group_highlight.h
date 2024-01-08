// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_HIGHLIGHT_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_HIGHLIGHT_H_

#include "base/memory/raw_ptr.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class TabGroupViews;
class TabGroupStyle;

// View for tab group highlights in the tab strip, which indicate that a group
// is in a selected state. There is one highlight for each group, which is
// positioned across all tabs in the group and painted by the tab strip.
class TabGroupHighlight : public views::View {
  METADATA_HEADER(TabGroupHighlight, views::View)

 public:
  TabGroupHighlight(TabGroupViews* tab_group_views,
                    const tab_groups::TabGroupId& group,
                    const TabGroupStyle& style);
  TabGroupHighlight(const TabGroupHighlight&) = delete;
  TabGroupHighlight& operator=(const TabGroupHighlight&) = delete;

  void UpdateBounds(views::View* leading_view, views::View* trailing_view);

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  bool GetCanProcessEventsWithinSubtree() const override;

 private:
  // Returns the highlight shape, which immitates the tab highlight shape.
  SkPath GetPath() const;

  const raw_ptr<TabGroupViews> tab_group_views_;
  const tab_groups::TabGroupId group_;
  const raw_ref<const TabGroupStyle> style_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_HIGHLIGHT_H_
