// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/resize_area_delegate.h"
#include "ui/views/view.h"

class BrowserView;
class ContentsWebView;
class MultiContentsDropTargetView;
class MultiContentsResizeArea;
class MultiContentsViewDelegate;
class MultiContentsViewDropTargetController;
class MultiContentsViewMiniToolbar;

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class RoundedCornersF;
}  // namespace gfx

namespace views {
class WebView;
}  // namespace views

class MultiContentsBackgroundView;

// MultiContentsView shows up to two contents web views side by side, and
// manages their layout relative to each other.
class MultiContentsView : public views::View,
                          public views::ResizeAreaDelegate,
                          public views::LayoutDelegate {
  METADATA_HEADER(MultiContentsView, views::View)

 public:
  struct ViewWidths {
    double start_width = 0;
    double resize_width = 0;
    double end_width = 0;
  };

  static constexpr int kSplitViewContentInset = 8;

  MultiContentsView(BrowserView* browser_view,
                    std::unique_ptr<MultiContentsViewDelegate> delegate);
  MultiContentsView(const MultiContentsView&) = delete;
  MultiContentsView& operator=(const MultiContentsView&) = delete;
  ~MultiContentsView() override;

  ContentsContainerView* GetActiveContentsContainerView() const;
  ContentsContainerView* GetInactiveContentsContainerView() const;
  ContentsContainerView* GetContentsContainerViewFor(
      content::WebContents* web_contents) const;

  // Returns the currently active ContentsWebView.
  ContentsWebView* GetActiveContentsView() const;

  // Returns the currently inactive ContentsWebView.
  ContentsWebView* GetInactiveContentsView() const;

  const gfx::RoundedCornersF& background_radii() const;
  void SetBackgroundRadii(const gfx::RoundedCornersF& radii);

  // Returns true if more than one WebContents is displayed.
  bool IsInSplitView() const;

  // Show the split view without set any WebContents and update the size of
  // contents views based on `ratio`, this is used to prepare the layout and
  // prevent a re-layout of WebContents.
  void ShowSplitView(double ratio);

  // Preserves the active WebContents and hides the second ContentsContainerView
  // and resize handle.
  void CloseSplitView();

  // Assigns the given |web_contents| to the ContentsContainerView's
  // ContentsWebView at |index| in contents_container_views_. |index| must be
  // either 0 or 1 as we currently only support two contents. If |index| is 1
  // and we are not currently in a split view, displays the split views.
  void SetWebContentsAtIndex(content::WebContents* web_contents, int index);

  // Sets the index of the active contents view within contents_views_.
  void SetActiveIndex(int index);
  int GetActiveIndex() const { return active_index_; }

  // Updates the size of the contents views based on |ratio|.
  void UpdateSplitRatio(double ratio);
  double GetSplitRatio() const { return start_ratio_; }

  // Sets whether the active contents view is highlighted.
  void SetHighlightActiveContentsView(bool needs_attention);

  // Helper method to execute an arbitrary callback on each visible contents
  // view. Will execute the callback on the active contents view first.
  void ExecuteOnEachVisibleContentsView(
      base::RepeatingCallback<void(ContentsWebView*)> callback);

  // If in a split view, swaps the order of the two contents views.
  void OnSwap();

  // If the split view is being resized.
  bool IsSplitResizing() const {
    return initial_start_width_on_resize_.has_value();
  }

  // Returns the minimum width for a single view within the `MultiContentsView`.
  // Returns 0 if not in a split view.
  int GetMinViewWidth() const;

  // Returns accessible panes to be used in BrowserView to create the order of
  // pane traversal.
  std::vector<views::View*> GetAccessiblePanes();

  // views::ResizeAreaDelegate:
  void OnResize(int resize_amount, bool done_resizing) override;

  // views::View:
  void OnThemeChanged() override;

  std::vector<ContentsContainerView*> contents_container_views() const {
    return contents_container_views_;
  }

  MultiContentsViewDropTargetController& drop_target_controller() const;

  bool IsDragAndDropEnabled() const;
  void OnDragAndDropPrefStateChange();

  void SetShouldShowTopSeparator(bool should_show);
  void SetShouldShowLeadingSeparator(bool should_show);
  void SetShouldShowTrailingSeparator(bool should_show);

  void set_min_contents_width_for_testing(int width) {
    min_contents_width_for_testing_ = std::make_optional(width);
  }

  ContentsWebView* start_contents_view_for_testing() const {
    return contents_container_views_[0]->contents_view();
  }

  MultiContentsResizeArea* resize_area_for_testing() const {
    return resize_area_;
  }

  ContentsWebView* end_contents_view_for_testing() const {
    return contents_container_views_[1]->contents_view();
  }

  MultiContentsViewMiniToolbar* mini_toolbar_for_testing(int index) const {
    return contents_container_views_[index]->mini_toolbar();
  }

  MultiContentsBackgroundView* background_view_for_testing() const {
    return background_view_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(MultiContentsViewBrowserTest, DropTargetLayout);
  FRIEND_TEST_ALL_PREFIXES(MultiContentsViewBrowserTest, SeparatorLayout);

  // Encapsulates the views required to draw a separator around contents.
  struct ContentsSeparators {
    void Reset();

    raw_ptr<views::View> top_separator = nullptr;
    raw_ptr<views::View> leading_separator = nullptr;
    raw_ptr<views::View> trailing_separator = nullptr;
    raw_ptr<views::View> top_leading_rounded_corner = nullptr;
    raw_ptr<views::View> top_trailing_rounded_corner = nullptr;

    bool should_show_top = false;
    bool should_show_leading = false;
    bool should_show_trailing = false;
  };

  static constexpr int kMinWebContentsWidth = 200;
  static constexpr double kMinWebContentsWidthPercentage = 0.1;

  // LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

  // Adds the drop target layout to the given list and return the remaining
  // available space after the layout.
  gfx::Rect CalculateDropTargetLayout(
      const gfx::Rect& available_space,
      std::vector<views::ChildLayout>& child_layouts) const;

  // Adds separator layouts to the given list and returns the remaining
  // space after the layout.
  gfx::Rect CalculateSeparatorLayouts(
      const gfx::Rect& available_space,
      std::vector<views::ChildLayout>& child_layouts) const;

  int GetInactiveIndex() const;

  void OnWebContentsFocused(views::WebView*);
  void OnNtpFooterFocused(views::WebView*);
  void OnActorOverlayFocused(views::WebView*);

  ViewWidths GetViewWidths(gfx::Rect available_space) const;

  // Clamps to the minimum of kMinWebContentsWidth or
  // kMinWebContentsWidthPercentage multiplied by the window width. This allows
  // for some flexibility when it comes to particularly narrow windows.
  ViewWidths ClampToMinWidth(ViewWidths widths) const;

  void UpdateContentsBorderAndOverlay();

  double CalculateRatioWithSnapPoints(double end_width,
                                      double total_width) const;

  raw_ptr<BrowserView> browser_view_;
  std::unique_ptr<MultiContentsViewDelegate> delegate_;

  raw_ptr<MultiContentsBackgroundView> background_view_;
  ContentsSeparators contents_separators_;

  // Holds ContentsContainerViews, when not in a split view the second
  // ContentsContainerView is not visible.
  std::vector<ContentsContainerView*> contents_container_views_;

  // Holds subscriptions for when the attached web contents to ContentsView
  // is focused.
  std::vector<base::CallbackListSubscription>
      web_contents_focused_subscriptions_;

  // Holds subscriptions for when the attached web contents to NtpFooterView
  // is focused.
  std::vector<base::CallbackListSubscription> ntp_footer_focused_subscriptions_;

  // Holds subscriptions for when the attached web contents to
  // ActorOverlayWebView is focused.
  std::vector<base::CallbackListSubscription>
      actor_overlay_focused_subscriptions_;

  // The handle responsible for resizing the two contents views as relative to
  // each other.
  raw_ptr<MultiContentsResizeArea> resize_area_ = nullptr;

  // The views that are shown for entering split view. E.g., this is shown when
  // the user drags a link to the edge of the contents view.
  raw_ptr<MultiContentsDropTargetView> drop_target_view_ = nullptr;

  // Handles incoming drag events to show/hide the drop target for entering
  // split view.
  std::unique_ptr<MultiContentsViewDropTargetController>
      drop_target_controller_;

  // The index in contents_views_ of the active contents view.
  int active_index_ = 0;

  // Current ratio of |contents_views_|'s first ContentsContainerView's width /
  // overall contents view width.
  double start_ratio_ = 0.5;

  // Width of `start_contents_.contents_view_` when a resize action began.
  // Nullopt if not currently resizing.
  std::optional<double> initial_start_width_on_resize_;

  // Insets of the start and end contents view when in split view
  gfx::Insets start_contents_view_inset_;
  gfx::Insets end_contents_view_inset_;

  bool active_contents_view_highlighted_ = false;

  std::optional<int> min_contents_width_for_testing_ = std::nullopt;

  // Width ratios that a split view will snap to when resize is within a
  // snap distance (kSideBySideSnapDistance).
  std::vector<double> snap_points_ = {0.5};

  // Tracks and handles drag and drop settings change.
  PrefChangeRegistrar pref_change_registrar_;
  bool is_drag_drop_pref_enabled_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_H_
