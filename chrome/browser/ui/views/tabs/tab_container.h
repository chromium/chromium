// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_H_

#include <memory>
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout_helper.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

class TabStrip;
class TabGroupHeader;

// A View that contains a sequence of Tabs for the TabStrip.
class TabContainer : public views::View, public views::ViewTargeterDelegate {
 public:
  METADATA_HEADER(TabContainer);

  explicit TabContainer(TabStripController* controller_);
  ~TabContainer() override;

  Tab* AddTab(std::unique_ptr<Tab> tab, int model_index, TabPinned pinned);
  void MoveTab(Tab* tab, int from_model_index, int to_model_index);

  // Remove the tab from |tabs_view_model_|, but *not* from the View hierarchy,
  // so it can be animated closed.
  void RemoveTabFromViewModel(int index);

  void MoveGroupHeader(TabGroupHeader* group_header, int first_tab_model_index);

  int GetModelIndexOf(const TabSlotView* slot_view);

  views::ViewModelT<Tab>* tabs_view_model() { return &tabs_view_model_; }

  Tab* GetTabAtModelIndex(int index) const;

  int GetTabCount() const;

  // Updates the indexes and count for a11y data on all tabs. Used by some
  // screen readers (e.g. ChromeVox).
  void UpdateAccessibleTabIndices();

  void HandleLongTap(ui::GestureEvent* event);

  bool IsRectInWindowCaption(const gfx::Rect& rect);

  // TODO (1295774): Move callers down into TabContainer so this
  // encapsulation-breaking getter can be removed.
  TabStripLayoutHelper* layout_helper() const { return layout_helper_.get(); }

  // views::View
  gfx::Size GetMinimumSize() const override;
  views::View* GetTooltipHandlerForPoint(
      const gfx::Point& point_in_tab_container_coords) override;

  // views::ViewTargeterDelegate:
  views::View* TargetForRect(views::View* root, const gfx::Rect& rect) override;

 private:
  // Returns the corresponding view index of a |tab| to be inserted at
  // |to_model_index|. Used to reorder the child views of the tab container
  // so that focus order stays consistent with the visual tab order.
  // |from_model_index| is where the tab currently is, if it's being moved
  // instead of added.
  int GetViewInsertionIndex(absl::optional<tab_groups::TabGroupId> group,
                            absl::optional<int> from_model_index,
                            int to_model_index) const;

  int GetViewIndexForModelIndex(int tab_model_index) const;

  // Returns true if the specified point in the TabContainer's local coordinate
  // space is within the hit-test region of the specified Tab.
  bool IsPointInTab(Tab* tab, const gfx::Point& point_in_tab_container_coords);

  // For a given point, finds a tab that is hit by the point. If the point hits
  // an area on which two tabs are overlapping, the tab is selected as follows:
  // - If one of the tabs is active, select it.
  // - Select the left one.
  // If no tabs are hit, returns null.
  Tab* FindTabHitByPoint(const gfx::Point& point_in_tab_container_coords);

  bool IsValidModelIndex(int model_index) const;

  // There is a one-to-one mapping between each of the
  // tabs in the TabStripModel and |tabs_view_model_|.
  // Because we animate tab removal there exists a
  // period of time where a tab is displayed but not
  // in the model. When this occurs the tab is removed
  // from |tabs_view_model_|, but remains in
  // |layout_helper_| (and remains a View child) until
  // the remove animation completes.
  views::ViewModelT<Tab> tabs_view_model_;

  TabStripController* controller_;

  std::unique_ptr<TabStripLayoutHelper> layout_helper_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_H_
