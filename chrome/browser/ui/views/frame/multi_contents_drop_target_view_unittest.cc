// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"

#include <memory>

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

class DropTargetViewTest : public ChromeViewsTestBase {
 protected:
  DropTargetViewTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kSideBySide, {}},
         {features::kSideBySideDropTargetNudge, {}}},
        {});
  }

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
  }
  void TearDown() override {
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
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<views::Widget> widget_;
  MockDragDelegate drag_delegate_;
  raw_ptr<MultiContentsDropTargetView> drop_target_view_;
};

TEST_F(DropTargetViewTest, ViewIsOpened) {
  MultiContentsDropTargetView* view = drop_target_view();

  EXPECT_EQ(0, view->animation_for_testing().GetCurrentValue());

  view->Show(MultiContentsDropTargetView::DropSide::START,
             MultiContentsDropTargetView::DropTargetState::kFull,
             MultiContentsDropTargetView::DragType::kLink);

  EXPECT_TRUE(view->GetVisible());
  EXPECT_TRUE(view->icon_view_for_testing()->GetVisible());
}

TEST_F(DropTargetViewTest, ViewIsClosed) {
  MultiContentsDropTargetView* view = drop_target_view();
  view->Show(MultiContentsDropTargetView::DropSide::START,
             MultiContentsDropTargetView::DropTargetState::kFull,
             MultiContentsDropTargetView::DragType::kLink);

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

  view->Show(MultiContentsDropTargetView::DropSide::START,
             MultiContentsDropTargetView::DropTargetState::kFull,
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

TEST_F(DropTargetViewTest, ViewIsOpenedAfterDelay) {
  MultiContentsDropTargetView* view = drop_target_view();
  auto now = base::TimeTicks::Now();
  gfx::AnimationTestApi animation(
      &(drop_target_view()->animation_for_testing()));
  auto scoped_mode = animation.SetRichAnimationRenderMode(
      gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED);

  view->Show(MultiContentsDropTargetView::DropSide::START,
             MultiContentsDropTargetView::DropTargetState::kFull,
             MultiContentsDropTargetView::DragType::kLink);

  view->animation_for_testing().SetSlideDuration(
      base::Seconds(kDelayedAnimationDuration));

  view->Hide();

  animation.SetStartTime(now);
  animation.Step(now + base::Seconds(15));

  EXPECT_TRUE(view->animation_for_testing().GetCurrentValue() > 0);
  EXPECT_TRUE(view->animation_for_testing().GetCurrentValue() < 1);
  EXPECT_TRUE(view->GetVisible());

  view->Show(MultiContentsDropTargetView::DropSide::START,
             MultiContentsDropTargetView::DropTargetState::kFull,
             MultiContentsDropTargetView::DragType::kLink);

  animation.Step(now + base::Seconds(kDelayedAnimationDuration + 1));

  EXPECT_TRUE(view->animation_for_testing().GetCurrentValue() == 1);
  EXPECT_TRUE(view->GetVisible());
}

TEST_F(DropTargetViewTest, ViewDoesNotAnimateWithReducedMotion) {
  MultiContentsDropTargetView* view = drop_target_view();

  // Set a non-zero animation duration to ensure animations would normally run.
  const base::TimeDelta duration = base::Seconds(kDelayedAnimationDuration);
  view->animation_for_testing().SetSlideDuration(duration);

  // Enable reduced motion.
  gfx::Animation::SetPrefersReducedMotionForTesting(true);
  ASSERT_TRUE(gfx::Animation::PrefersReducedMotion());

  gfx::AnimationTestApi animation_api(&view->animation_for_testing());

  // The view should appear immediately without animation.
  view->Show(MultiContentsDropTargetView::DropSide::START,
             MultiContentsDropTargetView::DropTargetState::kFull,
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

TEST_F(DropTargetViewTest, DropCallback) {
  MultiContentsDropTargetView* view = drop_target_view();
  view->Show(MultiContentsDropTargetView::DropSide::START,
             MultiContentsDropTargetView::DropTargetState::kFull,
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

TEST_F(DropTargetViewTest, GetPreferredWidth) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kSideBySide,
      {{features::kSideBySideDropTargetMinWidth.name, "100"},
       {features::kSideBySideDropTargetMaxWidth.name, "400"},
       {features::kSideBySideDropTargetTargetWidthPercentage.name, "20"}});

  MultiContentsDropTargetView* view = drop_target_view();
  view->Show(MultiContentsDropTargetView::DropSide::START,
             MultiContentsDropTargetView::DropTargetState::kFull,
             MultiContentsDropTargetView::DragType::kTab);
  EXPECT_TRUE(view->GetVisible());

  // Width is clamped to the minimum.
  EXPECT_EQ(100, view->GetPreferredWidth(400));

  // Width is clamped to the maximum.
  EXPECT_EQ(400, view->GetPreferredWidth(3000));

  // Width is 20% of the web contents width.
  EXPECT_EQ(200, view->GetPreferredWidth(1000));

  // When hidden, width should be 0.
  view->Hide();
  EXPECT_FALSE(view->GetVisible());
  EXPECT_EQ(0, view->GetPreferredWidth(1000));
}

TEST_F(DropTargetViewTest, GetPreferredWidthForLink) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kSideBySide,
      {{features::kSideBySideDropTargetMinWidth.name, "100"},
       {features::kSideBySideDropTargetMaxWidth.name, "400"},
       {features::kSideBySideDropTargetForLinkTargetWidthPercentage.name,
        "20"}});

  MultiContentsDropTargetView* view = drop_target_view();
  view->Show(MultiContentsDropTargetView::DropSide::START,
             MultiContentsDropTargetView::DropTargetState::kFull,
             MultiContentsDropTargetView::DragType::kLink);
  EXPECT_TRUE(view->GetVisible());

  // Width is clamped to the minimum.
  EXPECT_EQ(100, view->GetPreferredWidth(400));

  // Width is clamped to the maximum.
  EXPECT_EQ(400, view->GetPreferredWidth(3000));

  // Width is 20% of the web contents width.
  EXPECT_EQ(200, view->GetPreferredWidth(1000));

  // When hidden, width should be 0.
  view->Hide();
  EXPECT_FALSE(view->GetVisible());
  EXPECT_EQ(0, view->GetPreferredWidth(1000));
}

TEST_F(DropTargetViewTest, GetPreferredWidthWithAnimation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kSideBySide,
      {{features::kSideBySideDropTargetMinWidth.name, "100"},
       {features::kSideBySideDropTargetMaxWidth.name, "400"},
       {features::kSideBySideDropTargetTargetWidthPercentage.name, "20"}});

  MultiContentsDropTargetView* view = drop_target_view();
  auto now = base::TimeTicks::Now();
  gfx::AnimationTestApi animation(
      &(drop_target_view()->animation_for_testing()));
  auto scoped_mode = animation.SetRichAnimationRenderMode(
      gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED);

  view->animation_for_testing().SetSlideDuration(
      base::Seconds(kDelayedAnimationDuration));

  view->Show(MultiContentsDropTargetView::DropSide::START,
             MultiContentsDropTargetView::DropTargetState::kFull,
             MultiContentsDropTargetView::DragType::kTab);

  animation.SetStartTime(now);
  animation.Step(now + base::Seconds(15));

  EXPECT_TRUE(view->GetVisible());
  EXPECT_GT(view->animation_for_testing().GetCurrentValue(), 0);
  EXPECT_LT(view->animation_for_testing().GetCurrentValue(), 1);

  // Width should be proportional to the animation progress.
  const int final_width = 200;
  int animated_width = view->GetPreferredWidth(1000);
  EXPECT_GT(animated_width, 0);
  EXPECT_LT(animated_width, final_width);

  // After animation finishes, it should be the final width.
  animation.Step(now + base::Seconds(kDelayedAnimationDuration + 1));
  EXPECT_EQ(view->animation_for_testing().GetCurrentValue(), 1);
  EXPECT_EQ(view->GetPreferredWidth(1000), final_width);
}

TEST_F(DropTargetViewTest, GetPreferredWidthWithStates) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kSideBySide,
        {{features::kSideBySideDropTargetMinWidth.name, "100"},
         {features::kSideBySideDropTargetMaxWidth.name, "400"},
         {features::kSideBySideDropTargetForLinkTargetWidthPercentage.name,
          "20"}}},
       {features::kSideBySideDropTargetNudge,
        {{features::kSideBySideDropTargetNudgeMinWidth.name, "50"},
         {features::kSideBySideDropTargetNudgeMaxWidth.name, "100"},
         {features::kSideBySideDropTargetNudgeTargetWidthPercentage.name, "5"},
         {features::kSideBySideDropTargetNudgeToFullMinWidth.name, "80"},
         {features::kSideBySideDropTargetNudgeToFullMaxWidth.name, "200"},
         {features::kSideBySideDropTargetNudgeToFullTargetWidthPercentage.name,
          "10"}}}},
      {});

  MultiContentsDropTargetView* view = drop_target_view();

  // Test nudge state.
  view->Show(MultiContentsDropTargetView::DropSide::START,
             MultiContentsDropTargetView::DropTargetState::kNudge,
             MultiContentsDropTargetView::DragType::kLink);
  EXPECT_TRUE(view->GetVisible());
  EXPECT_EQ(50, view->GetPreferredWidth(800));
  EXPECT_EQ(100, view->GetPreferredWidth(3000));
  EXPECT_EQ(60, view->GetPreferredWidth(1200));

  // Test nudge to full state.
  view->Show(MultiContentsDropTargetView::DropSide::START,
             MultiContentsDropTargetView::DropTargetState::kNudgeToFull,
             MultiContentsDropTargetView::DragType::kLink);
  EXPECT_TRUE(view->GetVisible());
  EXPECT_EQ(80, view->GetPreferredWidth(400));
  EXPECT_EQ(200, view->GetPreferredWidth(3000));
  EXPECT_EQ(100, view->GetPreferredWidth(1000));

  // Test full state.
  view->Show(MultiContentsDropTargetView::DropSide::START,
             MultiContentsDropTargetView::DropTargetState::kFull,
             MultiContentsDropTargetView::DragType::kLink);
  EXPECT_TRUE(view->GetVisible());
  EXPECT_EQ(100, view->GetPreferredWidth(400));
  EXPECT_EQ(400, view->GetPreferredWidth(3000));
  EXPECT_EQ(200, view->GetPreferredWidth(1000));
}

