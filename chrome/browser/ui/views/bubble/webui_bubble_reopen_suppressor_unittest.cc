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
  EXPECT_FALSE(suppressor.IsShowing());
  EXPECT_FALSE(suppressor.ShouldSuppress());

  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  suppressor.Observe(widget.get());
  EXPECT_TRUE(suppressor.IsShowing());
  EXPECT_TRUE(suppressor.ShouldSuppress());

  widget->CloseNow();
  EXPECT_FALSE(suppressor.IsShowing());
  // The threshold check should catch that it closed just now.
  EXPECT_TRUE(suppressor.ShouldSuppress());
}

TEST_F(WebUIBubbleReopenSuppressorTest, ThresholdBypass) {
  WebUIBubbleReopenSuppressor suppressor;
  suppressor.SetSuppressionThresholdForTesting(base::TimeDelta());

  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  suppressor.Observe(widget.get());

  widget->CloseNow();
  EXPECT_FALSE(suppressor.IsShowing());
  // Because threshold is zero, it should no longer suppress.
  EXPECT_FALSE(suppressor.ShouldSuppress());
}
