// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/tab_collection_animating_layout_manager.h"

#include <memory>
#include <vector>

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
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
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

class MockAnimatingLayoutManagerDelegate
    : public TabCollectionAnimatingLayoutManager::Delegate {
 public:
  MOCK_METHOD(bool, IsViewDragging, (const views::View&), (const, override));
  MOCK_METHOD(void, OnAnimationEnded, (), (override));
};

class TestLayoutManager : public views::LayoutManagerBase {
 public:
  TestLayoutManager() = default;
  ~TestLayoutManager() override = default;

 protected:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override {
    views::ProposedLayout layout;

    // Place children at full width, starting from the top of the parent.
    int y = 0;
    for (const auto& child : host_view()->children()) {
      layout.child_layouts.emplace_back(child, child->GetVisible(),
                                        gfx::Rect(0, y, 100, 20));
      y += 20;
    }

    // Calculate the total height based on all child layouts.
    layout.host_size = gfx::Size(size_bounds.width().value_or(100), y);
    return layout;
  }
};

}  // namespace

class TabCollectionAnimatingLayoutManagerTest
    : public views::ViewsTestBase,
      public testing::WithParamInterface<bool> {
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
    layout_manager_delegate_ = std::make_unique<
        testing::NiceMock<MockAnimatingLayoutManagerDelegate>>();
    layout_manager_ = host_view_->SetLayoutManager(
        std::make_unique<TabCollectionAnimatingLayoutManager>(
            std::make_unique<TestLayoutManager>(),
            *layout_manager_delegate_.get()));
    widget_->Show();
  }
  void TearDown() override {
    layout_manager_ = nullptr;
    host_view_ = nullptr;
    widget_.reset();
    layout_manager_delegate_.reset();
    animation_mode_.reset();
    views::ViewsTestBase::TearDown();
  }

  bool IsAnimationDurationEnabled() const { return GetParam(); }

  TabCollectionAnimatingLayoutManager* layout_manager() {
    return layout_manager_;
  }
  MockAnimatingLayoutManagerDelegate* layout_manager_delegate() {
    return layout_manager_delegate_.get();
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
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::View> host_view_;
  raw_ptr<TabCollectionAnimatingLayoutManager> layout_manager_;
};

TEST_P(TabCollectionAnimatingLayoutManagerTest, AddChild) {
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

    // Height should start at 0 with empty bounds.
    EXPECT_EQ(child->height(), 0);
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
  EXPECT_EQ(child1->bounds(), gfx::Rect(0, 0, 100, 20));

  // Add another child, verify it also animates to target bounds.
  const auto* child2 = add_child_and_animate_to_target();
  EXPECT_EQ(child2->bounds(), gfx::Rect(0, 20, 100, 20));
}

TEST_P(TabCollectionAnimatingLayoutManagerTest,
       PreferredSizeDuringSwapAnimation) {
  // Use standard duration for frame-by-frame observation.
  gfx::ScopedAnimationDurationScaleMode normal_duration_mode(
      gfx::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // Setup the layout manager and initial child views.
  SetLayoutManager(std::make_unique<TabCollectionAnimatingLayoutManager>(
      std::make_unique<TestLayoutManager>(), *layout_manager_delegate(),
      TabCollectionAnimatingLayoutManager::AnimationAxis::kVertical,
      /*animate_host_size=*/true));

  widget()->SetBounds(gfx::Rect(0, 0, 100, 100));
  auto* const child1 =
      host_view()->AddChildView(std::make_unique<views::View>());
  auto* const child2 =
      host_view()->AddChildView(std::make_unique<views::View>());
  widget()->LayoutRootViewIfNecessary();

  // Advance time such that the initial add animation has time to complete.
  task_environment()->FastForwardBy(base::Seconds(1));

  const int expected_height = 40;

  // Swap tabs.
  host_view()->ReorderChildView(child2, 0);
  host_view()->InvalidateLayout();
  widget()->LayoutRootViewIfNecessary();

  // Ensures preferred size remains stable at 40dips during the swap animation.
  // The animation duration is typically 200ms, so we poll 20 times in 10ms
  // steps.
  for (int i = 0; i < 20; ++i) {
    task_environment()->FastForwardBy(base::Milliseconds(10));
    widget()->LayoutRootViewIfNecessary();

    ASSERT_TRUE(layout_manager()->is_animating());

    // Verify stability at every frame.
    EXPECT_EQ(layout_manager()->GetPreferredSize(host_view()).height(),
              expected_height);
  }

  // Advance time to allow the swap animation to reach its final state.
  task_environment()->FastForwardBy(base::Seconds(1));

  // Verify final state.
  EXPECT_FALSE(layout_manager()->is_animating());
  EXPECT_EQ(layout_manager()->GetPreferredSize(host_view()).height(),
            expected_height);
  EXPECT_EQ(child2->bounds(), gfx::Rect(0, 0, 100, 20));
  EXPECT_EQ(child1->bounds(), gfx::Rect(0, 20, 100, 20));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TabCollectionAnimatingLayoutManagerTest,
    testing::Bool(),
    [](const testing::TestParamInfo<
        TabCollectionAnimatingLayoutManagerTest::ParamType>& info) {
      return info.param ? "AnimationDurationEnabled"
                        : "AnimationDurationDisabled";
    });
