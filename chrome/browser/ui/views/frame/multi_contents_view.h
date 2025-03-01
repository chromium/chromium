// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/resize_area_delegate.h"
#include "ui/views/view.h"

class ContentsWebView;
class MultiContentsResizeArea;

namespace blink {
class WebMouseEvent;
}  // namespace blink

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

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

  MultiContentsView(content::BrowserContext* browser_context,
                    WebContentsPressedCallback inactive_view_pressed_callback);
  MultiContentsView(const MultiContentsView&) = delete;
  MultiContentsView& operator=(const MultiContentsView&) = delete;
  ~MultiContentsView() override;

  // Returns the currently active ContentsWebView.
  ContentsWebView* GetActiveContentsView();

  // Returns the currently inactive ContentsWebView.
  ContentsWebView* GetInactiveContentsView();

  // Assigns the given |web_contents| to a ContentsWebView. If |active| it will
  // be assigned to the active contents view, else it will be assigned to
  // the inactive contents view.
  void SetWebContents(content::WebContents* web_contents, bool active);

  // Sets the index of the active contents view, as relative to the inactive
  // contents view. A value of 0 will activate start_contents_view_.
  void SetActivePosition(int position);

  // Handles a mouse event prior to it being passed along to the WebContents.
  bool PreHandleMouseEvent(const blink::WebMouseEvent& event);

  // views::ResizeAreaDelegate:
  void OnResize(int resize_amount, bool done_resizing) override;

  // views::View:
  void Layout(PassKey) override;

  ContentsWebView* start_contents_view_for_testing() const {
    return start_contents_view_;
  }

  MultiContentsResizeArea* resize_area_for_testing() const {
    return resize_area_;
  }

  ContentsWebView* end_contents_view_for_testing() const {
    return end_contents_view_;
  }

 private:
  ViewWidths GetViewWidths(gfx::Rect available_space);

  ViewWidths ClampToMinWidth(ViewWidths widths);

  // The left contents view, in LTR.
  raw_ptr<ContentsWebView> start_contents_view_ = nullptr;

  // The right contents view, in LTR.
  raw_ptr<ContentsWebView> end_contents_view_ = nullptr;

  // The handle responsible for resizing the two contents views as relative to
  // each other.
  raw_ptr<MultiContentsResizeArea> resize_area_ = nullptr;

  // The index of the active context view. A value of 0 corresponds to
  // start_contents_view_.
  int active_position_ = 0;

  // Callback to be executed when the user clicks anywhere within the bounds of
  // the inactive contents view.
  WebContentsPressedCallback inactive_view_pressed_callback_;

  // Current ratio of `start_contents_view_` width / overall contents view
  // width.
  double start_ratio_ = 0.5;

  // Width of `start_contents_view_` when a resize action began. Nullopt if not
  // currently resizing.
  std::optional<double> initial_start_width_on_resize_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_VIEW_H_
