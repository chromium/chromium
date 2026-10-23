// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_collection_animating_layout_manager.h"

#include <memory>
#include <tuple>
#include <vector>

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

class MockAnimatingLayoutManagerDelegate
    : public TabCollectionAnimatingLayoutManager::Delegate {
 public:
  MockAnimatingLayoutManagerDelegate() {
    ON_CALL(*this, ShouldAnimateLayout).WillByDefault(testing::Return(true));
  }
  MOCK_METHOD(bool, IsViewDragging, (const views::View&), (const, override));
  MOCK_METHOD(void, OnAnimationEnded, (), (override));
  MOCK_METHOD(bool, ShouldAnimateLayout, (), (override));
};

class TestLayoutManager : public views::LayoutManagerBase {
 public:
  explicit TestLayoutManager(
      TabCollectionAnimatingLayoutManager::AnimationAxis axis)
      : axis_(axis) {}
  ~TestLayoutManager() override = default;

  int calculate_proposed_layout_count() const {
    return calculate_proposed_layout_count_;
  }

 protected:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override {
    ++calculate_proposed_layout_count_;
    views::ProposedLayout layout;

    if (axis_ ==
        TabCollectionAnimatingLayoutManager::AnimationAxis::kVertical) {
      // Place children at full width, starting from the top of the parent.
      int y = 0;
      const int child_width = size_bounds.width().value_or(100);
      for (const auto& child : host_view()->children()) {
        const int child_height = child->GetPreferredSize().height() > 0
                                     ? child->GetPreferredSize().height()
                                     : 20;
        layout.child_layouts.emplace_back(
            child, child->GetVisible(),
            gfx::Rect(0, y, child_width, child_height));
        y += child_height;
      }

      // Calculate the total height based on all child layouts.
      layout.host_size = gfx::Size(child_width, y);
    } else if (axis_ == TabCollectionAnimatingLayoutManager::AnimationAxis::
                            kHorizontalWrappingVertically) {
      // Place children horizontally within a fixed-width parent.
      int x = 0;
      for (const auto& child : host_view()->children()) {
        const int child_width = child->GetPreferredSize().width() > 0
                                    ? child->GetPreferredSize().width()
                                    : 20;
        layout.child_layouts.emplace_back(child, child->GetVisible(),
                                          gfx::Rect(x, 0, child_width, 100));
        x += child_width;
      }

      // Calculate host size with fixed bounded width and total height.
      layout.host_size = gfx::Size(size_bounds.width().value_or(100), 100);
    } else {
      // Place children at full height, starting from the left of the parent.
      int x = 0;
      for (const auto& child : host_view()->children()) {
        const int child_width = child->GetPreferredSize().width() > 0
                                    ? child->GetPreferredSize().width()
                                    : 20;
        layout.child_layouts.emplace_back(child, child->GetVisible(),
                                          gfx::Rect(x, 0, child_width, 100));
        x += child_width;
      }

      // Calculate the total width based on all child layouts.
      layout.host_size = gfx::Size(x, size_bounds.height().value_or(100));
    }
    return layout;
  }

 private:
  TabCollectionAnimatingLayoutManager::AnimationAxis axis_;
  mutable int calculate_proposed_layout_count_ = 0;
};

}  // namespace

