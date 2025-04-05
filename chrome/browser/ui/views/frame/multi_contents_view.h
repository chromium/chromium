// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/resize_area_delegate.h"
#include "ui/views/view.h"

class BrowserView;
class ContentsWebView;
class MultiContentsResizeArea;

namespace blink {
class WebMouseEvent;
}  // namespace blink

namespace content {
class WebContents;
}  // namespace content
namespace gfx {
class Canvas;
}  // namespace gfx

// MultiContentsView shows up to two contents web views side by side, and
// manages their layout relative to each other.
class MultiContentsView : public views::View, public views::ResizeAreaDelegate {
  METADATA_HEADER(MultiContentsView, views::View)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMultiContentsViewElementId);

  using WebContentsPressedCallback =
      base::RepeatingCallback<void(content::WebContents*)>;

  struct ViewWidths {
    double start_width = 0;
    double resize_width = 0;
    double end_width = 0;
  };

  MultiContentsView(BrowserView* browser_view,
                    WebContentsPressedCallback inactive_view_pressed_callback);
  MultiContentsView(const MultiContentsView&) = delete;
  MultiContentsView& operator=(const MultiContentsView&) = delete;
  ~MultiContentsView() override;

  // Returns the currently active ContentsWebView.
  ContentsWebView* GetActiveContentsView();

  // Returns the currently inactive ContentsWebView.
  ContentsWebView* GetInactiveContentsView();

  // Returns true if more than one WebContents is displayed.
  bool IsInSplitView();

  // Assigns the given |web_contents| to the ContentsWebView at |index| in
  // contents_views_. |index| must be either 0 or 1 as we currently only support
  // two contents. If |index| is 1 and we are not currently in a split
  // view, displays the split views.
  void SetWebContentsAtIndex(content::WebContents* web_contents, int index);

  // Preserves the active WebContents and hides the second ContentsWebView and
  // resize handle.
  void CloseSplitView();

  // Sets the index of the active contents view within contents_views_.
  void SetActiveIndex(int index);

  // Handles a mouse event prior to it being passed along to the WebContents.
  bool PreHandleMouseEvent(const blink::WebMouseEvent& event);

  // Helper method to execute an arbitrary callback on each visible contents
  // view. Will execute the callback on the active contents view first.
  void ExecuteOnEachVisibleContentsView(
      base::RepeatingCallback<void(ContentsWebView*)> callback);

  // views::ResizeAreaDelegate:
  void OnResize(int resize_amount, bool done_resizing) override;

  // views::View:
  void Layout(PassKey) override;
  void OnPaint(gfx::Canvas* canvas) override;

  ContentsWebView* start_contents_view_for_testing() const {
    return contents_views_[0];
  }

  MultiContentsResizeArea* resize_area_for_testing() const {
    return resize_area_;
  }

  ContentsWebView* end_contents_view_for_testing() const {
    return contents_views_[1];
  }

 private:
  ViewWidths GetViewWidths(gfx::Rect available_space);

  ViewWidths ClampToMinWidth(ViewWidths widths);

  raw_ptr<BrowserView> browser_view_;

  // Holds ContentsWebViews, when not in a split view the second ContentsWebView
  // is not visible.
  std::vector<ContentsWebView*> contents_views_;

  // The handle responsible for resizing the two contents views as relative to
  // each other.
  raw_ptr<MultiContentsResizeArea> resize_area_ = nullptr;

  // The index in contents_views_ of the active contents view.
  int active_index_ = 0;

  // Callback to be executed when the user clicks anywhere within the bounds of
  // the inactive contents view.
  WebContentsPressedCallback inactive_view_pressed_callback_;

  // Current ratio of |contents_views_|'s first ContentsWebView's width /
  // overall contents view width.
  double start_ratio_ = 0.5;

  // Width of `start_contents_.contents_view_` when a resize action began.
  // Nullopt if not currently resizing.
  std::optional<double> initial_start_width_on_resize_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_H_
