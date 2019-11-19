// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_SLOT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_SLOT_VIEW_H_

#include "chrome/browser/ui/tabs/tab_group_id.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout.h"
#include "ui/views/view.h"

// View that can be laid out in the tabstrip.
class TabSlotView : public views::View {
 public:
  enum class ViewType {
    kTab,
    kTabGroupHeader,
  };

  TabSlotView();
  ~TabSlotView() override;

  // Returns the type of TabSlotView that this is. Used for distinguishing
  // between Tabs and TabGroupHeaders when dragging.
  virtual ViewType GetTabSlotViewType() const = 0;

  // Returns the TabSizeInfo for this View. Used by tab strip layout to size it
  // appropriately.
  virtual TabSizeInfo GetTabSizeInfo() const = 0;

  // Used to set the tab group that this view belongs to.
  void set_group(base::Optional<TabGroupId> group) { group_ = group; }
  base::Optional<TabGroupId> group() const { return group_; }

  // Used to mark the view as having been detached.  Once this has happened, the
  // view should be invisibly closed.  This is irreversible.
  void set_detached() { detached_ = true; }
  bool detached() const { return detached_; }

  // Used to mark the view as being dragged.
  void set_dragging(bool dragging) { dragging_ = dragging; }
  bool dragging() const { return dragging_; }

 private:
  base::Optional<TabGroupId> group_;

  // True if the view has been detached.
  bool detached_ = false;

  // True if the tab is being dragged.
  bool dragging_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_SLOT_VIEW_H_