TEST_F(DropTargetViewTest, AnimateFromNudgeToFull) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kSideBySide, {}},
       {features::kSideBySideDropTargetNudge,
        {{features::kSideBySideDropTargetNudgeMinWidth.name, "50"},
         {features::kSideBySideDropTargetNudgeMaxWidth.name, "100"},
         {features::kSideBySideDropTargetNudgeTargetWidthPercentage.name, "5"},
         {features::kSideBySideDropTargetNudgeToFullMinWidth.name, "80"},
         {features::kSideBySideDropTargetNudgeToFullMaxWidth.name, "2200"},
         {features::kSideBySideDropTargetNudgeToFullTargetWidthPercentage.name,
          "20"}}}},
      {});

  // Arbitrarily chosen. The view will calculate widths relative to this.
  constexpr int kContentsWidth = 1200;

  MultiContentsDropTargetView* view = drop_target_view();
  auto now = base::TimeTicks::Now();
  gfx::AnimationTestApi animation(
      &(drop_target_view()->animation_for_testing()));
  view->animation_for_testing().SetSlideDuration(
      base::Seconds(kDelayedAnimationDuration));
  auto scoped_mode = animation.SetRichAnimationRenderMode(
      gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED);

  // Start in nudge state.
  view->Show(MultiContentsDropTargetView::DropSide::START,
             MultiContentsDropTargetView::DropTargetState::kNudge,
             MultiContentsDropTargetView::DragType::kLink);
  EXPECT_TRUE(view->GetVisible());

  // Finish the animation and check the width.
  animation.SetStartTime(now);
  animation.Step(now + base::Seconds(kDelayedAnimationDuration));
  const int nudge_width = view->GetPreferredWidth(kContentsWidth);
  view->SetSize(gfx::Size(nudge_width, view->size().height()));
  EXPECT_EQ(0.05f * kContentsWidth, nudge_width);

  // Transition to nudge-to-full state with an animation.
  view->Show(MultiContentsDropTargetView::DropSide::START,
             MultiContentsDropTargetView::DropTargetState::kNudgeToFull,
             MultiContentsDropTargetView::DragType::kLink);
  EXPECT_EQ(0.05f * kContentsWidth, view->GetPreferredWidth(kContentsWidth));

  // Step the animation to the middle.
  animation.Step(now + base::Seconds(kDelayedAnimationDuration / 2));

  // Check that the width is between the nudge and nudge-to-full widths.
  // At half of the animation, we expect a width of at most 10% of the contents
  // width.
  const int nudge_to_full_width = 0.2f * kContentsWidth;
  const int current_width = view->GetPreferredWidth(kContentsWidth);
  EXPECT_GT(current_width, nudge_width);
  EXPECT_LT(current_width, nudge_to_full_width);

  // Finish the animation.
  animation.Step(now + base::Seconds(kDelayedAnimationDuration));
  EXPECT_EQ(view->GetPreferredWidth(kContentsWidth), nudge_to_full_width);
}

