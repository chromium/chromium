// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONTEXT_HIGHLIGHT_CONTEXT_HIGHLIGHT_OVERLAY_VIEW_H_
#define CHROME_BROWSER_UI_CONTEXT_HIGHLIGHT_CONTEXT_HIGHLIGHT_OVERLAY_VIEW_H_

#include "cc/trees/tracked_element_bounds.h"
#include "ui/views/view.h"

// ContextHighlightOverlayView is a view that draws AI-generated highlights over
// web content. It is intended to be a child of the BrowserView, sized to cover
// the web content area.
class ContextHighlightOverlayView : public views::View {
  METADATA_HEADER(ContextHighlightOverlayView, views::View)
 public:
  ContextHighlightOverlayView();
  ~ContextHighlightOverlayView() override;

  ContextHighlightOverlayView(const ContextHighlightOverlayView&) = delete;
  ContextHighlightOverlayView& operator=(const ContextHighlightOverlayView&) =
      delete;

  // Updates the highlight bounds based on the latest RenderFrameMetadata.
  void UpdateHighlightBounds(const cc::TrackedElementBounds& bounds,
                             float device_scale_factor);

 protected:
  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  friend class ContextHighlightOverlayViewTest;

  // The highlight rectangles to be drawn, in the view's coordinate system.
  std::vector<gfx::Rect> highlight_rects_;
};

#endif  // CHROME_BROWSER_UI_CONTEXT_HIGHLIGHT_CONTEXT_HIGHLIGHT_OVERLAY_VIEW_H_
