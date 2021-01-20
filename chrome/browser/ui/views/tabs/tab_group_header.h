// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_HEADER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_HEADER_H_

#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/widget/widget_observer.h"

class TabStrip;
struct TabSizeInfo;

namespace views {
class Label;
class View;
}

// View for tab group headers in the tab strip, which are markers of group
// boundaries. There is one header for each group, which is included in the tab
// strip flow and positioned left of the leftmost tab in the group.
class TabGroupHeader : public TabSlotView,
                       public views::ContextMenuController,
                       public views::ViewTargeterDelegate {
 public:
  TabGroupHeader(TabStrip* tab_strip, const tab_groups::TabGroupId& group);
  TabGroupHeader(const TabGroupHeader&) = delete;
  TabGroupHeader& operator=(const TabGroupHeader&) = delete;
  ~TabGroupHeader() override;

  // TabSlotView:
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnFocus() override;
  void OnThemeChanged() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  TabSlotView::ViewType GetTabSlotViewType() const override;
  TabSizeInfo GetTabSizeInfo() const override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  // Updates our visual state according to the tab_groups::TabGroupVisualData
  // for our group.
  void VisualsChanged();

  // Removes {editor_bubble_tracker_} from observing the widget.
  void RemoveObserverFromWidget(views::Widget* widget);

 private:
  friend class TabGroupEditorBubbleViewDialogBrowserTest;

  // Calculate the width for this View.
  int CalculateWidth() const;

  // Helper method used to log the time since the group was last expanded or
  // collapsed.
  void LogCollapseTime();

  TabStrip* const tab_strip_;

  views::View* title_chip_;
  views::Label* title_;

  // Focus ring for accessibility.
  views::FocusRing* focus_ring_ = nullptr;

  // Time used for logging the last time the group was collapsed or expanded.
  base::TimeTicks last_modified_expansion_;

  // Tracks whether our editor bubble is open. At most one can be open
  // at once.
  class EditorBubbleTracker : public views::WidgetObserver {
   public:
    EditorBubbleTracker() = default;
    ~EditorBubbleTracker() override;

    void Opened(views::Widget* bubble_widget);
    bool is_open() const { return is_open_; }
    views::Widget* widget() const { return widget_; }

    // views::WidgetObserver:
    void OnWidgetDestroyed(views::Widget* widget) override;

   private:
    bool is_open_ = false;
    views::Widget* widget_;
  };

  EditorBubbleTracker editor_bubble_tracker_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_HEADER_H_
