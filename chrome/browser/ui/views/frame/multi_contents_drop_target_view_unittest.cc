// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"

#include <memory>
#include <optional>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr int kDelayedAnimationDuration = 60;

class MockDragDelegate : public MultiContentsDropTargetView::DragDelegate {
 public:
  MOCK_METHOD(bool,
              GetDropFormats,
              (int* formats, std::set<ui::ClipboardFormatType>* format_types),
              (override));
  MOCK_METHOD(bool, CanDrop, (const ui::OSExchangeData& data), (override));
  MOCK_METHOD(void,
              OnDragEntered,
              (const ui::DropTargetEvent& event),
              (override));
  MOCK_METHOD(void, OnDragExited, (), (override));
  MOCK_METHOD(void, OnDragDone, (), (override));
  MOCK_METHOD(int,
              OnDragUpdated,
              (const ui::DropTargetEvent& event),
              (override));
  MOCK_METHOD(views::View::DropCallback,
              GetDropCallback,
              (const ui::DropTargetEvent& event),
              (override));
};

class DropTargetViewTest : public ChromeViewsTestBase,
                           public testing::WithParamInterface<
                               MultiContentsDropTargetView::DropSide> {
 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget_->Show();
    drop_target_view_ = widget_->SetContentsView(
        std::make_unique<MultiContentsDropTargetView>());
    drop_target_view_->SetSize({0, widget_->GetSize().height()});

    drop_target_view_->SetDragDelegate(&drag_delegate_);
    drop_target_view_->animation_for_testing().SetSlideDuration(
        base::Seconds(0));
    normal_duration_.emplace(
        gfx::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  }
  void TearDown() override {
    normal_duration_.reset();
    drop_target_view_ = nullptr;
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  MultiContentsDropTargetView* drop_target_view() {
    return drop_target_view_.get();
  }

  views::Widget* widget() { return widget_.get(); }

  MockDragDelegate& drag_delegate() { return drag_delegate_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  MockDragDelegate drag_delegate_;
  raw_ptr<MultiContentsDropTargetView> drop_target_view_;
  std::optional<gfx::ScopedAnimationDurationScaleMode> normal_duration_;
};

INSTANTIATE_TEST_SUITE_P(
    DropSide,
    DropTargetViewTest,
    testing::Values(MultiContentsDropTargetView::DropSide::START,
                    MultiContentsDropTargetView::DropSide::END,
                    MultiContentsDropTargetView::DropSide::BOTTOM));

TEST_P(DropTargetViewTest, ViewIsOpened) {
  MultiContentsDropTargetView* view = drop_target_view();

  EXPECT_EQ(0, view->animation_for_testing().GetCurrentValue());

  view->Show(GetParam(), MultiContentsDropTargetView::DropTargetState::kFull,
             MultiContentsDropTargetView::DragType::kLink);

  EXPECT_TRUE(view->GetVisible());
  EXPECT_TRUE(view->icon_view_for_testing()->GetVisible());
}

TEST_P(DropTargetViewTest, ViewIsClosed) {
  MultiContentsDropTargetView* view = drop_target_view();
  view->Show(GetParam(), MultiContentsDropTargetView::DropTargetState::kFull,
             MultiContentsDropTargetView::DragType::kLink);

  EXPECT_TRUE(view->animation_for_testing().GetCurrentValue() == 1);

  view->Hide();

  EXPECT_FALSE(view->GetVisible());
}

TEST_P(DropTargetViewTest, ViewIsClosedAfterDelay) {
  MultiContentsDropTargetView* view = drop_target_view();
  auto now = base::TimeTicks::Now();
  gfx::AnimationTestApi animation(
      &(drop_target_view()->animation_for_testing()));
  auto scoped_mode = animation.SetRichAnimationRenderMode(
      gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED);

  view->animation_for_testing().SetSlideDuration(
      base::Seconds(kDelayedAnimationDuration));

  view->Show(GetParam(), MultiContentsDropTargetView::DropTargetState::kFull,
             MultiContentsDropTargetView::DragType::kLink);

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

TEST_P(DropTargetViewTest, ViewIsOpenedAfterDelay) {
  MultiContentsDropTargetView* view = drop_target_view();
  auto now = base::TimeTicks::Now();
  gfx::AnimationTestApi animation(
      &(drop_target_view()->animation_for_testing()));
  auto scoped_mode = animation.SetRichAnimationRenderMode(
      gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED);

  view->Show(GetParam(), MultiContentsDropTargetView::DropTargetState::kFull,
             MultiContentsDropTargetView::DragType::kLink);

  view->animation_for_testing().SetSlideDuration(
      base::Seconds(kDelayedAnimationDuration));

  view->Hide();

  animation.SetStartTime(now);
  animation.Step(now + base::Seconds(15));

  EXPECT_TRUE(view->animation_for_testing().GetCurrentValue() > 0);
  EXPECT_TRUE(view->animation_for_testing().GetCurrentValue() < 1);
  EXPECT_TRUE(view->GetVisible());

  view->Show(GetParam(), MultiContentsDropTargetView::DropTargetState::kFull,
             MultiContentsDropTargetView::DragType::kLink);

  animation.Step(now + base::Seconds(kDelayedAnimationDuration + 1));

  EXPECT_TRUE(view->animation_for_testing().GetCurrentValue() == 1);
  EXPECT_TRUE(view->GetVisible());
}

TEST_P(DropTargetViewTest, ViewDoesNotAnimateWithReducedMotion) {
  MultiContentsDropTargetView* view = drop_target_view();

  // Set a non-zero animation duration to ensure animations would normally run.
  const base::TimeDelta duration = base::Seconds(kDelayedAnimationDuration);
  view->animation_for_testing().SetSlideDuration(duration);

  // Enable reduced motion.
  gfx::Animation::SetPrefersReducedMotionForTesting(true);
  ASSERT_TRUE(gfx::Animation::PrefersReducedMotion());

  gfx::AnimationTestApi animation_api(&view->animation_for_testing());

  // The view should appear immediately without animation.
  view->Show(GetParam(), MultiContentsDropTargetView::DropTargetState::kFull,
             MultiContentsDropTargetView::DragType::kLink);
  EXPECT_EQ(view->animation_for_testing().GetCurrentValue(), 1.0);

  // The view should hide immediately without animation.
  view->Hide();
  EXPECT_FALSE(view->GetVisible());
  EXPECT_EQ(view->animation_for_testing().GetCurrentValue(), 0.0);

  // Reset the setting to not affect other tests.
  gfx::Animation::SetPrefersReducedMotionForTesting(false);
}

TEST_F(DropTargetViewTest, CanDropURL) {
  ON_CALL(drag_delegate(), CanDrop(testing::_))
      .WillByDefault(testing::Return(true));
  ui::OSExchangeData data;
  data.SetURL(GURL("https://www.google.com"), u"Google");
  EXPECT_TRUE(drop_target_view()->CanDrop(data));
}

TEST_F(DropTargetViewTest, CannotDropNonURL) {
  ON_CALL(drag_delegate(), CanDrop(testing::_))
      .WillByDefault(testing::Return(false));
  ui::OSExchangeData data;
  data.SetString(u"Some random string");
  EXPECT_FALSE(drop_target_view()->CanDrop(data));
}

TEST_F(DropTargetViewTest, CannotDropEmptyURL) {
  ON_CALL(drag_delegate(), CanDrop(testing::_))
      .WillByDefault(testing::Return(false));
  ui::OSExchangeData data;
  // An OSExchangeData with no URL data will result in an empty URL list.
  EXPECT_FALSE(drop_target_view()->CanDrop(data));
}

TEST_F(DropTargetViewTest, GetDropFormats) {
  ON_CALL(drag_delegate(), GetDropFormats(testing::_, testing::_))
      .WillByDefault(
          [](int* formats, std::set<ui::ClipboardFormatType>* format_types) {
            *formats = ui::OSExchangeData::URL;
            format_types->insert(ui::ClipboardFormatType::UrlType());
            return true;
          });
  int formats = 0;
  std::set<ui::ClipboardFormatType> format_types;
  EXPECT_TRUE(drop_target_view()->GetDropFormats(&formats, &format_types));
  EXPECT_EQ(format_types.count(ui::ClipboardFormatType::UrlType()), 1u);
}

TEST_F(DropTargetViewTest, OnDragUpdated) {
  ON_CALL(drag_delegate(), OnDragUpdated(testing::_))
      .WillByDefault(testing::Return(ui::DragDropTypes::DRAG_LINK));
  const ui::DropTargetEvent event(ui::OSExchangeData(), gfx::PointF(),
                                  gfx::PointF(), ui::DragDropTypes::DRAG_LINK);
  EXPECT_EQ(ui::DragDropTypes::DRAG_LINK,
            drop_target_view()->OnDragUpdated(event));
}

TEST_F(DropTargetViewTest, OnDragExited) {
  EXPECT_CALL(drag_delegate(), OnDragExited()).Times(1);
  MultiContentsDropTargetView* view = drop_target_view();
  view->OnDragExited();
}

TEST_F(DropTargetViewTest, OnDragDone) {
  EXPECT_CALL(drag_delegate(), OnDragDone()).Times(1);
  MultiContentsDropTargetView* view = drop_target_view();
  view->OnDragDone();
}

TEST_P(DropTargetViewTest, DropCallback) {
  MultiContentsDropTargetView* view = drop_target_view();
  view->Show(GetParam(), MultiContentsDropTargetView::DropTargetState::kFull,
             MultiContentsDropTargetView::DragType::kLink);
  ASSERT_TRUE(view->GetVisible());

  const GURL url("https://chromium.org");
  ui::OSExchangeData data;
  data.SetURL(url, u"");

  const ui::DropTargetEvent event(data, gfx::PointF(), gfx::PointF(),
                                  ui::DragDropTypes::DRAG_LINK);

  // The drop target view should request the callback from the delegate.
  EXPECT_CALL(drag_delegate(), GetDropCallback(testing::_)).Times(1);
  views::View::DropCallback callback = view->GetDropCallback(event);
}

TEST_P(DropTargetViewTest, GetSizeForAvailableSpace) {
  MultiContentsDropTargetView* view = drop_target_view();
  view->Show(GetParam(), MultiContentsDropTargetView::DropTargetState::kFull,
             MultiContentsDropTargetView::DragType::kTab);
  EXPECT_TRUE(view->GetVisible());

  // Size is clamped to the minimum.
  EXPECT_EQ(MultiContentsDropTargetView::kDropTargetMinSize,
            view->GetSizeForAvailableSpace(400));

  // Size is clamped to the maximum.
  EXPECT_EQ(MultiContentsDropTargetView::kDropTargetMaxSize,
            view->GetSizeForAvailableSpace(3000));

  // Size is the target percentage of the web contents size.
  EXPECT_EQ(
      1000 * MultiContentsDropTargetView::kDropTargetTargetSizePercentage / 100,
      view->GetSizeForAvailableSpace(1000));

  // When hidden, size should be 0.
  view->Hide();
  EXPECT_FALSE(view->GetVisible());
  EXPECT_EQ(0, view->GetSizeForAvailableSpace(1000));
}

TEST_P(DropTargetViewTest, GetSizeForAvailableSpaceForLink) {
  MultiContentsDropTargetView* view = drop_target_view();
  view->Show(GetParam(), MultiContentsDropTargetView::DropTargetState::kFull,
             MultiContentsDropTargetView::DragType::kLink);
  EXPECT_TRUE(view->GetVisible());

  // Size is clamped to the minimum.
  EXPECT_EQ(MultiContentsDropTargetView::kDropTargetMinSize,
            view->GetSizeForAvailableSpace(400));

  // Size is clamped to the maximum.
  EXPECT_EQ(MultiContentsDropTargetView::kDropTargetMaxSize,
            view->GetSizeForAvailableSpace(3000));

  // Size is the target percentage of the web contents size.
  EXPECT_EQ(
      1000 *
          MultiContentsDropTargetView::kDropTargetForLinkTargetSizePercentage /
          100,
      view->GetSizeForAvailableSpace(1000));

  // When hidden, size should be 0.
  view->Hide();
  EXPECT_FALSE(view->GetVisible());
  EXPECT_EQ(0, view->GetSizeForAvailableSpace(1000));
}

TEST_P(DropTargetViewTest, GetSizeForAvailableSpaceWithAnimation) {
  MultiContentsDropTargetView* view = drop_target_view();
  auto now = base::TimeTicks::Now();
  gfx::AnimationTestApi animation(
      &(drop_target_view()->animation_for_testing()));
  auto scoped_mode = animation.SetRichAnimationRenderMode(
      gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED);

  view->animation_for_testing().SetSlideDuration(
      base::Seconds(kDelayedAnimationDuration));

  view->Show(GetParam(), MultiContentsDropTargetView::DropTargetState::kFull,
             MultiContentsDropTargetView::DragType::kTab);

  animation.SetStartTime(now);
  animation.Step(now + base::Seconds(15));

  EXPECT_TRUE(view->GetVisible());
  EXPECT_GT(view->animation_for_testing().GetCurrentValue(), 0);
  EXPECT_LT(view->animation_for_testing().GetCurrentValue(), 1);

  // Size should be proportional to the animation progress.
  const int final_size =
      1000 * MultiContentsDropTargetView::kDropTargetTargetSizePercentage / 100;
  int animated_size = view->GetSizeForAvailableSpace(1000);
  EXPECT_GT(animated_size, 0);
  EXPECT_LT(animated_size, final_size);

  // After animation finishes, it should be the final size.
  animation.Step(now + base::Seconds(kDelayedAnimationDuration + 1));
  EXPECT_EQ(view->animation_for_testing().GetCurrentValue(), 1);
  EXPECT_EQ(view->GetSizeForAvailableSpace(1000), final_size);
}

TEST_P(DropTargetViewTest, GetSizeForAvailableSpaceWithStates) {
  MultiContentsDropTargetView* view = drop_target_view();

  // Test full state.
  view->Show(GetParam(), MultiContentsDropTargetView::DropTargetState::kFull,
             MultiContentsDropTargetView::DragType::kLink);
  EXPECT_TRUE(view->GetVisible());
  EXPECT_EQ(MultiContentsDropTargetView::kDropTargetMinSize,
            view->GetSizeForAvailableSpace(400));
  EXPECT_EQ(MultiContentsDropTargetView::kDropTargetMaxSize,
            view->GetSizeForAvailableSpace(3000));
  EXPECT_EQ(
      1000 *
          MultiContentsDropTargetView::kDropTargetForLinkTargetSizePercentage /
          100,
      view->GetSizeForAvailableSpace(1000));

  if (GetParam() == MultiContentsDropTargetView::DropSide::BOTTOM) {
    // Nudge state only shows on the left/right sides.
    return;
  }

  // Test nudge state.
  view->Show(GetParam(), MultiContentsDropTargetView::DropTargetState::kNudge,
             MultiContentsDropTargetView::DragType::kLink);
  EXPECT_TRUE(view->GetVisible());
  EXPECT_EQ(MultiContentsDropTargetView::kNudgeMinSize,
            view->GetSizeForAvailableSpace(800));
  EXPECT_EQ(MultiContentsDropTargetView::kNudgeMaxSize,
            view->GetSizeForAvailableSpace(5000));
  EXPECT_EQ(
      2000 * MultiContentsDropTargetView::kNudgeTargetSizePercentage / 100,
      view->GetSizeForAvailableSpace(2000));

  // Test nudge to full state.
  view->Show(GetParam(),
             MultiContentsDropTargetView::DropTargetState::kNudgeToFull,
             MultiContentsDropTargetView::DragType::kLink);
  EXPECT_TRUE(view->GetVisible());
  EXPECT_EQ(MultiContentsDropTargetView::kNudgeToFullMinSize,
            view->GetSizeForAvailableSpace(400));
  EXPECT_EQ(MultiContentsDropTargetView::kNudgeToFullMaxSize,
            view->GetSizeForAvailableSpace(3000));
  EXPECT_EQ(1000 *
                MultiContentsDropTargetView::kNudgeToFullTargetSizePercentage /
                100,
            view->GetSizeForAvailableSpace(1000));
}

TEST_P(DropTargetViewTest, AnimateFromNudgeToFull) {
  if (GetParam() == MultiContentsDropTargetView::DropSide::BOTTOM) {
    // Nudge state only shows on the left/right sides.
    GTEST_SKIP();
  }

  // Chosen so that the nudge states sizes are not clamped to their min/max
  // sizes. The view will calculate sizes relative to this.
  constexpr int kContentsSize = 1800;

  MultiContentsDropTargetView* view = drop_target_view();
  auto now = base::TimeTicks::Now();
  gfx::AnimationTestApi animation(
      &(drop_target_view()->animation_for_testing()));
  view->animation_for_testing().SetSlideDuration(
      base::Seconds(kDelayedAnimationDuration));
  auto scoped_mode = animation.SetRichAnimationRenderMode(
      gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED);

  // Start in nudge state.
  view->Show(GetParam(), MultiContentsDropTargetView::DropTargetState::kNudge,
             MultiContentsDropTargetView::DragType::kLink);
  EXPECT_TRUE(view->GetVisible());

  // Finish the animation and check the size.
  animation.SetStartTime(now);
  animation.Step(now + base::Seconds(kDelayedAnimationDuration));
  const int nudge_size = view->GetSizeForAvailableSpace(kContentsSize);
  view->SetSize(GetParam() == MultiContentsDropTargetView::DropSide::BOTTOM
                    ? gfx::Size(view->width(), nudge_size)
                    : gfx::Size(nudge_size, view->height()));
  EXPECT_EQ(kContentsSize *
                MultiContentsDropTargetView::kNudgeTargetSizePercentage / 100,
            nudge_size);

  // Transition to nudge-to-full state with an animation.
  view->Show(GetParam(),
             MultiContentsDropTargetView::DropTargetState::kNudgeToFull,
             MultiContentsDropTargetView::DragType::kLink);
  EXPECT_EQ(kContentsSize *
                MultiContentsDropTargetView::kNudgeTargetSizePercentage / 100,
            view->GetSizeForAvailableSpace(kContentsSize));

  // Step the animation to the middle.
  animation.Step(now + base::Seconds(kDelayedAnimationDuration / 2));

  // Check that the size is between the nudge and nudge-to-full sizes.
  // At half of the animation, we expect a size to be scaled to the target
  // size.
  const int nudge_to_full_size =
      kContentsSize *
      MultiContentsDropTargetView::kNudgeToFullTargetSizePercentage / 100;
  const int current_size = view->GetSizeForAvailableSpace(kContentsSize);
  EXPECT_GT(current_size, nudge_size);
  EXPECT_LT(current_size, nudge_to_full_size);

  // Finish the animation.
  animation.Step(now + base::Seconds(kDelayedAnimationDuration));
  EXPECT_EQ(view->GetSizeForAvailableSpace(kContentsSize), nudge_to_full_size);
}

TEST_P(DropTargetViewTest, AnimateFromNudgeToFullMidAnimation) {
  if (GetParam() == MultiContentsDropTargetView::DropSide::BOTTOM) {
    // Nudge state only shows on the left/right sides.
    GTEST_SKIP();
  }

  // Chosen so that the nudge states sizes are not clamped to their min/max
  // sizes. The view will calculate sizes relative to this.
  constexpr int kContentsSize = 1800;

  MultiContentsDropTargetView* view = drop_target_view();
  auto now = base::TimeTicks::Now();
  gfx::AnimationTestApi animation(
      &(drop_target_view()->animation_for_testing()));
  view->animation_for_testing().SetSlideDuration(
      base::Seconds(kDelayedAnimationDuration));
  auto scoped_mode = animation.SetRichAnimationRenderMode(
      gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED);

  // Start in nudge state.
  view->Show(GetParam(), MultiContentsDropTargetView::DropTargetState::kNudge,
             MultiContentsDropTargetView::DragType::kLink);
  EXPECT_TRUE(view->GetVisible());

  // Step the animation to the middle.
  animation.SetStartTime(now);
  animation.Step(now + base::Seconds(kDelayedAnimationDuration / 2));

  const int nudge_size =
      kContentsSize * MultiContentsDropTargetView::kNudgeTargetSizePercentage /
      100;
  const int nudge_mid_animation_size =
      view->GetSizeForAvailableSpace(kContentsSize);
  EXPECT_GT(nudge_mid_animation_size, 0);
  EXPECT_LT(nudge_mid_animation_size, nudge_size);

  // Transition to nudge-to-full state with an animation.
  view->Show(GetParam(),
             MultiContentsDropTargetView::DropTargetState::kNudgeToFull,
             MultiContentsDropTargetView::DragType::kLink);

  // Step the animation by 1ms. The size should be larger than where it was
  // when the nudge-animation was interrupted.
  animation.Step(now + base::Seconds(kDelayedAnimationDuration / 2) +
                 base::Milliseconds(1));
  // Check that the size is between the nudge and nudge-to-full sizes.
  const int nudge_to_full_size =
      kContentsSize *
      MultiContentsDropTargetView::kNudgeToFullTargetSizePercentage / 100;
  const int full_mid_animation_size =
      view->GetSizeForAvailableSpace(kContentsSize);
  EXPECT_GT(full_mid_animation_size, nudge_mid_animation_size);
  EXPECT_LT(full_mid_animation_size, nudge_to_full_size);

  // Finish the animation.
  animation.Step(now + base::Seconds(kDelayedAnimationDuration * 2));
  EXPECT_EQ(view->GetSizeForAvailableSpace(kContentsSize), nudge_to_full_size);
}

// Tests that the icon and label are positioned correctly when dragging a tab.
TEST_P(DropTargetViewTest, TabDrag_PositionsIconAndLabelCorrectly) {
  widget()->SetSize(gfx::Size(500, 500));
  drop_target_view()->Show(GetParam(),
                           MultiContentsDropTargetView::DropTargetState::kFull,
                           MultiContentsDropTargetView::DragType::kTab);
  widget()->LayoutRootViewIfNecessary();

  // When dragging a tab, the icon and label should be centered horizontally and
  // aligned to the top, unless it is to the bottom drop target in which case it
  // should be centered vertically as well.
  views::ImageView* icon = drop_target_view()->icon_view_for_testing();
  views::Label* label = drop_target_view()->label_for_testing();
  EXPECT_TRUE(drop_target_view()->GetVisible());
  EXPECT_TRUE(icon->GetVisible());
  EXPECT_TRUE(label->GetVisible());

  constexpr int kErrorMargin = 1;
  EXPECT_NEAR(icon->GetBoundsInScreen().CenterPoint().x(),
              drop_target_view()->GetBoundsInScreen().CenterPoint().x(),
              kErrorMargin);
  EXPECT_EQ(icon->GetBoundsInScreen().bottom(), label->GetBoundsInScreen().y());
  EXPECT_NEAR(label->GetBoundsInScreen().CenterPoint().x(),
              drop_target_view()->GetBoundsInScreen().CenterPoint().x(),
              kErrorMargin);
  if (GetParam() == MultiContentsDropTargetView::DropSide::BOTTOM) {
    EXPECT_LT(icon->GetBoundsInScreen().y(),
              drop_target_view()->GetBoundsInScreen().CenterPoint().y());
    EXPECT_GT(label->GetBoundsInScreen().bottom(),
              drop_target_view()->GetBoundsInScreen().CenterPoint().y());
  } else {
    EXPECT_LT(label->GetBoundsInScreen().bottom(),
              drop_target_view()->GetBoundsInScreen().CenterPoint().y());
  }
}

// Tests that the icon and label are positioned correctly when dragging a link.
TEST_P(DropTargetViewTest, LinkDrag_PositionsIconAndLabelCorrectly) {
  widget()->SetSize(gfx::Size(500, 500));
  drop_target_view()->Show(GetParam(),
                           MultiContentsDropTargetView::DropTargetState::kFull,
                           MultiContentsDropTargetView::DragType::kLink);
  widget()->LayoutRootViewIfNecessary();

  // When dragging a link, the icon and label should be centered both
  // horizontally and vertically.
  views::ImageView* icon = drop_target_view()->icon_view_for_testing();
  views::Label* label = drop_target_view()->label_for_testing();
  EXPECT_TRUE(drop_target_view()->GetVisible());
  EXPECT_TRUE(icon->GetVisible());
  EXPECT_TRUE(label->GetVisible());

  constexpr int kErrorMargin = 1;
  EXPECT_NEAR(icon->GetBoundsInScreen().CenterPoint().x(),
              drop_target_view()->GetBoundsInScreen().CenterPoint().x(),
              kErrorMargin);
  EXPECT_EQ(icon->GetBoundsInScreen().bottom(), label->GetBoundsInScreen().y());
  EXPECT_LT(icon->GetBoundsInScreen().y(),
            drop_target_view()->GetBoundsInScreen().CenterPoint().y());
  EXPECT_NEAR(label->GetBoundsInScreen().CenterPoint().x(),
              drop_target_view()->GetBoundsInScreen().CenterPoint().x(),
              kErrorMargin);
  EXPECT_GT(label->GetBoundsInScreen().bottom(),
            drop_target_view()->GetBoundsInScreen().CenterPoint().y());
}

// Tests that the icon's y-position stays constant between the nudge and full
// states.
TEST_P(DropTargetViewTest, IconVerticalAlignmentWhenLabelHidden) {
  if (GetParam() == MultiContentsDropTargetView::DropSide::BOTTOM) {
    GTEST_SKIP();
  }

  widget()->SetSize(
      {drop_target_view()->icon_view_for_testing()->GetPreferredSize().width() +
           1,
       500});
  drop_target_view()->Show(GetParam(),
                           MultiContentsDropTargetView::DropTargetState::kNudge,
                           MultiContentsDropTargetView::DragType::kLink);
  widget()->LayoutRootViewIfNecessary();
  views::ImageView* icon = drop_target_view()->icon_view_for_testing();
  const int icon_y_for_nudge = icon->y();

  drop_target_view()->Show(GetParam(),
                           MultiContentsDropTargetView::DropTargetState::kFull,
                           MultiContentsDropTargetView::DragType::kLink);
  widget()->LayoutRootViewIfNecessary();

  EXPECT_EQ(0, drop_target_view()->label_for_testing()->width());
  EXPECT_EQ(icon->y(), icon_y_for_nudge);
}

}  // namespace