TEST_F(DropTargetViewTest, AnimateFromNudgeToFullMidAnimation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kSideBySide, {}},
       {features::kSideBySideDropTargetNudge,
        {{features::kSideBySideDropTargetNudgeMinWidth.name, "50"},
         {features::kSideBySideDropTargetNudgeMaxWidth.name, "100"},
         {features::kSideBySideDropTargetNudgeTargetWidthPercentage.name, "5"},
         {features::kSideBySideDropTargetNudgeToFullMinWidth.name, "80"},
         {features::kSideBySideDropTargetNudgeToFullMaxWidth.name, "2200"},
         {features::kSideBySideDropTargetNudgeToFullTargetWidthPercentage.name,
          "20"}}}},
      {});

  // Arbitrarily chosen. The view will calculate widths relative to this.
  constexpr int kContentsWidth = 1200;

  MultiContentsDropTargetView* view = drop_target_view();
  auto now = base::TimeTicks::Now();
  gfx::AnimationTestApi animation(
      &(drop_target_view()->animation_for_testing()));
  view->animation_for_testing().SetSlideDuration(
      base::Seconds(kDelayedAnimationDuration));
  auto scoped_mode = animation.SetRichAnimationRenderMode(
      gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED);

  // Start in nudge state.
  view->Show(MultiContentsDropTargetView::DropSide::START,
             MultiContentsDropTargetView::DropTargetState::kNudge,
             MultiContentsDropTargetView::DragType::kLink);
  EXPECT_TRUE(view->GetVisible());

  // Step the animation to the middle.
  animation.SetStartTime(now);
  animation.Step(now + base::Seconds(kDelayedAnimationDuration / 2));

  const int nudge_width = 0.05f * kContentsWidth;
  const int nudge_mid_animation_width = view->GetPreferredWidth(kContentsWidth);
  EXPECT_GT(nudge_mid_animation_width, 0);
  EXPECT_LT(nudge_mid_animation_width, nudge_width);

  // Transition to nudge-to-full state with an animation.
  view->Show(MultiContentsDropTargetView::DropSide::START,
             MultiContentsDropTargetView::DropTargetState::kNudgeToFull,
             MultiContentsDropTargetView::DragType::kLink);

  // Step the animation by 1ms. The width should be larger than where it was
  // when the nudge-animation was interrupted.
  animation.Step(now + base::Seconds(kDelayedAnimationDuration / 2) +
                 base::Milliseconds(1));
  // Check that the width is between the nudge and nudge-to-full widths.
  const int nudge_to_full_width = 0.2f * kContentsWidth;
  const int full_mid_animation_width = view->GetPreferredWidth(kContentsWidth);
  EXPECT_GT(full_mid_animation_width, nudge_mid_animation_width);
  EXPECT_LT(full_mid_animation_width, nudge_to_full_width);

  // Finish the animation.
  animation.Step(now + base::Seconds(kDelayedAnimationDuration * 2));
  EXPECT_EQ(view->GetPreferredWidth(kContentsWidth), nudge_to_full_width);
}

