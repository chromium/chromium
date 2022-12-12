// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_TAB_STRIP_REGION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_TAB_STRIP_REGION_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/views/accessible_pane_view.h"

namespace views {
class FlexLayout;
}

class NewTabButton;
class TabSearchButton;
class TabStrip;
class TipMarqueeView;
class TabStripScrollContainer;

// Container for the tabstrip and the other views sharing space with it -
// with the exception of the caption buttons.
class TabStripRegionView final : public views::AccessiblePaneView {
 public:
  METADATA_HEADER(TabStripRegionView);
  explicit TabStripRegionView(std::unique_ptr<TabStrip> tab_strip);
  TabStripRegionView(const TabStripRegionView&) = delete;
  TabStripRegionView& operator=(const TabStripRegionView&) = delete;
  ~TabStripRegionView() override;

  // Returns true if the specified rect intersects the window caption area of
  // the browser window. |rect| is in the local coordinate space
  // of |this|.
  bool IsRectInWindowCaption(const gfx::Rect& rect);

  // A convenience function which calls |IsRectInWindowCaption()| with a rect of
  // size 1x1 and an origin of |point|. |point| is in the local coordinate space
  // of |this|.
  bool IsPositionInWindowCaption(const gfx::Point& point);

  NewTabButton* new_tab_button() { return new_tab_button_; }

  TabSearchButton* tab_search_button() { return tab_search_button_; }

  TipMarqueeView* tip_marquee_view() { return tip_marquee_view_; }

  views::View* reserved_grab_handle_space_for_testing() {
    return reserved_grab_handle_space_;
  }

  // views::View:

  // These system drag & drop methods forward the events to TabDragController to
  // support its fallback tab dragging mode in the case where the platform
  // can't support the usual run loop based mode.
  bool CanDrop(const OSExchangeData& data) override;
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  // We don't override GetDropCallback() because we don't actually want to
  // transfer any data.

  // views::AccessiblePaneView:
  void ChildPreferredSizeChanged(views::View* child) override;
  gfx::Size GetMinimumSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  views::View* GetDefaultFocusableChild() override;

  views::FlexLayout* layout_manager_for_testing() { return layout_manager_; }
  views::View* GetTabStripContainerForTesting() { return tab_strip_container_; }

 private:
  // Updates the border padding for |new_tab_button_|.  This should be called
  // whenever any input of the computation of the border's sizing changes.
  void UpdateNewTabButtonBorder();

  raw_ptr<views::FlexLayout, DanglingUntriaged> layout_manager_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> tab_strip_container_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> reserved_grab_handle_space_ = nullptr;
  raw_ptr<TabStrip, DanglingUntriaged> tab_strip_ = nullptr;
  raw_ptr<TabStripScrollContainer, DanglingUntriaged>
      tab_strip_scroll_container_ = nullptr;
  raw_ptr<NewTabButton, DanglingUntriaged> new_tab_button_ = nullptr;
  raw_ptr<TabSearchButton, DanglingUntriaged> tab_search_button_ = nullptr;
  raw_ptr<TipMarqueeView, DanglingUntriaged> tip_marquee_view_ = nullptr;

  const base::CallbackListSubscription subscription_ =
      ui::TouchUiController::Get()->RegisterCallback(
          base::BindRepeating(&TabStripRegionView::UpdateNewTabButtonBorder,
                              base::Unretained(this)));
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_TAB_STRIP_REGION_VIEW_H_
