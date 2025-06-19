// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
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
class Canvas;
}  // namespace gfx

namespace views {
class WebView;
}  // namespace views

// MultiContentsView shows up to two contents web views side by side, and
// manages their layout relative to each other.
class MultiContentsView : public views::View,
                          public views::ResizeAreaDelegate,
                          public views::LayoutDelegate {
  METADATA_HEADER(MultiContentsView, views::View)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMultiContentsViewElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kStartContainerViewScrimElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kEndContainerViewScrimElementId);

  struct ViewWidths {
    double start_width = 0;
    double resize_width = 0;
    double end_width = 0;

    double drop_target_width = 0;
  };

  static constexpr int kSplitViewContentInset = 8;

  MultiContentsView(BrowserView* browser_view,
                    std::unique_ptr<MultiContentsViewDelegate> delegate);
  MultiContentsView(const MultiContentsView&) = delete;
  MultiContentsView& operator=(const MultiContentsView&) = delete;
  ~MultiContentsView() override;

  // Returns the currently active ContentsWebView.
  ContentsWebView* GetActiveContentsView();

  // Returns the currently inactive ContentsWebView.
  ContentsWebView* GetInactiveContentsView();

  // Returns true if more than one WebContents is displayed.
  bool IsInSplitView() const;

  // Assigns the given |web_contents| to the ContentsContainerView's
  // ContentsWebView at |index| in contents_container_views_. |index| must be
  // either 0 or 1 as we currently only support two contents. If |index| is 1
  // and we are not currently in a split view, displays the split views.
  void SetWebContentsAtIndex(content::WebContents* web_contents, int index);

  // Preserves the active WebContents and hides the second ContentsContainerView
  // and resize handle.
  void CloseSplitView();

  // Sets the index of the active contents view within contents_views_.
  void SetActiveIndex(int index);

  // Updates the the size of the contents views based on |ratio|.
  void UpdateSplitRatio(double ratio);

  // Sets whether a scrim should show over the inactive contents view.
  void SetInactiveScrimVisibility(bool show_inactive_scrim);

  // Helper method to execute an arbitrary callback on each visible contents
  // view. Will execute the callback on the active contents view first.
  void ExecuteOnEachVisibleContentsView(
      base::RepeatingCallback<void(ContentsWebView*)> callback);

  // If in a split view, swaps the order of the two contents views.
  void OnSwap();

  // views::ResizeAreaDelegate:
  void OnResize(int resize_amount, bool done_resizing) override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  MultiContentsViewDropTargetController& drop_target_controller() {
    return *drop_target_controller_;
  }

  gfx::Insets& start_contents_view_inset() {
    return start_contents_view_inset_;
  }

  gfx::Insets& end_contents_view_inset() { return end_contents_view_inset_; }

  void set_min_contents_width_for_testing(int width) {
    min_contents_width_for_testing_ = std::make_optional(width);
  }

  ContentsWebView* start_contents_view_for_testing() const {
    return contents_container_views_[0]->GetContentsView();
  }

  MultiContentsResizeArea* resize_area_for_testing() const {
    return resize_area_;
  }

  ContentsWebView* end_contents_view_for_testing() const {
    return contents_container_views_[1]->GetContentsView();
  }

  MultiContentsViewMiniToolbar* mini_toolbar_for_testing(int index) const {
    return contents_container_views_[index]->GetMiniToolbar();
  }

 private:
  static constexpr int kMinWebContentsWidth = 200;
  static constexpr double kMinWebContentsWidthPercentage = 0.1;

  // LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

  int GetInactiveIndex();

  void OnWebContentsFocused(views::WebView*);

  ViewWidths GetViewWidths(gfx::Rect available_space) const;

  // Clamps to the minimum of kMinWebContentsWidth or
  // kMinWebContentsWidthPercentage multiplied by the window width. This allows
  // for some flexibility when it comes to particularly narrow windows.
  ViewWidths ClampToMinWidth(ViewWidths widths) const;

  void UpdateContentsBorderAndOverlay();

  raw_ptr<BrowserView> browser_view_;
  std::unique_ptr<MultiContentsViewDelegate> delegate_;

  // Holds ContentsContainerViews, when not in a split view the second
  // ContentsContainerView is not visible.
  std::vector<ContentsContainerView*> contents_container_views_;

  // Holds subscriptions for when the attached web contents to ContentsView
  // is focused.
  std::vector<base::CallbackListSubscription>
      web_contents_focused_subscriptions_;

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

  bool show_inactive_scrim_ = false;

  std::optional<int> min_contents_width_for_testing_ = std::nullopt;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_H_
