// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/views/help_bubble_views.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "components/user_education/views/help_bubble_views_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace user_education {

// Note: base HelpBubbleViewsTest is found in help_bubble_view_unittest.cc

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kAnchorElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kCustomBubbleId);
}  // namespace

class HelpBubbleViewsCustomBubbleTest : public views::ViewsTestBase {
 public:
  HelpBubbleViewsCustomBubbleTest() = default;
  ~HelpBubbleViewsCustomBubbleTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = std::make_unique<test::TestThemedWidget>();
    widget_->Init(CreateParamsForTestWidget());
    contents_view_ = widget_->SetContentsView(std::make_unique<views::View>());
    contents_view_->SetLayoutManager(std::make_unique<views::FillLayout>());
    anchor_view_ = contents_view_->AddChildView(
        std::make_unique<views::StaticSizedView>(gfx::Size(20, 20)));
    anchor_view_->SetProperty(views::kElementIdentifierKey, kAnchorElementId);
    UNCALLED_MOCK_CALLBACK(ui::ElementTracker::Callback, shown);
    auto sub = ui::ElementTracker::GetElementTracker()
                   ->AddElementShownInAnyContextCallback(kAnchorElementId,
                                                         shown.Get());
    base::RunLoop run_loop;
    EXPECT_CALL(shown, Run).WillOnce([&](ui::TrackedElement*) {
      run_loop.Quit();
    });
    widget_->Show();
    run_loop.Run();

    CHECK(anchor_view_->GetProperty(views::kElementIdentifierKey));
  }

  void TearDown() override {
    CloseWidget();
    ViewsTestBase::TearDown();
  }

  void CloseWidget() {
    contents_view_ = nullptr;
    anchor_view_ = nullptr;
    widget_.reset();
  }

  void DeleteAnchor() {
    auto* const anchor = anchor_view_.get();
    anchor_view_ = nullptr;
    contents_view_->RemoveChildViewT(anchor);
  }

  views::BubbleDialogDelegateView* CreateBubble() {
    auto bubble = std::make_unique<views::BubbleDialogDelegateView>(
        anchor_view_, views::BubbleBorder::TOP_RIGHT);
    bubble->SetLayoutManager(std::make_unique<views::FillLayout>());
    bubble->SetProperty(views::kElementIdentifierKey, kCustomBubbleId);
    auto* const button =
        bubble->AddChildView(std::make_unique<views::LabelButton>());
    button->SetText(u"Button");
    auto* const result = bubble.get();
    auto* widget =
        views::BubbleDialogDelegateView::CreateBubble(std::move(bubble));
    widget->Show();
    return result;
  }

  views::View* anchor_view() const { return anchor_view_; }

  auto BuildHelpBubble(views::BubbleDialogDelegateView* bubble) {
    CHECK(bubble);
    return base::WrapUnique(new HelpBubbleViews(
        bubble, views::ElementTrackerViews::GetInstance()->GetElementForView(
                    anchor_view_)));
  }

 private:
  raw_ptr<views::View> contents_view_;
  raw_ptr<views::View> anchor_view_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(HelpBubbleViewsCustomBubbleTest, CreateHelpBubble) {
  auto* const bubble = CreateBubble();
  auto help_bubble = BuildHelpBubble(bubble);
  ASSERT_EQ(bubble, help_bubble->bubble_view_for_testing());
  ASSERT_EQ(bubble->GetWidget()->GetWindowBoundsInScreen(),
            help_bubble->GetBoundsInScreen());
  ASSERT_EQ(views::ElementTrackerViews::GetContextForView(bubble),
            help_bubble->GetContext());
  EXPECT_TRUE(help_bubble->is_open());
}

TEST_F(HelpBubbleViewsCustomBubbleTest, CloseHelpBubble) {
  auto* const bubble = CreateBubble();
  auto help_bubble = BuildHelpBubble(bubble);
  UNCALLED_MOCK_CALLBACK(ui::ElementTracker::Callback, hidden);
  auto sub =
      ui::ElementTracker::GetElementTracker()
          ->AddElementHiddenInAnyContextCallback(kCustomBubbleId, hidden.Get());
  base::RunLoop run_loop;
  EXPECT_CALL(hidden, Run).WillOnce([&](ui::TrackedElement*) {
    run_loop.Quit();
  });
  help_bubble->Close();
  run_loop.Run();
  EXPECT_FALSE(help_bubble->is_open());
  EXPECT_EQ(nullptr, help_bubble->bubble_view_for_testing());
}

TEST_F(HelpBubbleViewsCustomBubbleTest, CloseHelpBubbleWidget) {
  auto* const bubble = CreateBubble();
  auto help_bubble = BuildHelpBubble(bubble);
  UNCALLED_MOCK_CALLBACK(HelpBubble::ClosedCallback, closed);
  auto sub = help_bubble->AddOnCloseCallback(closed.Get());

  base::RunLoop run_loop;
  EXPECT_CALL(closed, Run).WillOnce([&](HelpBubble*, HelpBubble::CloseReason) {
    run_loop.Quit();
  });
  bubble->GetWidget()->Close();
  run_loop.Run();

  EXPECT_FALSE(help_bubble->is_open());
  EXPECT_EQ(nullptr, help_bubble->bubble_view_for_testing());
}

TEST_F(HelpBubbleViewsCustomBubbleTest, AnchorViewHidden) {
  auto* const bubble = CreateBubble();
  auto help_bubble = BuildHelpBubble(bubble);
  UNCALLED_MOCK_CALLBACK(HelpBubble::ClosedCallback, closed);
  auto sub = help_bubble->AddOnCloseCallback(closed.Get());

  base::RunLoop run_loop;
  EXPECT_CALL(closed, Run).WillOnce([&](HelpBubble*, HelpBubble::CloseReason) {
    run_loop.Quit();
  });
  anchor_view()->SetVisible(false);
  run_loop.Run();

  EXPECT_FALSE(help_bubble->is_open());
  EXPECT_EQ(nullptr, help_bubble->bubble_view_for_testing());
}

}  // namespace user_education
