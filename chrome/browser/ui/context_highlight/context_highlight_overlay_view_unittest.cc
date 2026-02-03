// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/context_highlight/context_highlight_overlay_view.h"

#include "cc/paint/paint_flags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/views/test/views_test_base.h"

class ContextHighlightOverlayViewTest : public views::ViewsTestBase {
 public:
  ContextHighlightOverlayViewTest() = default;
  ~ContextHighlightOverlayViewTest() override = default;

 protected:
  std::vector<gfx::Rect> GetHighlightRects(ContextHighlightOverlayView* view) {
    return view->highlight_rects_;
  }
};

TEST_F(ContextHighlightOverlayViewTest, Initialization) {
  auto view = std::make_unique<ContextHighlightOverlayView>();

  // Verify it doesn't process events.
  EXPECT_FALSE(view->GetCanProcessEventsWithinSubtree());

  // Verify it has a layer and is transparent.
  ASSERT_TRUE(view->layer());
  EXPECT_FALSE(view->layer()->fills_bounds_opaquely());
}

TEST_F(ContextHighlightOverlayViewTest, UpdateHighlightBounds) {
  auto view = std::make_unique<ContextHighlightOverlayView>();

  cc::TrackedElementBounds bounds;
  base::Token id(1, 2);
  gfx::Rect rect(10, 20, 100, 200);
  bounds[id] = {rect};

  // Test with scale factor 1.0.
  view->UpdateHighlightBounds(bounds, 1.0f);
  auto rects = GetHighlightRects(view.get());
  ASSERT_EQ(rects.size(), 1u);
  EXPECT_EQ(rects[0], rect);

  // Test with scale factor 2.0.
  view->UpdateHighlightBounds(bounds, 2.0f);
  rects = GetHighlightRects(view.get());
  ASSERT_EQ(rects.size(), 1u);
  EXPECT_EQ(rects[0], gfx::Rect(5, 10, 50, 100));
}
