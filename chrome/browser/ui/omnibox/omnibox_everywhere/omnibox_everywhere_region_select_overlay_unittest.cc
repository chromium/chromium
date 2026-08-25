// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_region_select_overlay.h"

#include <memory>

#include "base/test/test_future.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace omnibox_everywhere {

class OmniboxEverywhereRegionSelectOverlayTest : public ChromeViewsTestBase {
 public:
  OmniboxEverywhereRegionSelectOverlayTest() = default;
  ~OmniboxEverywhereRegionSelectOverlayTest() override = default;
};

TEST_F(OmniboxEverywhereRegionSelectOverlayTest, CreateAndDismiss) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorRED);

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      bitmap, future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_TRUE(overlay->widget());
  EXPECT_TRUE(overlay->widget()->IsVisible());

  // Close the overlay widget (simulating Escape / dismiss).
  overlay->widget()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest, ConfirmReturnsBitmap) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorBLUE);

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      bitmap, future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_TRUE(overlay->widget());

  // Trigger mouse press to confirm.
  ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(50, 50),
                             gfx::Point(50, 50), base::TimeTicks::Now(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  overlay->widget()->GetContentsView()->OnMousePressed(press_event);

  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get().empty());
  EXPECT_EQ(future.Get().getColor(0, 0), SK_ColorBLUE);
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest, EscapeKeyPressedCancels) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorRED);

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      bitmap, future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_TRUE(overlay->widget());

  // Trigger Escape key via FocusManager accelerator.
  ui::Accelerator escape_accel(ui::VKEY_ESCAPE, ui::EF_NONE);
  EXPECT_TRUE(
      overlay->widget()->GetFocusManager()->ProcessAccelerator(escape_accel));

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       NativeCloseNowDismissesCleanly) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorRED);

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      bitmap, future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);
  ASSERT_TRUE(overlay->widget());

  // Synchronously closing native window directly.
  overlay->widget()->CloseNow();

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(OmniboxEverywhereRegionSelectOverlayTest,
       DestructorDismissesPendingCallback) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorGREEN);

  base::test::TestFuture<const SkBitmap&> future;
  auto overlay = OmniboxEverywhereRegionSelectOverlay::Create(
      bitmap, future.GetCallback(), GetContext());
  ASSERT_TRUE(overlay);

  overlay.reset();

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

}  // namespace omnibox_everywhere