class TabCollectionAnimatingLayoutManagerTest
    : public views::ViewsTestBase,
      public testing::WithParamInterface<
          std::tuple<bool,
                     TabCollectionAnimatingLayoutManager::AnimationAxis>> {
 public:
  TabCollectionAnimatingLayoutManagerTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~TabCollectionAnimatingLayoutManagerTest() override = default;

  // views::ViewsTestBase:
  void SetUp() override {
    views::ViewsTestBase::SetUp();

    // Force rich animation because these tests expect to see in-progress values
    // for animated properties. Even when scale mode is ZERO_DURATION the test
    // expects the animation to complete asynchronously.
    render_mode_lock_ = gfx::AnimationTestApi::SetRichAnimationRenderMode(
        gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED);

    animation_mode_ = std::make_unique<gfx::ScopedAnimationDurationScaleMode>(
        IsAnimationDurationEnabled()
            ? gfx::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION
            : gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    host_view_ = widget_->SetContentsView(std::make_unique<views::View>());
    animation_coordinator_ = std::make_unique<
        TabCollectionAnimatingLayoutManager::AnimationCoordinator>(*host_view_);
    // Detach from the widget's compositor animation runner so animations are
    // driven deterministically by `MOCK_TIME` timers rather than asynchronous
    // GPU swap acks.
    animation_coordinator_->SetView(nullptr);
    layout_manager_delegate_ = std::make_unique<
        testing::NiceMock<MockAnimatingLayoutManagerDelegate>>();
    layout_manager_ = host_view_->SetLayoutManager(
        std::make_unique<TabCollectionAnimatingLayoutManager>(
            std::make_unique<TestLayoutManager>(animation_axis()),
            *layout_manager_delegate_.get(), *animation_coordinator_,
            animation_axis()));
    widget_->Show();
  }
  void TearDown() override {
    layout_manager_ = nullptr;
    host_view_ = nullptr;
    widget_.reset();
    animation_coordinator_.reset();
    layout_manager_delegate_.reset();
    animation_mode_.reset();
    views::ViewsTestBase::TearDown();
  }

  bool IsAnimationDurationEnabled() const { return std::get<0>(GetParam()); }
  TabCollectionAnimatingLayoutManager::AnimationAxis animation_axis() const {
    return std::get<1>(GetParam());
  }

  TabCollectionAnimatingLayoutManager* layout_manager() {
    return layout_manager_;
  }
  MockAnimatingLayoutManagerDelegate* layout_manager_delegate() {
    return layout_manager_delegate_.get();
  }
  TabCollectionAnimatingLayoutManager::AnimationCoordinator&
  animation_coordinator() {
    return *animation_coordinator_;
  }
  views::View* host_view() { return host_view_; }
  views::Widget* widget() { return widget_.get(); }

  // Safely sets the layout manager and updates the raw_ptr. This prevents
  // dangling pointers during teardown if the layout manager is replaced
  // mid-test.
  void SetLayoutManager(
      std::unique_ptr<TabCollectionAnimatingLayoutManager> layout_manager) {
    // Explicitly nullify the raw_ptr before the old layout manager is destroyed
    // by View::SetLayoutManagerImpl(...) to avoid triggering a UAF DanglingPtr
    // crash
    layout_manager_ = nullptr;
    layout_manager_ = host_view_->SetLayoutManager(std::move(layout_manager));
  }

 private:
  std::unique_ptr<base::AutoReset<gfx::Animation::RichAnimationRenderMode>>
      render_mode_lock_;
  std::unique_ptr<gfx::ScopedAnimationDurationScaleMode> animation_mode_;
  std::unique_ptr<MockAnimatingLayoutManagerDelegate> layout_manager_delegate_;
  std::unique_ptr<TabCollectionAnimatingLayoutManager::AnimationCoordinator>
      animation_coordinator_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::View> host_view_;
  raw_ptr<TabCollectionAnimatingLayoutManager> layout_manager_;
};

TEST_P(TabCollectionAnimatingLayoutManagerTest, AddChild) {
  const bool is_vertical =
      animation_axis() ==
      TabCollectionAnimatingLayoutManager::AnimationAxis::kVertical;

  // Setup the Widget's container view bounds.
  widget()->SetBounds(gfx::Rect(0, 0, 100, 100));
  widget()->LayoutRootViewIfNecessary();

  const auto add_child_and_animate_to_target = [&]() {
    // Add an empty child.
    auto* const child =
        host_view()->AddChildView(std::make_unique<views::View>());

    // Trigger an initial layout.
    host_view()->InvalidateLayout();
    widget()->LayoutRootViewIfNecessary();

    // Size along animation axis should start at 0 with empty bounds.
    if (is_vertical) {
      EXPECT_EQ(child->height(), 0);
    } else {
      EXPECT_EQ(child->width(), 0);
    }
    EXPECT_TRUE(child->bounds().IsEmpty());

    // Expect callback when animation ends.
    EXPECT_CALL(*layout_manager_delegate(), OnAnimationEnded());

    // Advance time such that the animation has time to complete.
    task_environment()->FastForwardBy(base::Seconds(1));

    // Ensure final layout is applied.
    widget()->LayoutRootViewIfNecessary();

    return child;
  };

  // Add the first child, verify it animates to target bounds.
  const auto* child1 = add_child_and_animate_to_target();
  EXPECT_EQ(child1->bounds(),
            is_vertical ? gfx::Rect(0, 0, 100, 20) : gfx::Rect(0, 0, 20, 100));

  // Add another child, verify it also animates to target bounds.
  const auto* child2 = add_child_and_animate_to_target();
  EXPECT_EQ(child2->bounds(), is_vertical ? gfx::Rect(0, 20, 100, 20)
                                          : gfx::Rect(20, 0, 20, 100));
}

TEST_P(TabCollectionAnimatingLayoutManagerTest,
       PreferredSizeDuringSwapAnimation) {
  // Use standard duration for frame-by-frame observation.
  gfx::ScopedAnimationDurationScaleMode normal_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Setup the layout manager and initial child views.
  SetLayoutManager(std::make_unique<TabCollectionAnimatingLayoutManager>(
      std::make_unique<TestLayoutManager>(animation_axis()),
      *layout_manager_delegate(), animation_coordinator(), animation_axis(),
      /*animate_host_size=*/true));

  widget()->SetBounds(gfx::Rect(0, 0, 100, 100));
  auto* const child1 =
      host_view()->AddChildView(std::make_unique<views::View>());
  auto* const child2 =
      host_view()->AddChildView(std::make_unique<views::View>());
  widget()->LayoutRootViewIfNecessary();

  // Advance time such that the initial add animation has time to complete.
  task_environment()->FastForwardBy(base::Seconds(1));

  const int expected_size =
      animation_axis() == TabCollectionAnimatingLayoutManager::AnimationAxis::
                              kHorizontalWrappingVertically
          ? 100
          : 40;

  // Swap tabs.
  host_view()->ReorderChildView(child2, 0);
  host_view()->InvalidateLayout();
  widget()->LayoutRootViewIfNecessary();

  // Ensures preferred size remains stable during the swap animation.
  // The animation duration is 200ms, so we poll 19 times in 10ms steps (up to
  // 190ms) to observe in-progress frames before the animation completes at
  // 200ms.
  for (int i = 0; i < 19; ++i) {
    task_environment()->FastForwardBy(base::Milliseconds(10));
    widget()->LayoutRootViewIfNecessary();

    ASSERT_TRUE(layout_manager()->is_animating());

    // Verify stability at every frame along the animation axis.
    if (animation_axis() ==
            TabCollectionAnimatingLayoutManager::AnimationAxis::kVertical ||
        animation_axis() == TabCollectionAnimatingLayoutManager::AnimationAxis::
                                kHorizontalWrappingVertically) {
      EXPECT_EQ(layout_manager()->GetPreferredSize(host_view()).height(),
                expected_size);
    } else {
      EXPECT_EQ(layout_manager()->GetPreferredSize(host_view()).width(),
                expected_size);
    }
  }

  // Advance time to allow the swap animation to reach its final state.
  task_environment()->FastForwardBy(base::Seconds(1));

  // Verify final state.
  EXPECT_FALSE(layout_manager()->is_animating());
  if (animation_axis() ==
      TabCollectionAnimatingLayoutManager::AnimationAxis::kVertical) {
    EXPECT_EQ(layout_manager()->GetPreferredSize(host_view()).height(),
              expected_size);
    EXPECT_EQ(child2->bounds(), gfx::Rect(0, 0, 100, 20));
    EXPECT_EQ(child1->bounds(), gfx::Rect(0, 20, 100, 20));
  } else if (animation_axis() ==
             TabCollectionAnimatingLayoutManager::AnimationAxis::
                 kHorizontalWrappingVertically) {
    EXPECT_EQ(layout_manager()->GetPreferredSize(host_view()).height(),
              expected_size);
    EXPECT_EQ(child2->bounds(), gfx::Rect(0, 0, 20, 100));
    EXPECT_EQ(child1->bounds(), gfx::Rect(20, 0, 20, 100));
  } else {
    EXPECT_EQ(layout_manager()->GetPreferredSize(host_view()).width(),
              expected_size);
    EXPECT_EQ(child2->bounds(), gfx::Rect(0, 0, 20, 100));
    EXPECT_EQ(child1->bounds(), gfx::Rect(20, 0, 20, 100));
  }
}

TEST_P(TabCollectionAnimatingLayoutManagerTest,
       AnimationCoordinatorSynchronizesMidAnimationJoin) {
  gfx::ScopedAnimationDurationScaleMode normal_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  const bool is_vertical =
      animation_axis() ==
      TabCollectionAnimatingLayoutManager::AnimationAxis::kVertical;

  testing::NiceMock<MockAnimatingLayoutManagerDelegate> second_delegate;
  auto second_host = std::make_unique<views::View>();
  auto* second_layout_manager = second_host->SetLayoutManager(
      std::make_unique<TabCollectionAnimatingLayoutManager>(
          std::make_unique<TestLayoutManager>(animation_axis()),
          second_delegate, animation_coordinator(), animation_axis()));

  widget()->SetBounds(gfx::Rect(0, 0, 100, 100));
  second_host->SetBounds(0, 0, 100, 100);
  widget()->LayoutRootViewIfNecessary();
  views::test::RunScheduledLayout(second_host.get());
  while (layout_manager()->is_animating() ||
         second_layout_manager->is_animating()) {
    task_environment()->FastForwardBy(base::Milliseconds(10));
    widget()->LayoutRootViewIfNecessary();
    views::test::RunScheduledLayout(second_host.get());
  }

  // Start an animation on the first manager and advance until the animation
  // coordinator is mid-flight (< kResetAnimationThreshold).
  host_view()->AddChildView(std::make_unique<views::View>());
  host_view()->InvalidateLayout();
  widget()->LayoutRootViewIfNecessary();
  while (animation_coordinator().current_offset() == 0.0) {
    task_environment()->FastForwardBy(base::Milliseconds(10));
    widget()->LayoutRootViewIfNecessary();
  }

  ASSERT_TRUE(layout_manager()->is_animating());
  ASSERT_FALSE(second_layout_manager->is_animating());
  ASSERT_GT(animation_coordinator().current_offset(), 0.0);
  ASSERT_LT(animation_coordinator().current_offset(), 0.8);

  // In the same UI turn, add a child to both managers. Both should synchronize
  // to the current animation coordinator offset and progress in lockstep.
  auto* child_a = host_view()->AddChildView(std::make_unique<views::View>());
  auto* child_b = second_host->AddChildView(std::make_unique<views::View>());
  host_view()->InvalidateLayout();
  second_host->InvalidateLayout();
  widget()->LayoutRootViewIfNecessary();
  views::test::RunScheduledLayout(second_host.get());

  EXPECT_TRUE(layout_manager()->is_animating());
  EXPECT_TRUE(second_layout_manager->is_animating());
  EXPECT_EQ(animation_coordinator().starting_offset(),
            animation_coordinator().current_offset());

  while (layout_manager()->is_animating() ||
         second_layout_manager->is_animating()) {
    task_environment()->FastForwardBy(base::Milliseconds(10));
    widget()->LayoutRootViewIfNecessary();
    views::test::RunScheduledLayout(second_host.get());

    // Both managers must remain in lockstep at every tick and finish on the
    // exact same tick.
    EXPECT_EQ(layout_manager()->is_animating(),
              second_layout_manager->is_animating());
    if (is_vertical) {
      EXPECT_EQ(child_a->height(), child_b->height());
    } else {
      EXPECT_NEAR(child_a->width(), child_b->width(), 1);
    }
  }

  EXPECT_EQ(is_vertical ? child_a->height() : child_a->width(), 20);
  EXPECT_EQ(is_vertical ? child_b->height() : child_b->width(), 20);
}

TEST_P(TabCollectionAnimatingLayoutManagerTest,
       AnimationCoordinatorSynchronizesResetAboveThreshold) {
  gfx::ScopedAnimationDurationScaleMode normal_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  const bool is_vertical =
      animation_axis() ==
      TabCollectionAnimatingLayoutManager::AnimationAxis::kVertical;

  testing::NiceMock<MockAnimatingLayoutManagerDelegate> second_delegate;
  auto second_host = std::make_unique<views::View>();
  auto* second_layout_manager = second_host->SetLayoutManager(
      std::make_unique<TabCollectionAnimatingLayoutManager>(
          std::make_unique<TestLayoutManager>(animation_axis()),
          second_delegate, animation_coordinator(), animation_axis()));

  widget()->SetBounds(gfx::Rect(0, 0, 100, 100));
  second_host->SetBounds(0, 0, 100, 100);
  widget()->LayoutRootViewIfNecessary();
  views::test::RunScheduledLayout(second_host.get());
  while (layout_manager()->is_animating() ||
         second_layout_manager->is_animating()) {
    task_environment()->FastForwardBy(base::Milliseconds(10));
    widget()->LayoutRootViewIfNecessary();
    views::test::RunScheduledLayout(second_host.get());
  }

  // Start an animation on the first manager and advance until the animation
  // coordinator is past `kResetAnimationThreshold` (0.8).
  auto* child_a = host_view()->AddChildView(std::make_unique<views::View>());
  host_view()->InvalidateLayout();
  widget()->LayoutRootViewIfNecessary();
  while (animation_coordinator().current_offset() <= 0.8) {
    task_environment()->FastForwardBy(base::Milliseconds(10));
    widget()->LayoutRootViewIfNecessary();
  }

  ASSERT_TRUE(layout_manager()->is_animating());
  ASSERT_GT(animation_coordinator().current_offset(), 0.8);
  const int size_before_reset =
      is_vertical ? child_a->height() : child_a->width();
  ASSERT_GT(size_before_reset, 0);

  // Trigger a new animation on the second manager. This resets the animation
  // coordinator to 0.0, and the first manager must re-baseline its starting
  // layout to its current interpolated state rather than jumping backward to 0.
  second_host->AddChildView(std::make_unique<views::View>());
  second_host->InvalidateLayout();
  views::test::RunScheduledLayout(second_host.get());
  widget()->LayoutRootViewIfNecessary();

  EXPECT_EQ(animation_coordinator().current_offset(), 0.0);
  EXPECT_EQ(animation_coordinator().starting_offset(), 0.0);
  EXPECT_GE(is_vertical ? child_a->height() : child_a->width(),
            size_before_reset);

  while (layout_manager()->is_animating() ||
         second_layout_manager->is_animating()) {
    task_environment()->FastForwardBy(base::Milliseconds(10));
    widget()->LayoutRootViewIfNecessary();
    views::test::RunScheduledLayout(second_host.get());
  }

  EXPECT_FALSE(layout_manager()->is_animating());
  EXPECT_FALSE(second_layout_manager->is_animating());
  EXPECT_EQ(is_vertical ? child_a->height() : child_a->width(), 20);
}

TEST_P(TabCollectionAnimatingLayoutManagerTest,
       AnimationCoordinatorSurvivesChildManagerDestruction) {
  gfx::ScopedAnimationDurationScaleMode normal_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  testing::NiceMock<MockAnimatingLayoutManagerDelegate> second_delegate;
  auto second_host = std::make_unique<views::View>();
  second_host->SetLayoutManager(
      std::make_unique<TabCollectionAnimatingLayoutManager>(
          std::make_unique<TestLayoutManager>(animation_axis()),
          second_delegate, animation_coordinator(), animation_axis()));

  widget()->SetBounds(gfx::Rect(0, 0, 100, 100));
  second_host->SetBounds(0, 0, 100, 100);
  host_view()->AddChildView(std::make_unique<views::View>());
  second_host->AddChildView(std::make_unique<views::View>());
  widget()->LayoutRootViewIfNecessary();
  views::test::RunScheduledLayout(second_host.get());

  while (animation_coordinator().current_offset() == 0.0) {
    task_environment()->FastForwardBy(base::Milliseconds(10));
    widget()->LayoutRootViewIfNecessary();
    views::test::RunScheduledLayout(second_host.get());
  }
  ASSERT_TRUE(layout_manager()->is_animating());

  // Destroy the second host and its layout manager mid-animation.
  second_host.reset();
  EXPECT_TRUE(layout_manager()->is_animating());

  while (layout_manager()->is_animating()) {
    task_environment()->FastForwardBy(base::Milliseconds(10));
    widget()->LayoutRootViewIfNecessary();
  }
  EXPECT_FALSE(layout_manager()->is_animating());
}

TEST_P(TabCollectionAnimatingLayoutManagerTest,
       StopAnimationNotifiesOnAnimationEnded) {
  gfx::ScopedAnimationDurationScaleMode normal_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  widget()->SetBounds(gfx::Rect(0, 0, 100, 100));
  widget()->LayoutRootViewIfNecessary();

  // Add a child to start an animation.
  host_view()->AddChildView(std::make_unique<views::View>());
  host_view()->InvalidateLayout();
  widget()->LayoutRootViewIfNecessary();
  ASSERT_TRUE(layout_manager()->is_animating());

  // Resize along the cross-axis mid-animation, which triggers `StopAnimation()`
  // and should synchronously notify the delegate that the animation ended.
  EXPECT_CALL(*layout_manager_delegate(), OnAnimationEnded()).Times(1);
  widget()->SetBounds(gfx::Rect(0, 0, 150, 150));
  widget()->LayoutRootViewIfNecessary();
  EXPECT_FALSE(layout_manager()->is_animating());
}

TEST_P(TabCollectionAnimatingLayoutManagerTest,
       StationaryChildInMovingNestedContainerHasZeroJitter) {
  gfx::ScopedAnimationDurationScaleMode normal_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  const bool is_vertical =
      animation_axis() ==
      TabCollectionAnimatingLayoutManager::AnimationAxis::kVertical;

  widget()->SetBounds(gfx::Rect(0, 0, 100, 100));

  // Create a leading tab with an odd size (21px) so 50% progress produces
  // half-integer offsets (-10.5 vs +10.5) that would otherwise round in the
  // same positive direction and cause +1px screen jitter on stationary views.
  auto* leading_tab =
      host_view()->AddChildView(std::make_unique<views::View>());
  leading_tab->SetPreferredSize(gfx::Size(21, 21));

  testing::NiceMock<MockAnimatingLayoutManagerDelegate> group_delegate;
  auto* group_view = host_view()->AddChildView(std::make_unique<views::View>());
  auto* group_layout_manager = group_view->SetLayoutManager(
      std::make_unique<TabCollectionAnimatingLayoutManager>(
          std::make_unique<TestLayoutManager>(animation_axis()), group_delegate,
          animation_coordinator(), animation_axis()));

  group_view->AddChildView(std::make_unique<views::View>());
  auto* stationary_tab =
      group_view->AddChildView(std::make_unique<views::View>());

  host_view()->InvalidateLayout();
  group_view->InvalidateLayout();
  widget()->LayoutRootViewIfNecessary();
  task_environment()->FastForwardBy(base::Seconds(1));
  widget()->LayoutRootViewIfNecessary();

  ASSERT_EQ(is_vertical ? group_view->bounds().y() : group_view->bounds().x(),
            21);
  ASSERT_EQ(
      is_vertical ? stationary_tab->bounds().y() : stationary_tab->bounds().x(),
      20);

  // Move `leading_tab` into `group_view` after the header: `group_view` shifts
  // in `host_view()` from 21 to 0 (-21px) and expands from 40 to 61, while
  // `stationary_tab` shifts inside `group_view` from 20 to 41 (+21px).
  group_view->AddChildViewAt(host_view()->RemoveChildViewT(leading_tab), 1);
  group_view->InvalidateLayout();
  host_view()->InvalidateLayout();
  widget()->LayoutRootViewIfNecessary();

  // Step through the animation (including a mid-animation interruption on the
  // parent container at step 10) and verify `stationary_tab` remains at exact
  // position 41 in `host_view()` with 0px jitter.
  int step = 0;
  while (layout_manager()->is_animating() ||
         group_layout_manager->is_animating()) {
    if (step == 10) {
      host_view()->AddChildView(std::make_unique<views::View>());
      host_view()->InvalidateLayout();
      widget()->LayoutRootViewIfNecessary();
    }

    task_environment()->FastForwardBy(base::Milliseconds(5));
    widget()->LayoutRootViewIfNecessary();

    const int group_pos =
        is_vertical ? group_view->bounds().y() : group_view->bounds().x();
    const int stationary_pos_in_group = is_vertical
                                            ? stationary_tab->bounds().y()
                                            : stationary_tab->bounds().x();
    EXPECT_EQ(group_pos + stationary_pos_in_group, 41) << "at step " << step;
    ++step;
  }

  EXPECT_GT(step, 20);
  EXPECT_EQ(is_vertical ? group_view->bounds().y() : group_view->bounds().x(),
            0);
  EXPECT_EQ(
      is_vertical ? stationary_tab->bounds().y() : stationary_tab->bounds().x(),
      41);
}

TEST_P(TabCollectionAnimatingLayoutManagerTest,
       ReparentedChildInMovingNestedContainerHasNoParabolicOvershoot) {
  gfx::ScopedAnimationDurationScaleMode normal_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  const bool is_vertical =
      animation_axis() ==
      TabCollectionAnimatingLayoutManager::AnimationAxis::kVertical;

  widget()->SetBounds(gfx::Rect(0, 0, 200, 200));

  // Create a 100px leading tab before `group_view` (which has a 20px header).
  // When `leading_tab` is reparented into `group_view`, `group_view` shifts by
  // -100px while `leading_tab` moves by a net +20px in `host_view()`. Without
  // converting `kPreviousCollectionBounds` relative to `group_view`'s starting
  // bounds, `leading_tab` would parabolically overshoot to 36px before
  // reversing back to 20px.
  auto* leading_tab =
      host_view()->AddChildView(std::make_unique<views::View>());
  leading_tab->SetPreferredSize(gfx::Size(100, 100));

  testing::NiceMock<MockAnimatingLayoutManagerDelegate> group_delegate;
  auto* group_view = host_view()->AddChildView(std::make_unique<views::View>());
  auto* group_layout_manager = group_view->SetLayoutManager(
      std::make_unique<TabCollectionAnimatingLayoutManager>(
          std::make_unique<TestLayoutManager>(animation_axis()), group_delegate,
          animation_coordinator(), animation_axis()));
  group_view->AddChildView(std::make_unique<views::View>());

  host_view()->InvalidateLayout();
  group_view->InvalidateLayout();
  widget()->LayoutRootViewIfNecessary();
  task_environment()->FastForwardBy(base::Seconds(1));
  widget()->LayoutRootViewIfNecessary();

  ASSERT_EQ(is_vertical ? leading_tab->bounds().y() : leading_tab->bounds().x(),
            0);
  ASSERT_EQ(is_vertical ? group_view->bounds().y() : group_view->bounds().x(),
            100);

  const gfx::Rect previous_bounds_in_screen = leading_tab->GetBoundsInScreen();
  group_layout_manager->AnimateAndReparentView(
      host_view()->RemoveChildViewT(leading_tab), previous_bounds_in_screen);
  group_view->InvalidateLayout();
  host_view()->InvalidateLayout();
  widget()->LayoutRootViewIfNecessary();

  int prev_pos_in_host = 0;
  int step = 0;
  while (layout_manager()->is_animating() ||
         group_layout_manager->is_animating()) {
    task_environment()->FastForwardBy(base::Milliseconds(5));
    widget()->LayoutRootViewIfNecessary();

    const int group_pos =
        is_vertical ? group_view->bounds().y() : group_view->bounds().x();
    const int tab_pos_in_group =
        is_vertical ? leading_tab->bounds().y() : leading_tab->bounds().x();
    const int pos_in_host = group_pos + tab_pos_in_group;

    EXPECT_GE(pos_in_host, prev_pos_in_host) << "at step " << step;
    EXPECT_LE(pos_in_host, 20) << "at step " << step;
    prev_pos_in_host = pos_in_host;
    ++step;
  }

  EXPECT_GT(step, 20);
  EXPECT_EQ(prev_pos_in_host, 20);
}

TEST_P(TabCollectionAnimatingLayoutManagerTest,
       ReparentedChildInNewMovingNestedContainerHasNoParabolicOvershoot) {
  gfx::ScopedAnimationDurationScaleMode normal_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  const bool is_vertical =
      animation_axis() ==
      TabCollectionAnimatingLayoutManager::AnimationAxis::kVertical;

  widget()->SetBounds(gfx::Rect(0, 0, 200, 200));

  // Create a 100px leading tab before `group_view` (which has a 20px header).
  // When `leading_tab` is reparented into `group_view`, `group_view` shifts by
  // -100px while `leading_tab` moves by a net +20px in `host_view()`. Without
  // converting `kPreviousCollectionBounds` relative to `group_view`'s starting
  // bounds, `leading_tab` would parabolically overshoot to 36px before
  // reversing back to 20px.
  auto* leading_tab =
      host_view()->AddChildView(std::make_unique<views::View>());
  leading_tab->SetPreferredSize(gfx::Size(100, 100));

  // Additionally, we need to create another tab after the position where we
  // intend to add the group view. Otherwise, the value of adding_views_start_x_
  // for the group view will be 0, meaning that the group will not move at all,
  // resulting in a false positive for this test.
  auto* trailing_tab =
      host_view()->AddChildView(std::make_unique<views::View>());
  trailing_tab->SetPreferredSize(gfx::Size(100, 100));

  host_view()->InvalidateLayout();
  widget()->LayoutRootViewIfNecessary();
  task_environment()->FastForwardBy(base::Seconds(1));
  widget()->LayoutRootViewIfNecessary();

  ASSERT_FALSE(layout_manager()->is_animating());
  ASSERT_EQ(is_vertical ? leading_tab->bounds().y() : leading_tab->bounds().x(),
            0);
  ASSERT_EQ(
      is_vertical ? trailing_tab->bounds().y() : trailing_tab->bounds().x(),
      100);

  class FixedHeightTestLayoutManager : public TestLayoutManager {
   public:
    explicit FixedHeightTestLayoutManager(
        TabCollectionAnimatingLayoutManager::AnimationAxis axis)
        : TestLayoutManager(axis) {}
    ~FixedHeightTestLayoutManager() override = default;

   protected:
    views::ProposedLayout CalculateProposedLayout(
        const views::SizeBounds& size_bounds) const override {
      views::ProposedLayout layout =
          TestLayoutManager::CalculateProposedLayout(size_bounds);
      layout.host_size.set_height(41);
      return layout;
    }
  };
  testing::NiceMock<MockAnimatingLayoutManagerDelegate> group_delegate;
  auto* group_view =
      host_view()->AddChildViewAt(std::make_unique<views::View>(), 1);
  auto* group_layout_manager = group_view->SetLayoutManager(
      std::make_unique<TabCollectionAnimatingLayoutManager>(
          // In horizontal tabs, tab group views have a fixed height. Make our
          // mock tab group view also report a fixed height, otherwise during
          // the initial layout pass, a cross-axis resize will stop it from
          // animating.
          animation_axis() == TabCollectionAnimatingLayoutManager::
                                  AnimationAxis::kHorizontal
              ? std::make_unique<FixedHeightTestLayoutManager>(animation_axis())
              : std::make_unique<TestLayoutManager>(animation_axis()),
          group_delegate, animation_coordinator(), animation_axis()));
  auto* header = group_view->AddChildView(std::make_unique<views::View>());

  const gfx::Rect previous_bounds_in_screen = leading_tab->GetBoundsInScreen();
  group_layout_manager->AnimateAndReparentView(
      host_view()->RemoveChildViewT(leading_tab), previous_bounds_in_screen);
  group_view->InvalidateLayout();
  host_view()->InvalidateLayout();
  if (is_vertical) {
    // Even before the initial layout pass updates `group_view->width()`, the
    // nested group layout manager should use its target width from the parent
    // container so children start at full width instead of 0.
    EXPECT_EQ(group_layout_manager->target_layout()
                  .GetLayoutFor(header)
                  ->bounds.width(),
              200);
    EXPECT_EQ(group_layout_manager->target_layout()
                  .GetLayoutFor(leading_tab)
                  ->bounds.width(),
              200);
  }
  widget()->LayoutRootViewIfNecessary();

  int prev_pos_in_host = 0;
  int step = 0;
  while (layout_manager()->is_animating() ||
         group_layout_manager->is_animating()) {
    task_environment()->FastForwardBy(base::Milliseconds(5));
    widget()->LayoutRootViewIfNecessary();

    const int group_pos =
        is_vertical ? group_view->bounds().y() : group_view->bounds().x();
    const int tab_pos_in_group =
        is_vertical ? leading_tab->bounds().y() : leading_tab->bounds().x();
    const int pos_in_host = group_pos + tab_pos_in_group;

    EXPECT_GE(pos_in_host, prev_pos_in_host) << "at step " << step;
    EXPECT_LE(pos_in_host, 20) << "at step " << step;
    prev_pos_in_host = pos_in_host;
    ++step;
  }

  EXPECT_GT(step, 20);
  EXPECT_EQ(prev_pos_in_host, 20);
}

TEST_P(TabCollectionAnimatingLayoutManagerTest, ShouldAnimateLayout) {
  if (!IsAnimationDurationEnabled()) {
    GTEST_SKIP();
  }
  ON_CALL(*layout_manager_delegate(), ShouldAnimateLayout)
      .WillByDefault(testing::Return(false));

  widget()->SetBounds(gfx::Rect(0, 0, 100, 100));
  widget()->LayoutRootViewIfNecessary();

  // When `ShouldAnimateLayout()` is false, new layouts snap immediately to
  // target without animating.
  host_view()->AddChildView(std::make_unique<views::View>());
  host_view()->InvalidateLayout();
  widget()->LayoutRootViewIfNecessary();

  EXPECT_FALSE(layout_manager()->is_animating());

  // Once `ShouldAnimateLayout()` is true, a new layout starts an animation.
  ON_CALL(*layout_manager_delegate(), ShouldAnimateLayout)
      .WillByDefault(testing::Return(true));

  host_view()->AddChildView(std::make_unique<views::View>());
  host_view()->InvalidateLayout();
  widget()->LayoutRootViewIfNecessary();

  EXPECT_TRUE(layout_manager()->is_animating());
}

TEST_P(TabCollectionAnimatingLayoutManagerTest,
       NestedContainerRecalculatesTargetTopDownOnChildInvalidation) {
  gfx::ScopedAnimationDurationScaleMode normal_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Simulates a parent container (`UnpinnedTabContainerView`) that allocates
  // target bounds to a nested container (`TabGroupView`) during the parent's
  // target layout calculation. When a child is added to the nested container,
  // `OnLayoutChanged()` fires on the nested container first (bottom-up), but
  // defers `RecalculateTarget()` until invalidation bubbles up to the parent
  // so targets are recalculated in a single top-down pass.
  class ParentContainerLayoutManager : public views::LayoutManagerBase {
   public:
    explicit ParentContainerLayoutManager(std::vector<std::string>* order)
        : order_(order) {}

   protected:
    views::ProposedLayout CalculateProposedLayout(
        const views::SizeBounds& size_bounds) const override {
      if (size_bounds.width().is_bounded()) {
        order_->push_back("parent");
      }
      views::ProposedLayout layout;
      int x = 0;
      for (views::View* child : host_view()->children()) {
        const int width = child->GetPreferredSize().width();
        layout.child_layouts.emplace_back(child, child->GetVisible(),
                                          gfx::Rect(x, 0, width, 100));
        x += width;
      }
      layout.host_size = gfx::Size(x, 100);
      return layout;
    }

   private:
    raw_ptr<std::vector<std::string>> order_;
  };

  class ProportionalGroupLayoutManager : public views::LayoutManagerBase {
   public:
    explicit ProportionalGroupLayoutManager(std::vector<std::string>* order)
        : order_(order) {}

   protected:
    views::ProposedLayout CalculateProposedLayout(
        const views::SizeBounds& size_bounds) const override {
      if (size_bounds.width().is_bounded()) {
        order_->push_back("group");
      }
      views::ProposedLayout layout;
      const size_t count = host_view()->children().size();
      if (count == 0) {
        return layout;
      }
      const int preferred_width = static_cast<int>(count) * 20;
      const int available_width = size_bounds.width().value_or(preferred_width);
      const int child_width = available_width / static_cast<int>(count);
      int x = 0;
      for (views::View* child : host_view()->children()) {
        layout.child_layouts.emplace_back(child, child->GetVisible(),
                                          gfx::Rect(x, 0, child_width, 100));
        x += child_width;
      }
      layout.host_size = gfx::Size(available_width, 100);
      return layout;
    }

   private:
    raw_ptr<std::vector<std::string>> order_;
  };

  widget()->SetBounds(gfx::Rect(0, 0, 200, 100));

  // Register an unrelated sibling container on the same coordinator to verify
  // it is not recalculated when `group_view` is invalidated.
  testing::NiceMock<MockAnimatingLayoutManagerDelegate> sibling_delegate;
  auto sibling_host = std::make_unique<views::View>();
  auto* sibling_layout_manager = sibling_host->SetLayoutManager(
      std::make_unique<TabCollectionAnimatingLayoutManager>(
          std::make_unique<TestLayoutManager>(
              TabCollectionAnimatingLayoutManager::AnimationAxis::kHorizontal),
          sibling_delegate, animation_coordinator(),
          TabCollectionAnimatingLayoutManager::AnimationAxis::kHorizontal));
  sibling_host->SetBounds(0, 0, 100, 100);
  views::test::RunScheduledLayout(sibling_host.get());
  auto* sibling_target_layout = static_cast<TestLayoutManager*>(
      sibling_layout_manager->target_layout_manager());

  std::vector<std::string> target_calculation_order;
  testing::NiceMock<MockAnimatingLayoutManagerDelegate> group_delegate;

  auto* group_view = host_view()->AddChildView(std::make_unique<views::View>());
  SetLayoutManager(std::make_unique<TabCollectionAnimatingLayoutManager>(
      std::make_unique<ParentContainerLayoutManager>(&target_calculation_order),
      *layout_manager_delegate(), animation_coordinator(),
      TabCollectionAnimatingLayoutManager::AnimationAxis::kHorizontal));

  auto* group_layout_manager = group_view->SetLayoutManager(
      std::make_unique<TabCollectionAnimatingLayoutManager>(
          std::make_unique<ProportionalGroupLayoutManager>(
              &target_calculation_order),
          group_delegate, animation_coordinator(),
          TabCollectionAnimatingLayoutManager::AnimationAxis::kHorizontal));

  auto* child1 = group_view->AddChildView(std::make_unique<views::View>());
  widget()->LayoutRootViewIfNecessary();
  task_environment()->FastForwardBy(base::Seconds(1));
  widget()->LayoutRootViewIfNecessary();

  ASSERT_EQ(layout_manager()->target_layout().GetLayoutFor(group_view)->bounds,
            gfx::Rect(0, 0, 20, 100));
  ASSERT_EQ(group_layout_manager->target_layout().GetLayoutFor(child1)->bounds,
            gfx::Rect(0, 0, 20, 100));

  target_calculation_order.clear();
  const int sibling_count_before =
      sibling_target_layout->calculate_proposed_layout_count();

  // Adding `child2` to `group_view` triggers `group_view`'s `OnLayoutChanged()`
  // first, which defers target recalculation until invalidation bubbles up to
  // `host_view()`. `PropagateRecalculateTarget()` then runs once top-down
  // ("parent" then "group"), updating `group_view`'s target width in the
  // parent to 40 before `group_view` calculates its target layout, and
  // skipping the unrelated `sibling_host`.
  auto* child2 = group_view->AddChildView(std::make_unique<views::View>());
  EXPECT_THAT(target_calculation_order,
              testing::ElementsAre("parent", "group"));
  EXPECT_EQ(sibling_target_layout->calculate_proposed_layout_count(),
            sibling_count_before);
  EXPECT_EQ(layout_manager()->target_layout().GetLayoutFor(group_view)->bounds,
            gfx::Rect(0, 0, 40, 100));
  EXPECT_EQ(group_layout_manager->target_layout().GetLayoutFor(child1)->bounds,
            gfx::Rect(0, 0, 20, 100));
  EXPECT_EQ(group_layout_manager->target_layout().GetLayoutFor(child2)->bounds,
            gfx::Rect(20, 0, 20, 100));

  // Stepping through animation ticks should not trigger additional target
  // recalculations on either the parent or nested group container.
  target_calculation_order.clear();
  while (layout_manager()->is_animating() ||
         group_layout_manager->is_animating()) {
    task_environment()->FastForwardBy(base::Milliseconds(10));
    widget()->LayoutRootViewIfNecessary();
  }
  EXPECT_THAT(target_calculation_order, testing::IsEmpty());
}

TEST_P(TabCollectionAnimatingLayoutManagerTest,
       LayoutSkipsRecalculateTargetDuringAnimationUnlessSizeBoundsChange) {
  gfx::ScopedAnimationDurationScaleMode normal_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  widget()->SetBounds(gfx::Rect(0, 0, 100, 100));
  widget()->LayoutRootViewIfNecessary();

  auto* test_layout = static_cast<TestLayoutManager*>(
      layout_manager()->target_layout_manager());

  host_view()->AddChildView(std::make_unique<views::View>());
  host_view()->InvalidateLayout();
  widget()->LayoutRootViewIfNecessary();
  ASSERT_TRUE(layout_manager()->is_animating());

  const int count_after_start = test_layout->calculate_proposed_layout_count();
  int frames = 0;
  while (layout_manager()->is_animating()) {
    task_environment()->FastForwardBy(base::Milliseconds(5));
    widget()->LayoutRootViewIfNecessary();
    ++frames;
  }
  EXPECT_GT(frames, 10);
  // `LayoutImpl()` should not recalculate the target layout on animation ticks
  // when available `SizeBounds` are unchanged.
  EXPECT_EQ(test_layout->calculate_proposed_layout_count(), count_after_start);

  // Resizing the widget changes `GetAvailableHostSize()`, which should cause
  // `LayoutImpl()` to recalculate the target layout without `InvalidateLayout`.
  widget()->SetBounds(gfx::Rect(0, 0, 150, 100));
  widget()->LayoutRootViewIfNecessary();
  EXPECT_GT(test_layout->calculate_proposed_layout_count(), count_after_start);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TabCollectionAnimatingLayoutManagerTest,
    testing::Combine(
        testing::Bool(),
        testing::Values(
            TabCollectionAnimatingLayoutManager::AnimationAxis::kVertical,
            TabCollectionAnimatingLayoutManager::AnimationAxis::kHorizontal,
            TabCollectionAnimatingLayoutManager::AnimationAxis::
                kHorizontalWrappingVertically)),
    [](const testing::TestParamInfo<
        TabCollectionAnimatingLayoutManagerTest::ParamType>& info) {
      const bool duration_enabled = std::get<0>(info.param);
      const auto axis = std::get<1>(info.param);
      std::string axis_str;
      switch (axis) {
        case TabCollectionAnimatingLayoutManager::AnimationAxis::kVertical:
          axis_str = "Vertical";
          break;
        case TabCollectionAnimatingLayoutManager::AnimationAxis::kHorizontal:
          axis_str = "Horizontal";
          break;
        case TabCollectionAnimatingLayoutManager::AnimationAxis::
            kHorizontalWrappingVertically:
          axis_str = "HorizontalWrappingVertically";
          break;
      }
      return base::StrCat({
          duration_enabled ? "AnimationDurationEnabled"
                           : "AnimationDurationDisabled",
          "_",
          axis_str,
      });
    });
