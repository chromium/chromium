// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_TAB_STRIP_REGION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_TAB_STRIP_REGION_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/tab_strip_view_interface.h"
#include "chrome/browser/ui/views/tabs/tab_search_container.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/views/accessible_pane_view.h"

class BrowserView;

namespace views {
class Button;
}
class NewTabButton;
class TabStripActionContainer;
class TabSearchButton;
class TabStrip;
class TabStripScrollContainer;
class ProductSpecificationsButton;
class TabSearchPositionMetricsLogger;

// Container for the tabstrip and the other views sharing space with it -
// with the exception of the caption buttons.
class TabStripRegionView final : public TabStripViewInterface {
  METADATA_HEADER(TabStripRegionView, views::AccessiblePaneView)

 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(TabSearchPositionEnum)
  enum class TabSearchPositionEnum {
    kLeading = 0,
    kTrailing = 1,
    kMaxValue = kTrailing,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:TabSearchPosition)

  explicit TabStripRegionView(BrowserView* browser_view);
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

  views::Button* GetNewTabButton();

  views::Button* new_tab_button_for_testing() { return new_tab_button_; }

  views::View* reserved_grab_handle_space_for_testing() {
    return reserved_grab_handle_space_;
  }

  // views::View:
  // The TabSearchButton and NewTabButton may need to be rendered above the
  // TabStrip, but FlexLayout needs the children to be stored in the correct
  // order in the view.
  views::View::Views GetChildrenInZOrder() override;

  // Calls the parent Layout, but in some cases may also need to manually
  // position the TabSearchButton to layer over the TabStrip.
  void Layout(PassKey) override;

  // These system drag & drop methods forward the events to TabDragController to
  // support its fallback tab dragging mode in the case where the platform
  // can't support the usual run loop based mode.
  // We need to handle this here instead of in TabStrip, because TabStrip's
  // bounds don't contain the empty space to the right of the last tab.
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
  views::View* GetDefaultFocusableChild() override;

  views::View* GetTabStripContainerForTesting() { return tab_strip_container_; }

  const Profile* profile() { return profile_; }

  TabStrip* tab_strip() { return tab_strip_; }

  // TabStripViewInterface:
  gfx::Size GetMinimumSize() const override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  bool IsTabStripEditable() const override;
  void SetTabStripNotEditableForTesting() const override;
  bool IsTabStripCloseable() const override;
  bool IsAnimating() const override;
  void StopAnimating() override;
  void UpdateLoadingAnimations(const base::TimeDelta& elapsed_time) override;
  std::optional<int> GetFocusedTabIndex() const override;
  Tab* GetTabAnchorViewAt(int tab_index) override;
  views::View* GetTabGroupAnchorView(
      const tab_groups::TabGroupId& group) override;
  TabDragContext* GetDragContext() override;
  void SetTabStripObserver(TabStripObserver* observer) override;

  void LogTabSearchPositionForTesting();

 private:
  // Updates the border padding for `new_tab_button_` and
  // `tab_search_container_`, if present.  This should be called whenever any
  // input of the computation of the border's sizing changes.
  void UpdateButtonBorders();

  // Updates the left and right margins for the tab strip. This should be
  // called whenever `tab_search_container_` changes size, if
  // `render_tab_search_before_tab_strip_` is true.
  void UpdateTabStripMargin();

  // Gets called on `Layout` and adjusts the x-axis position of the `view` based
  // on `offset`. This should only used for views that show before tab strip.
  void AdjustViewBoundsRect(View* view, int offset);

  raw_ptr<const Profile> profile_ = nullptr;
  raw_ptr<TabStripActionContainer> tab_strip_action_container_ = nullptr;
  raw_ptr<views::View> tab_strip_container_ = nullptr;
  raw_ptr<views::View> reserved_grab_handle_space_ = nullptr;
  raw_ptr<TabStrip> tab_strip_ = nullptr;
  raw_ptr<TabStripScrollContainer> tab_strip_scroll_container_ = nullptr;
  raw_ptr<views::Button> new_tab_button_ = nullptr;
  raw_ptr<TabSearchContainer> tab_search_container_ = nullptr;
  raw_ptr<ProductSpecificationsButton> product_specifications_button_ = nullptr;

  // On some platforms for Chrome Refresh, the TabSearchButton should be
  // laid out before the TabStrip. Storing this configuration prevents
  // rechecking the child order on every layout.
  const bool render_tab_search_before_tab_strip_;

  std::unique_ptr<TabSearchPositionMetricsLogger>
      tab_search_position_metrics_logger_;

  const base::CallbackListSubscription subscription_ =
      ui::TouchUiController::Get()->RegisterCallback(
          base::BindRepeating(&TabStripRegionView::UpdateButtonBorders,
                              base::Unretained(this)));
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_TAB_STRIP_REGION_VIEW_H_
