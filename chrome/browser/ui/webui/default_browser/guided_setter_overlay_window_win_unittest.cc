// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/guided_setter_overlay_window_win.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

class GuidedSetterOverlayWindowWinTest : public views::ViewsTestBase {
 protected:
  GuidedSetterOverlayWindowWinTest() = default;
  ~GuidedSetterOverlayWindowWinTest() override = default;

  views::Widget* GetWidget(GuidedSetterOverlayWindowWin* overlay) {
    return overlay->widget_for_testing();
  }
};

TEST_F(GuidedSetterOverlayWindowWinTest, Lifecycle) {
  auto overlay = std::make_unique<GuidedSetterOverlayWindowWin>(GetContext());

  // Widget should be created immediately but remain invisible initially.
  EXPECT_TRUE(GetWidget(overlay.get()));
  EXPECT_FALSE(GetWidget(overlay.get())->IsVisible());

  overlay->Hide();
  EXPECT_FALSE(GetWidget(overlay.get())->IsVisible());
}

TEST_F(GuidedSetterOverlayWindowWinTest, UpdateAndShow) {
  auto overlay = std::make_unique<GuidedSetterOverlayWindowWin>(GetContext());

  gfx::Point start(120, 150);
  gfx::Point end(300, 250);

  // Update and show the overlay window.
  overlay->UpdateAndShow(start, end);

  // Widget should be visible.
  views::Widget* widget = GetWidget(overlay.get());
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_FALSE(widget->GetWindowBoundsInScreen().IsEmpty());

  // Hide the overlay window.
  overlay->Hide();
  EXPECT_FALSE(widget->IsVisible());
}
