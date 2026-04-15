// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/views/help_bubble_factory_views.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "components/user_education/views/help_bubble_view.h"
#include "components/user_education/views/help_bubble_views.h"
#include "components/user_education/views/help_bubble_views_test_util.h"
#include "components/user_education/views/view_subregion_anchor.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace user_education {

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestAnchorId);
}

class HelpBubbleFactoryViewsTest : public views::ViewsTestBase {
 public:
  HelpBubbleFactoryViewsTest() = default;
  ~HelpBubbleFactoryViewsTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = std::make_unique<test::TestThemedWidget>();
    widget_->Init(CreateParamsForTestWidget());
    contents_view_ = widget_->SetContentsView(std::make_unique<views::View>());
    contents_view_->SetLayoutManager(std::make_unique<views::FillLayout>());
    anchor_view_ =
        contents_view_->AddChildView(std::make_unique<views::View>());
    anchor_view_->SetProperty(views::kElementIdentifierKey, kTestElementId);
    views::test::WidgetVisibleWaiter waiter(widget_.get());
    widget_->Show();
    waiter.Wait();
    widget_->LayoutRootViewIfNecessary();
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<HelpBubble> CreateHelpBubble(
      views::View* view,
      base::RepeatingClosure button_callback) {
    HelpBubbleParams params;
    params.body_text = u"To X, do Y";
    params.arrow = HelpBubbleArrow::kTopRight;

    if (button_callback) {
      HelpBubbleButtonParams button_params;
      button_params.text = u"Go away";
      button_params.is_default = true;
      button_params.callback = std::move(button_callback);
      params.buttons.push_back(std::move(button_params));
    }

    views::TrackedElementViews* const element =
        views::ElementTrackerViews::GetInstance()->GetElementForView(view);
    CHECK(element);
    return factory_.CreateBubble(element, std::move(params));
  }

