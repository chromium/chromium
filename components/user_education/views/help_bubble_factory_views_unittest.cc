// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/views/help_bubble_factory_views.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/views/help_bubble_view.h"
#include "components/user_education/views/help_bubble_views_test_util.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace user_education {

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementId);
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
    anchor_view_ = contents_view_->AddChildView(
        std::make_unique<views::StaticSizedView>(gfx::Size(20, 20)));
    anchor_view_->SetProperty(views::kElementIdentifierKey, kTestElementId);
    widget_->Show();
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

}  // namespace user_education
