// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_onboarding_bubble_view.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace contextual_tasks {
namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestAnchorElementId);

using Variant = ContextualTasksOnboardingBubbleView::Variant;

class ContextualTasksOnboardingBubbleViewTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    anchor_widget_->Show();
  }

  void TearDown() override {
    bubble_widget_.reset();
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  views::Widget* CreateAndShowBubble(
      Variant variant,
      base::OnceClosure on_accept = base::DoNothing(),
      base::RepeatingClosure on_learn_more = base::DoNothing()) {
    bubble_widget_ = ContextualTasksOnboardingBubbleView::CreateAndShow(
        views::BubbleAnchor(anchor_widget_->GetContentsView()), variant,
        std::move(on_accept), std::move(on_learn_more),
        anchor_widget_->GetNativeView());
    return bubble_widget_.get();
  }

  ContextualTasksOnboardingBubbleView* GetBubbleView() {
    return bubble_widget_
               ? views::AsViewClass<ContextualTasksOnboardingBubbleView>(
                     bubble_widget_->widget_delegate()->GetContentsView())
               : nullptr;
  }

 protected:
  std::unique_ptr<views::Widget> anchor_widget_;
  std::unique_ptr<views::Widget> bubble_widget_;
};

TEST_F(ContextualTasksOnboardingBubbleViewTest, OnboardingVariantProperties) {
  CreateAndShowBubble(Variant::kOnboarding);
  auto* view = GetBubbleView();
  ASSERT_NE(view, nullptr);
  EXPECT_EQ(view->variant_for_testing(), Variant::kOnboarding);
  EXPECT_EQ(view->title_label_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_CONTEXTUAL_TASKS_FIRST_RUN_EXPERIENCE_TITLE));
  EXPECT_NE(
      view->body_label_for_testing()->GetText().find(l10n_util::GetStringUTF16(
          IDS_CONTEXTUAL_TASKS_FIRST_RUN_EXPERIENCE_DESCRIPTION)),
      std::u16string::npos);
  EXPECT_NE(
      view->body_label_for_testing()->GetText().find(l10n_util::GetStringUTF16(
          IDS_CONTEXTUAL_TASKS_FIRST_RUN_EXPERIENCE_LEARN_MORE)),
      std::u16string::npos);
  EXPECT_EQ(view->accept_button_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_CONTEXTUAL_TASKS_FIRST_RUN_EXPERIENCE_ACCEPT_BUTTON));

  auto* delegate = bubble_widget_->widget_delegate()->AsBubbleDialogDelegate();
  ASSERT_NE(delegate, nullptr);
  EXPECT_EQ(delegate->background_color(), SK_ColorWHITE);
  EXPECT_EQ(delegate->fixed_width(), 288);
  EXPECT_EQ(delegate->margins(), gfx::Insets(16));
  EXPECT_EQ(delegate->GetCornerRadius(), 8);
  EXPECT_FALSE(delegate->adjust_if_offscreen());
  auto* frame_view = static_cast<views::BubbleFrameView*>(
      bubble_widget_->non_client_view()->frame_view());
  ASSERT_NE(frame_view, nullptr);
  EXPECT_TRUE(frame_view->GetDisplayVisibleArrow());
}

TEST_F(ContextualTasksOnboardingBubbleViewTest, AskGoogleVariantProperties) {
  CreateAndShowBubble(Variant::kAskGoogle);
  auto* view = GetBubbleView();
  ASSERT_NE(view, nullptr);
  EXPECT_EQ(view->variant_for_testing(), Variant::kAskGoogle);
  EXPECT_EQ(view->title_label_for_testing()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_CONTEXTUAL_TASKS_FIRST_RUN_EXPERIENCE_SHORT_TITLE));
  EXPECT_NE(
      view->body_label_for_testing()->GetText().find(l10n_util::GetStringUTF16(
          IDS_LENS_COBROWSE_CURRENT_TAB_IPH_DESCRIPTION)),
      std::u16string::npos);
  EXPECT_NE(view->body_label_for_testing()->GetText().find(
                l10n_util::GetStringUTF16(IDS_LEARN_MORE)),
            std::u16string::npos);
}

TEST_F(ContextualTasksOnboardingBubbleViewTest, ElementIdentifiersRegistered) {
  CreateAndShowBubble(Variant::kOnboarding);
  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(bubble_widget_.get());
  for (ui::ElementIdentifier id :
       {ContextualTasksOnboardingBubbleView::kBubbleElementId,
        ContextualTasksOnboardingBubbleView::kTitleElementId,
        ContextualTasksOnboardingBubbleView::kBodyElementId,
        ContextualTasksOnboardingBubbleView::kAcceptButtonElementId}) {
    EXPECT_NE(views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
                  id, context),
              nullptr);
  }
}

TEST_F(ContextualTasksOnboardingBubbleViewTest,
       AcceptButtonClickTriggersCallbackAndCloses) {
  bool accepted = false;
  CreateAndShowBubble(
      Variant::kOnboarding,
      base::BindLambdaForTesting([&accepted]() { accepted = true; }));
  auto* view = GetBubbleView();
  ASSERT_NE(view, nullptr);
  views::test::WidgetDestroyedWaiter close_waiter(bubble_widget_.get());
  views::test::ButtonTestApi(view->accept_button_for_testing())
      .NotifyDefaultMouseClick();
  EXPECT_TRUE(accepted);
  EXPECT_TRUE(bubble_widget_->IsClosed());
  close_waiter.Wait();
}

TEST_F(ContextualTasksOnboardingBubbleViewTest,
       LearnMoreClickTriggersCallback) {
  int learn_more_clicks = 0;
  CreateAndShowBubble(Variant::kOnboarding, base::DoNothing(),
                      base::BindLambdaForTesting(
                          [&learn_more_clicks]() { ++learn_more_clicks; }));
  auto* view = GetBubbleView();
  ASSERT_NE(view, nullptr);
  view->body_label_for_testing()->ClickFirstLinkForTesting();
  EXPECT_EQ(learn_more_clicks, 1);
}

TEST_F(ContextualTasksOnboardingBubbleViewTest,
       TrackedElementAnchorHiddenClearsAnchorAndCloses) {
  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(anchor_widget_.get());
  auto anchor_element =
      std::make_unique<ui::test::TestElement>(kTestAnchorElementId, context);
  anchor_element->SetNativeView(anchor_widget_->GetNativeView());
  anchor_element->SetScreenBounds(gfx::Rect(10, 10, 40, 20));
  anchor_element->Show();

  bubble_widget_ = ContextualTasksOnboardingBubbleView::CreateAndShow(
      views::BubbleAnchor(anchor_element.get()), Variant::kOnboarding,
      base::DoNothing(), base::DoNothing(), anchor_widget_->GetNativeView());
  ASSERT_NE(bubble_widget_, nullptr);
  auto* delegate = bubble_widget_->widget_delegate()->AsBubbleDialogDelegate();
  ASSERT_NE(delegate, nullptr);
  EXPECT_EQ(delegate->GetAnchor().GetIfElement(), anchor_element.get());

  views::test::WidgetDestroyedWaiter close_waiter(bubble_widget_.get());
  anchor_element->Hide();
  anchor_element.reset();
  EXPECT_TRUE(delegate->GetAnchor().IsNull());
  EXPECT_TRUE(bubble_widget_->IsClosed());
  close_waiter.Wait();
}

}  // namespace
}  // namespace contextual_tasks