  test::TestHelpBubbleDelegate test_delegate_;
  HelpBubbleFactoryViews factory_{&test_delegate_};
  raw_ptr<views::View, DanglingUntriaged> contents_view_;
  raw_ptr<views::View, DanglingUntriaged> anchor_view_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(HelpBubbleFactoryViewsTest, TestShowHelpBubble) {
  auto help_bubble = CreateHelpBubble(anchor_view_.get(), base::DoNothing());
  ASSERT_TRUE(help_bubble);
}

TEST_F(HelpBubbleFactoryViewsTest, HelpBubbleDismissedOnAnchorHidden) {
  UNCALLED_MOCK_CALLBACK(HelpBubble::ClosedCallback, closed);

  auto help_bubble = CreateHelpBubble(anchor_view_.get(), base::DoNothing());
  auto subscription = help_bubble->AddOnCloseCallback(closed.Get());

  // Wait for the help bubble to close.
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  EXPECT_CALL(closed, Run)
      .WillOnce([&](HelpBubble* bubble, HelpBubble::CloseReason reason) {
        EXPECT_EQ(help_bubble.get(), bubble);
        EXPECT_EQ(HelpBubble::CloseReason::kAnchorHidden, reason);
        run_loop.Quit();
      });
  anchor_view_->SetVisible(false);
  run_loop.Run();
}

class HelpBubbleFactoryViewsSubregionAnchorTest
    : public HelpBubbleFactoryViewsTest {
 public:
  HelpBubbleFactoryViewsSubregionAnchorTest() = default;
  ~HelpBubbleFactoryViewsSubregionAnchorTest() override = default;

  void SetUp() override {
    HelpBubbleFactoryViewsTest::SetUp();
    anchor_ =
        std::make_unique<ViewSubregionAnchor>(kTestAnchorId, *anchor_view_);
  }

  void TearDown() override {
    anchor_.reset();
    HelpBubbleFactoryViewsTest::TearDown();
  }

 protected:
  void FlushEvents() {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  bool AnchorVisible() const {
    return ui::ElementTracker::GetElementTracker()->IsElementVisible(
        kTestAnchorId,
        views::ElementTrackerViews::GetContextForWidget(widget_.get()));
  }

  std::unique_ptr<HelpBubble> CreateHelpBubble(
      base::RepeatingClosure button_callback) {
    CHECK(AnchorVisible());

    HelpBubbleParams params;
    params.body_text = u"To X, do Y";
    params.arrow = HelpBubbleArrow::kTopRight;

    if (button_callback) {
      HelpBubbleButtonParams button_params;
      button_params.text = u"Go away";
      button_params.is_default = true;
      button_params.callback = std::move(button_callback);
      params.buttons.push_back(std::move(button_params));
    }

    return factory_.CreateBubble(anchor_.get(), std::move(params));
  }

  std::unique_ptr<ViewSubregionAnchor> anchor_;
};

TEST_F(HelpBubbleFactoryViewsSubregionAnchorTest,
       SyntheticAnchorVisibilityTracksView) {
  EXPECT_TRUE(AnchorVisible());
  anchor_view_->SetVisible(false);
  EXPECT_FALSE(AnchorVisible());
  anchor_view_->SetVisible(true);
  EXPECT_TRUE(AnchorVisible());
  contents_view_->SetVisible(false);
  EXPECT_FALSE(AnchorVisible());
  widget_->Hide();
  // Make sure there's nothing left to process on the hide here.
  FlushEvents();
  EXPECT_FALSE(AnchorVisible());
  contents_view_->SetVisible(true);
  EXPECT_FALSE(AnchorVisible());
  widget_->Show();
  EXPECT_TRUE(AnchorVisible());
}

TEST_F(HelpBubbleFactoryViewsSubregionAnchorTest,
       SyntheticAnchorVisibilityManuallyHidden) {
  // Verify basic hiding and un-hiding.
  EXPECT_TRUE(AnchorVisible());
  anchor_->SetHidden(true);
  EXPECT_FALSE(AnchorVisible());
  anchor_->SetHidden(false);
  EXPECT_TRUE(AnchorVisible());

  // Verify that the view becoming visible doesn't override hidden state.
  anchor_->SetHidden(true);
  anchor_view_->SetVisible(false);
  anchor_view_->SetVisible(true);
  EXPECT_FALSE(AnchorVisible());
  anchor_->SetHidden(false);
  EXPECT_TRUE(AnchorVisible());

  // Verify that un-hiding doesn't override view visibility state.
  anchor_view_->SetVisible(false);
  anchor_->SetHidden(true);
  anchor_->SetHidden(false);
  EXPECT_FALSE(AnchorVisible());
  anchor_view_->SetVisible(true);
  EXPECT_TRUE(AnchorVisible());

  // Verify that deleting the anchor causes it to hide.
  anchor_.reset();
  EXPECT_FALSE(AnchorVisible());
}

TEST_F(HelpBubbleFactoryViewsSubregionAnchorTest, SyntheticAnchorBounds) {
  anchor_->MaybeUpdateAnchor(gfx::Rect(1, 1, 1, 1));
  EXPECT_TRUE(
      anchor_view_->GetBoundsInScreen().Contains(anchor_->GetScreenBounds()));
  EXPECT_TRUE(
      widget_->GetWindowBoundsInScreen().Contains(anchor_->GetScreenBounds()));
}

TEST_F(HelpBubbleFactoryViewsSubregionAnchorTest, ShowBubbleOnSyntheticAnchor) {
  anchor_->MaybeUpdateAnchor(gfx::Rect(1, 1, 1, 1));
  auto help_bubble = CreateHelpBubble(base::DoNothing());
  ASSERT_TRUE(help_bubble);
  EXPECT_EQ(help_bubble->AsA<HelpBubbleViews>()
                ->bubble_view_for_testing()
                ->GetAnchorRect(),
            anchor_->GetScreenBounds());
  EXPECT_EQ(widget_->GetNativeView(), anchor_->GetNativeView());
}

TEST_F(HelpBubbleFactoryViewsSubregionAnchorTest,
       BubbleMovesWithSyntheticAnchor) {
  anchor_->MaybeUpdateAnchor(gfx::Rect(1, 1, 1, 1));
  auto help_bubble = CreateHelpBubble(base::DoNothing());
  auto* const help_bubble_view =
      help_bubble->AsA<HelpBubbleViews>()->bubble_view_for_testing();
  const gfx::Rect old_bounds = help_bubble_view->GetBoundsInScreen();
  anchor_->MaybeUpdateAnchor(gfx::Rect(20, 30, 13, 17));
  EXPECT_EQ(help_bubble_view->GetAnchorRect(), anchor_->GetScreenBounds());
  const gfx::Rect new_bounds = help_bubble_view->GetBoundsInScreen();
  EXPECT_GT(new_bounds.x(), old_bounds.x());
  EXPECT_GT(new_bounds.y(), old_bounds.y());
}

TEST_F(HelpBubbleFactoryViewsSubregionAnchorTest,
       HelpBubbleDismissedOnAnchorHidden) {
  UNCALLED_MOCK_CALLBACK(HelpBubble::ClosedCallback, closed);

  auto help_bubble = CreateHelpBubble(base::DoNothing());
  auto subscription = help_bubble->AddOnCloseCallback(closed.Get());

  // Wait for the help bubble to close.
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  EXPECT_CALL(closed, Run)
      .WillOnce([&](HelpBubble* bubble, HelpBubble::CloseReason reason) {
        EXPECT_EQ(help_bubble.get(), bubble);
        EXPECT_EQ(HelpBubble::CloseReason::kAnchorHidden, reason);
        run_loop.Quit();
      });
  anchor_view_->SetVisible(false);
  run_loop.Run();
}

}  // namespace user_education
