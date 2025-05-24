// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/resize_area_delegate.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"

class BrowserView;
class ContentsWebView;
class MultiContentsResizeArea;
class MultiContentsViewDropTargetController;
class MultiContentsViewMiniToolbar;

namespace blink {
class WebMouseEvent;
}  // namespace blink

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class Canvas;
}  // namespace gfx

namespace views {
class WebView;
}  // namespace views

// TODO(crbug.com/394369035): The drop target view will eventually have its
// own class. Move this declaration into the class once ready.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kMultiContentsViewDropTargetElementId);

// MultiContentsView shows up to two contents web views side by side, and
// manages their layout relative to each other.
class MultiContentsView : public views::View, public views::ResizeAreaDelegate {
  METADATA_HEADER(MultiContentsView, views::View)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMultiContentsViewElementId);

  using WebContentsFocusedCallback =
      base::RepeatingCallback<void(content::WebContents*)>;

  using WebContentsResizeCallback = base::RepeatingCallback<void(double)>;

  struct ViewWidths {
    double start_width = 0;
    double resize_width = 0;
    double end_width = 0;
  };

  MultiContentsView(
      BrowserView* browser_view,
      WebContentsFocusedCallback inactive_contents_focused_callback,
      WebContentsResizeCallback contents_resize_callback);
  MultiContentsView(const MultiContentsView&) = delete;
  MultiContentsView& operator=(const MultiContentsView&) = delete;
  ~MultiContentsView() override;

  // Returns the currently active ContentsWebView.
  ContentsWebView* GetActiveContentsView();

  // Returns the currently inactive ContentsWebView.
  ContentsWebView* GetInactiveContentsView();

  // Returns true if more than one WebContents is displayed.
  bool IsInSplitView();

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

  // Handles a mouse event prior to it being passed along to the WebContents.
  bool PreHandleMouseEvent(const blink::WebMouseEvent& event);

  // Helper method to execute an arbitrary callback on each visible contents
  // view. Will execute the callback on the active contents view first.
  void ExecuteOnEachVisibleContentsView(
      base::RepeatingCallback<void(ContentsWebView*)> callback);

  // If in a split view, swaps the order of the two contents views.
  void OnSwap();

  void UpdateSplitRatio(double ratio);

  // views::ResizeAreaDelegate:
  void OnResize(int resize_amount, bool done_resizing) override;

  // views::View:
  void Layout(PassKey) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  MultiContentsViewDropTargetController& drop_target_controller() {
    return *drop_target_controller_;
  }

  void SetMinWidthForTesting(int width) {
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

  static int contents_inset_for_testing() { return kSplitViewContentInset; }

 private:
  static constexpr int kMinWebContentsWidth = 200;
  static constexpr double kMinWebContentsWidthPercentage = 0.1;
  static constexpr int kContentCornerRadius = 6;
  static constexpr int kContentOutlineCornerRadius = 8;
  static constexpr int kContentOutlineThickness = 1;
  static constexpr int kSplitViewContentInset = 8;
  static constexpr int kSplitViewContentPadding = 4;

  // ContentsContainerView holds the ContentsWebView and the outlines and
  // minitoolbar when in split view.
  class ContentsContainerView : public views::View,
                                public views::LayoutDelegate {
    METADATA_HEADER(ContentsContainerView, views::View)
   public:
    explicit ContentsContainerView(BrowserView* browser_view);
    ContentsContainerView(ContentsContainerView&) = delete;
    ContentsContainerView& operator=(const ContentsContainerView&) = delete;
    ~ContentsContainerView() override = default;

    ContentsWebView* GetContentsView() { return contents_view_; }
    MultiContentsViewMiniToolbar* GetMiniToolbar() { return mini_toolbar_; }

   private:
    // LayoutDelegate:
    views::ProposedLayout CalculateProposedLayout(
        const views::SizeBounds& size_bounds) const override;

    raw_ptr<ContentsWebView> contents_view_;
    raw_ptr<MultiContentsViewMiniToolbar> mini_toolbar_;
  };

  int GetInactiveIndex();

  void OnWebContentsFocused(views::WebView*);

  ViewWidths GetViewWidths(gfx::Rect available_space);

  // Clamps to the minimum of kMinWebContentsWidth or
  // kMinWebContentsWidthPercentage multiplied by the window width. This allows
  // for some flexibility when it comes to particularly narrow windows.
  ViewWidths ClampToMinWidth(ViewWidths widths);

  void UpdateContentsBorderAndOverlay();

  raw_ptr<BrowserView> browser_view_;

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

  // Handles incoming drag events to show/hide the drop target for entering
  // split view.
  std::unique_ptr<MultiContentsViewDropTargetController>
      drop_target_controller_;

  // The index in contents_views_ of the active contents view.
  int active_index_ = 0;

  // Callback to be executed when the user focuses the inactive contents view.
  WebContentsFocusedCallback inactive_contents_focused_callback_;

  // Callback to be executed when the user resizes the contents.
  WebContentsResizeCallback contents_resize_callback_;

  // Current ratio of |contents_views_|'s first ContentsContainerView's width /
  // overall contents view width.
  double start_ratio_ = 0.5;

  // Width of `start_contents_.contents_view_` when a resize action began.
  // Nullopt if not currently resizing.
  std::optional<double> initial_start_width_on_resize_;

  std::optional<int> min_contents_width_for_testing_ = std::nullopt;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_H_
