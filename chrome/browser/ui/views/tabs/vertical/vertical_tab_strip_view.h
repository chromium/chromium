// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"

class TabCollectionNode;
class TabStripModel;
class VerticalPinnedTabContainerView;
class VerticalUnpinnedTabContainerView;

namespace views {
class ScrollView;
class Separator;
class View;
class ViewTracker;
}  // namespace views

// Container that holds the pinned and unpinned tabs in the vertical tab strip.
class VerticalTabStripView final : public views::View,
                                   public views::LayoutDelegate,
                                   public TabStripModelObserver {
  METADATA_HEADER(VerticalTabStripView, views::View)

 public:
  explicit VerticalTabStripView(TabCollectionNode* collection_node);
  VerticalTabStripView(const VerticalTabStripView&) = delete;
  VerticalTabStripView& operator=(const VerticalTabStripView&) = delete;
  ~VerticalTabStripView() override;

  views::Separator* tabs_separator_for_testing() { return tabs_separator_; }
  VerticalPinnedTabContainerView* GetPinnedTabsContainer();
  VerticalUnpinnedTabContainerView* GetUnpinnedTabsContainer();

  void SetCollapsedState(bool is_collapsed);

  bool IsPositionInWindowCaption(const gfx::Point& point);

  void InitializeTabStrip(TabStripModel& tab_strip_model);

  // LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

  // views::View:
  gfx::Size GetMinimumSize() const override;
  void OnMouseEntered(const ui::MouseEvent& event) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  views::View* AddScrollViewContents(std::unique_ptr<views::View> view);
  void RemoveScrollViewContents(views::View* view);
  void ResetCollectionNode();

  // Called when the compositor has successfully presented the next frame
  // after an activation of `tracked_view` in `scroll_view`.
  void DidPresentFramePostActivation(
      views::ScrollView* scroll_view,
      std::unique_ptr<views::ViewTracker> tracked_view);

  raw_ptr<TabCollectionNode> collection_node_;
  raw_ptr<views::ScrollView> pinned_tabs_scroll_view_ = nullptr;
  raw_ptr<VerticalPinnedTabContainerView> pinned_tabs_container_view_ = nullptr;
  raw_ptr<views::Separator> tabs_separator_ = nullptr;
  raw_ptr<views::ScrollView> unpinned_tabs_scroll_view_ = nullptr;
  raw_ptr<VerticalUnpinnedTabContainerView> unpinned_tabs_container_view_ =
      nullptr;
  bool is_collapsed_ = false;
  base::CallbackListSubscription node_destroyed_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_VIEW_H_
