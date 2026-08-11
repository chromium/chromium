// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_reopen_suppressor.h"

#include <memory>

#include "ui/views/mouse_constants.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

using WebUIBubbleReopenSuppressorTest = views::ViewsTestBase;

TEST_F(WebUIBubbleReopenSuppressorTest, ShowsAndCloses) {
  WebUIBubbleReopenSuppressor suppressor;
  // Use a long threshold to prevent flakiness on slow trybots.
  suppressor.SetSuppressionThresholdForTesting(base::Days(1));

  auto should_suppress = [&]() {
    suppressor.OnMousePressed();
    return suppressor.ShouldSuppressBubbleShow(/*is_pointer_interaction=*/true);
  };

  EXPECT_FALSE(suppressor.IsShowing());
  EXPECT_FALSE(should_suppress());

  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  suppressor.Observe(widget.get());
  EXPECT_TRUE(suppressor.IsShowing());
  EXPECT_TRUE(should_suppress());

  widget->CloseNow();
  EXPECT_FALSE(suppressor.IsShowing());
  // The threshold check should catch that it closed just now.
  EXPECT_TRUE(should_suppress());
}

TEST_F(WebUIBubbleReopenSuppressorTest, ThresholdBypass) {
  WebUIBubbleReopenSuppressor suppressor;
  suppressor.SetSuppressionThresholdForTesting(base::TimeDelta());

  auto should_suppress = [&]() {
    suppressor.OnMousePressed();
    return suppressor.ShouldSuppressBubbleShow(/*is_mouse_interaction=*/true);
  };

  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  suppressor.Observe(widget.get());

  widget->CloseNow();
  EXPECT_FALSE(suppressor.IsShowing());
  // Because threshold is zero, it should no longer suppress.
  EXPECT_FALSE(should_suppress());
}
