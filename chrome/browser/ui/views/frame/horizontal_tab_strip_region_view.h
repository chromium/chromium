// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_HORIZONTAL_TAB_STRIP_REGION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_HORIZONTAL_TAB_STRIP_REGION_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "build/buildflag.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab_search_container.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/buildflags.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/views/accessible_pane_view.h"

class BrowserView;

namespace ash {
class TabScrubber;
}

namespace views {
class ActionViewController;
class Button;
class LabelButton;
}
class NewTabButton;
class TabStripActionContainer;
class TabSearchButton;
class TabStripComboButton;
class TabStrip;
class TabStripScrollContainer;
class TabSearchPositionMetricsLogger;
class TabStripControlButton;
class TabStripFlatEdgeButton;

// Container for the tabstrip and the other views sharing space with it -
// with the exception of the caption buttons.
class HorizontalTabStripRegionView final : public TabStripRegionView {
  METADATA_HEADER(HorizontalTabStripRegionView, TabStripRegionView)

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

  explicit HorizontalTabStripRegionView(BrowserView* browser_view);
  HorizontalTabStripRegionView(const HorizontalTabStripRegionView&) = delete;
  HorizontalTabStripRegionView& operator=(const HorizontalTabStripRegionView&) =
      delete;
  ~HorizontalTabStripRegionView() override;

  // Returns true if |point| falls within the window caption area of the
  // horizontal tab strip. Returns false if the point hits an interactive child
  // view. |point| is in the local coordinate space of |this|.
  bool IsPositionInWindowCaption(const gfx::Point& point);

  views::Button* new_tab_button_for_testing() { return new_tab_button_; }

  views::View* reserved_grab_handle_space_for_testing() {
    return reserved_grab_handle_space_;
  }

#if BUILDFLAG(IS_CHROMEOS)
  ash::TabScrubber* get_tab_scrubber_for_testing() {
    return tab_scrubber_.get();
  }
#endif

  // views::View:
  // The TabSearchButton and NewTabButton may need to be rendered above the
  // TabStrip, but FlexLayout needs the children to be stored in the correct
  // order in the view.
  views::View::Views GetChildrenInZOrder() override;

  // Calls the parent Layout, but in some cases may also need to manually
  // position the TabSearchButton to layer over the TabStrip.
  void Layout(PassKey) override;

  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // views::AccessiblePaneView:
  void ChildPreferredSizeChanged(views::View* child) override;
  views::View* GetDefaultFocusableChild() override;

  const Profile* profile() { return profile_; }

  TabStrip* tab_strip() { return tab_strip_; }

  TabStripFlatEdgeButton* GetTabSearchButton();
  TabStripComboButton* GetComboButton() { return combo_button_; }

#if BUILDFLAG(ENABLE_GLIC)
  views::LabelButton* GetGlicButton();
#endif  // BUILDFLAG(ENABLE_GLIC)

  // TabStripRegionView:
  void InitializeTabStrip() override;
  void ResetTabStrip() override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  bool IsTabStripEditable() const override;
  void DisableTabStripEditingForTesting() override;
  bool IsTabStripCloseable() const override;
  void UpdateLoadingAnimations(const base::TimeDelta& elapsed_time) override;
  std::optional<int> GetFocusedTabIndex() const override;
  const TabRendererData& GetTabRendererData(int tab_index) override;
  views::View* GetTabAnchorViewAt(int tab_index) override;
  views::View* GetTabGroupAnchorView(
      const tab_groups::TabGroupId& group) override;
  void OnTabGroupFocusChanged(
      std::optional<tab_groups::TabGroupId> new_focused_group_id,
      std::optional<tab_groups::TabGroupId> old_focused_group_id) override;
  TabDragContext* GetDragContext() override;
  std::optional<BrowserRootView::DropIndex> GetDropIndex(
      const ui::DropTargetEvent& event) override;
  BrowserRootView::DropTarget* GetDropTarget(
      gfx::Point loc_in_local_coords) override;
  views::View* GetViewForDrop() override;
  bool CanDrop(const OSExchangeData& data) override;
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  void SetTabStripObserver(TabStripObserver* observer) override;
  views::View* GetTabStripView() override;

  bool HasLeadingButtons() const;
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

  bool tab_strip_set_ = false;

  raw_ptr<const Profile> profile_ = nullptr;
  raw_ptr<TabStripActionContainer> tab_strip_action_container_ = nullptr;
  raw_ptr<views::View> tab_strip_container_ = nullptr;
  raw_ptr<views::View> reserved_grab_handle_space_ = nullptr;
  raw_ptr<TabStrip> tab_strip_ = nullptr;
  raw_ptr<TabStripScrollContainer> tab_strip_scroll_container_ = nullptr;
  raw_ptr<TabStripComboButton> combo_button_ = nullptr;
  raw_ptr<views::Button> new_tab_button_ = nullptr;
  raw_ptr<TabSearchContainer> tab_search_container_ = nullptr;
  raw_ptr<TabStripControlButton> unfocus_button_ = nullptr;

  // On some platforms for Chrome Refresh, the TabSearchButton should be
  // laid out before the TabStrip. Storing this configuration prevents
  // rechecking the child order on every layout.
  const bool render_tab_search_before_tab_strip_;

  std::unique_ptr<TabSearchPositionMetricsLogger>
      tab_search_position_metrics_logger_;

#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<ash::TabScrubber> tab_scrubber_;
#endif

  std::unique_ptr<views::ActionViewController> action_view_controller_;

  const base::CallbackListSubscription subscription_ =
      ui::TouchUiController::Get()->RegisterCallback(base::BindRepeating(
          &HorizontalTabStripRegionView::UpdateButtonBorders,
          base::Unretained(this)));
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_HORIZONTAL_TAB_STRIP_REGION_VIEW_H_