// Tests that the icon and label are positioned correctly when dragging a tab.
TEST_F(DropTargetViewTest, TabDrag_PositionsIconAndLabelCorrectly) {
  widget()->SetSize(gfx::Size(500, 500));
  drop_target_view()->Show(MultiContentsDropTargetView::DropSide::START,
                           MultiContentsDropTargetView::DropTargetState::kFull,
                           MultiContentsDropTargetView::DragType::kTab);
  widget()->LayoutRootViewIfNecessary();

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
  EXPECT_LT(label->GetBoundsInScreen().bottom(),
            drop_target_view()->GetBoundsInScreen().CenterPoint().y());
}

// Tests that the icon and label are positioned correctly when dragging a link.
TEST_F(DropTargetViewTest, LinkDrag_PositionsIconAndLabelCorrectly) {
  widget()->SetSize(gfx::Size(500, 500));
  drop_target_view()->Show(MultiContentsDropTargetView::DropSide::START,
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

// Tests that the icon and label are positioned correctly when dragging a link.
TEST_F(DropTargetViewTest, IconVerticalAlignmentWhenLabelHidden) {
  widget()->SetSize(
      {drop_target_view()->icon_view_for_testing()->GetPreferredSize().width() +
           1,
       500});
  drop_target_view()->Show(MultiContentsDropTargetView::DropSide::START,
                           MultiContentsDropTargetView::DropTargetState::kNudge,
                           MultiContentsDropTargetView::DragType::kLink);
  widget()->LayoutRootViewIfNecessary();
  views::ImageView* icon = drop_target_view()->icon_view_for_testing();
  const int icon_y_for_nudge = icon->y();

  drop_target_view()->Show(MultiContentsDropTargetView::DropSide::START,
                           MultiContentsDropTargetView::DropTargetState::kFull,
                           MultiContentsDropTargetView::DragType::kLink);
  widget()->LayoutRootViewIfNecessary();

  EXPECT_EQ(0, drop_target_view()->label_for_testing()->width());
  EXPECT_EQ(icon->y(), icon_y_for_nudge);
}

}  // namespace
