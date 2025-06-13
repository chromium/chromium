// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"

#include "base/time/time.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

namespace {

constexpr int kDelayedAnimationDuration = 60;

class MockDropDelegate : public MultiContentsDropTargetView::DropDelegate {
 public:
  MOCK_METHOD(void,
              HandleLinkDrop,
              (MultiContentsDropTargetView::DropSide, const std::vector<GURL>&),
              (override));
};

class DropTargetViewTest : public ChromeViewsTestBase {
 protected:
  DropTargetViewTest() : drop_target_view_(drop_delegate_) {
    drop_target_view_.animation_for_testing().SetSlideDuration(
        base::Seconds(0));
  }

  MultiContentsDropTargetView* drop_target_view() { return &drop_target_view_; }

  MockDropDelegate& drop_delegate() { return drop_delegate_; }

 private:
  MockDropDelegate drop_delegate_;
  MultiContentsDropTargetView drop_target_view_;
};

TEST_F(DropTargetViewTest, ViewIsOpened) {
  MultiContentsDropTargetView* view = drop_target_view();

  EXPECT_TRUE(view->animation_for_testing().GetCurrentValue() == 0);

  view->Show(MultiContentsDropTargetView::DropSide::START);

  EXPECT_TRUE(view->GetVisible());
  EXPECT_TRUE(view->icon_view_for_testing()->GetVisible());
}

TEST_F(DropTargetViewTest, ViewIsClosed) {
  MultiContentsDropTargetView* view = drop_target_view();
  view->Show(MultiContentsDropTargetView::DropSide::START);

  EXPECT_TRUE(view->animation_for_testing().GetCurrentValue() == 1);

  view->Hide();

  EXPECT_FALSE(view->GetVisible());
}

TEST_F(DropTargetViewTest, ViewIsClosedAfterDelay) {
  MultiContentsDropTargetView* view = drop_target_view();
  auto now = base::TimeTicks::Now();
  gfx::AnimationTestApi animation(
      &(drop_target_view()->animation_for_testing()));
  auto scoped_mode = animation.SetRichAnimationRenderMode(
      gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED);

  view->animation_for_testing().SetSlideDuration(
      base::Seconds(kDelayedAnimationDuration));

  view->Show(MultiContentsDropTargetView::DropSide::START);

  animation.SetStartTime(now);
  animation.Step(now + base::Seconds(15));

  EXPECT_TRUE(view->animation_for_testing().GetCurrentValue() > 0);
  EXPECT_TRUE(view->animation_for_testing().GetCurrentValue() < 1);
  EXPECT_TRUE(view->GetVisible());

  view->Hide();

  animation.Step(now + base::Seconds(kDelayedAnimationDuration + 1));

  EXPECT_TRUE(view->animation_for_testing().GetCurrentValue() == 0);
  EXPECT_FALSE(view->GetVisible());
}

TEST_F(DropTargetViewTest, ViewIsOpenedAfterDelay) {
  MultiContentsDropTargetView* view = drop_target_view();
  auto now = base::TimeTicks::Now();
  gfx::AnimationTestApi animation(
      &(drop_target_view()->animation_for_testing()));
  auto scoped_mode = animation.SetRichAnimationRenderMode(
      gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED);

  view->Show(MultiContentsDropTargetView::DropSide::START);

  view->animation_for_testing().SetSlideDuration(
      base::Seconds(kDelayedAnimationDuration));

  view->Hide();

  animation.SetStartTime(now);
  animation.Step(now + base::Seconds(15));

  EXPECT_TRUE(view->animation_for_testing().GetCurrentValue() > 0);
  EXPECT_TRUE(view->animation_for_testing().GetCurrentValue() < 1);
  EXPECT_TRUE(view->GetVisible());

  view->Show(MultiContentsDropTargetView::DropSide::START);

  animation.Step(now + base::Seconds(kDelayedAnimationDuration + 1));

  EXPECT_TRUE(view->animation_for_testing().GetCurrentValue() == 1);
  EXPECT_TRUE(view->GetVisible());
}

TEST_F(DropTargetViewTest, CanDropURL) {
  ui::OSExchangeData data;
  data.SetURL(GURL("https://www.google.com"), u"Google");
  EXPECT_TRUE(drop_target_view()->CanDrop(data));
}

TEST_F(DropTargetViewTest, CannotDropNonURL) {
  ui::OSExchangeData data;
  data.SetString(u"Some random string");
  EXPECT_FALSE(drop_target_view()->CanDrop(data));
}

TEST_F(DropTargetViewTest, CannotDropEmptyURL) {
  ui::OSExchangeData data;
  // An OSExchangeData with no URL data will result in an empty URL list.
  EXPECT_FALSE(drop_target_view()->CanDrop(data));
}

TEST_F(DropTargetViewTest, GetDropFormats) {
  int formats = 0;
  std::set<ui::ClipboardFormatType> format_types;
  EXPECT_TRUE(drop_target_view()->GetDropFormats(&formats, &format_types));
  EXPECT_EQ(format_types.count(ui::ClipboardFormatType::UrlType()), 1u);
}

TEST_F(DropTargetViewTest, OnDragUpdated) {
  const ui::DropTargetEvent event(ui::OSExchangeData(), gfx::PointF(),
                                  gfx::PointF(), ui::DragDropTypes::DRAG_LINK);
  EXPECT_EQ(ui::DragDropTypes::DRAG_LINK,
            drop_target_view()->OnDragUpdated(event));
}

TEST_F(DropTargetViewTest, OnDragExitedClosesView) {
  MultiContentsDropTargetView* view = drop_target_view();
  view->Show(MultiContentsDropTargetView::DropSide::START);
  ASSERT_TRUE(view->GetVisible());

  view->OnDragExited();

  // With zero-duration animation, the view should close and hide immediately.
  EXPECT_FALSE(view->GetVisible());
  EXPECT_EQ(view->animation_for_testing().GetCurrentValue(), 0);
}

TEST_F(DropTargetViewTest, OnDragDoneClosesView) {
  MultiContentsDropTargetView* view = drop_target_view();
  view->Show(MultiContentsDropTargetView::DropSide::START);
  ASSERT_TRUE(view->GetVisible());

  view->OnDragDone();

  // The view should close and hide immediately.
  EXPECT_FALSE(view->GetVisible());
  EXPECT_EQ(view->animation_for_testing().GetCurrentValue(), 0);
}

TEST_F(DropTargetViewTest, DropCallbackPerformsDropAndCloses) {
  MultiContentsDropTargetView* view = drop_target_view();
  view->Show(MultiContentsDropTargetView::DropSide::START);
  ASSERT_TRUE(view->GetVisible());

  const GURL url("https://chromium.org");
  ui::OSExchangeData data;
  data.SetURL(url, u"");

  const ui::DropTargetEvent event(data, gfx::PointF(), gfx::PointF(),
                                  ui::DragDropTypes::DRAG_LINK);

  // Expect the delegate to be called with the correct URL.
  EXPECT_CALL(drop_delegate(),
              HandleLinkDrop(MultiContentsDropTargetView::DropSide::START,
                             testing::ElementsAre(url)));

  // Retrieve and run the drop callback.
  views::View::DropCallback callback = view->GetDropCallback(event);
  ui::mojom::DragOperation output_op = ui::mojom::DragOperation::kNone;
  std::unique_ptr<ui::LayerTreeOwner> drag_image;
  std::move(callback).Run(event, output_op, std::move(drag_image));

  // The view should close after the drop operation.
  EXPECT_FALSE(view->GetVisible());
}

}  // namespace
