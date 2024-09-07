// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_actions_bar_bubble_views.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/toolbar/test_toolbar_actions_bar_bubble_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_bubble_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/test/test_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace {

const int kIconSize = 16;

const char16_t kHeadingText[] = u"Heading";
const char16_t kBodyText[] = u"Body";
const char16_t kActionButtonText[] = u"Action";
const char16_t kDismissButtonText[] = u"Dismiss";
const char16_t kExtraViewText[] = u"Learn";

}  // namespace

class ToolbarActionsBarBubbleViewsTest : public ChromeViewsTestBase {
 protected:
  ToolbarActionsBarBubbleViewsTest() {}
  ToolbarActionsBarBubbleViewsTest(const ToolbarActionsBarBubbleViewsTest&) =
      delete;
  ToolbarActionsBarBubbleViewsTest& operator=(
      const ToolbarActionsBarBubbleViewsTest&) = delete;
  ~ToolbarActionsBarBubbleViewsTest() override = default;

  void TearDown() override {
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  std::unique_ptr<views::Widget> CreateAnchorWidget() {
    std::unique_ptr<views::Widget> anchor_widget =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                         views::Widget::InitParams::TYPE_WINDOW);
    anchor_widget->Show();
    return anchor_widget;
  }

  void ShowBubble(TestToolbarActionsBarBubbleDelegate* delegate) {
    ASSERT_TRUE(delegate);
    ASSERT_FALSE(bubble_widget_);
    ASSERT_FALSE(bubble_);
    anchor_widget_ = CreateAnchorWidget();
    bool anchored_to_action = false;
    bubble_ = new ToolbarActionsBarBubbleViews(
        anchor_widget_->GetContentsView(), anchored_to_action,
        delegate->GetDelegate());
    bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_);
    bubble_widget_->Show();
  }

  void CloseBubble() {
    ASSERT_TRUE(bubble_);
    bubble_->GetWidget()->Close();
    base::RunLoop().RunUntilIdle();
    bubble_ = nullptr;
    bubble_widget_ = nullptr;
  }

  void ClickButton(views::Button* button) {
    bubble()->ResetViewShownTimeStampForTesting();

    ASSERT_TRUE(button);
    const gfx::Point point(10, 10);
    const ui::MouseEvent event(ui::EventType::kMousePressed, point, point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
    button->OnMousePressed(event);
    button->OnMouseReleased(event);
    base::RunLoop().RunUntilIdle();
  }

  views::Widget* anchor_widget() { return anchor_widget_.get(); }
  views::Widget* bubble_widget() { return bubble_widget_; }
  ToolbarActionsBarBubbleViews* bubble() { return bubble_; }

 private:
  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<views::Widget, DanglingUntriaged> bubble_widget_ = nullptr;
  raw_ptr<ToolbarActionsBarBubbleViews, DanglingUntriaged> bubble_ = nullptr;
};

TEST_F(ToolbarActionsBarBubbleViewsTest, TestBubbleLayoutActionButton) {
  TestToolbarActionsBarBubbleDelegate delegate(kHeadingText, kBodyText,
                                               kActionButtonText);
  ShowBubble(&delegate);

  EXPECT_TRUE(bubble()->GetOkButton());
  EXPECT_EQ(bubble()->GetOkButton()->GetText(), kActionButtonText);
  EXPECT_FALSE(bubble()->GetCancelButton());

  CloseBubble();
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestBubbleLayoutNoButtons) {
  TestToolbarActionsBarBubbleDelegate delegate(kHeadingText, kBodyText);
  ShowBubble(&delegate);

  EXPECT_EQ(bubble()->GetExtraView(), nullptr);
  EXPECT_FALSE(bubble()->GetOkButton());
  EXPECT_FALSE(bubble()->GetCancelButton());
  EXPECT_FALSE(bubble()->learn_more_button());

  CloseBubble();
}

TEST_F(ToolbarActionsBarBubbleViewsTest,
       TestBubbleLayoutActionAndDismissButton) {
  TestToolbarActionsBarBubbleDelegate delegate(
      kHeadingText, kBodyText, kActionButtonText, kDismissButtonText);
  ShowBubble(&delegate);

  EXPECT_TRUE(bubble()->GetOkButton());
  EXPECT_EQ(bubble()->GetOkButton()->GetText(), kActionButtonText);
  EXPECT_TRUE(bubble()->GetCancelButton());
  EXPECT_EQ(bubble()->GetCancelButton()->GetText(), kDismissButtonText);

  EXPECT_FALSE(bubble()->learn_more_button());
  EXPECT_FALSE(bubble()->item_list());

  CloseBubble();
}

TEST_F(ToolbarActionsBarBubbleViewsTest,
       TestBubbleLayoutActionDismissAndLearnMoreButton) {
  TestToolbarActionsBarBubbleDelegate delegate(
      kHeadingText, kBodyText, kActionButtonText, kDismissButtonText);
  auto extra_view_info =
      std::make_unique<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>();
  extra_view_info->text = kExtraViewText;
  extra_view_info->is_learn_more = true;
  delegate.set_extra_view_info(std::move(extra_view_info));

  ShowBubble(&delegate);

  EXPECT_TRUE(bubble()->GetOkButton());
  EXPECT_EQ(bubble()->GetOkButton()->GetText(), kActionButtonText);
  EXPECT_TRUE(bubble()->GetCancelButton());
  EXPECT_EQ(bubble()->GetCancelButton()->GetText(), kDismissButtonText);
  EXPECT_TRUE(bubble()->learn_more_button());
  EXPECT_EQ(bubble()->learn_more_button()->GetTooltipText(gfx::Point(0, 0)),
            kExtraViewText);
  EXPECT_FALSE(bubble()->item_list());

  CloseBubble();
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestBubbleLayoutNoBodyText) {
  TestToolbarActionsBarBubbleDelegate delegate(kHeadingText, std::u16string(),
                                               kActionButtonText);
  ShowBubble(&delegate);

  EXPECT_TRUE(bubble()->GetOkButton());
  EXPECT_EQ(bubble()->GetOkButton()->GetText(), kActionButtonText);
  EXPECT_FALSE(bubble()->GetCancelButton());
  EXPECT_FALSE(bubble()->learn_more_button());
  EXPECT_FALSE(bubble()->body_text());
  EXPECT_FALSE(bubble()->item_list());

  CloseBubble();
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestBubbleDefaultDialogButtons) {
  TestToolbarActionsBarBubbleDelegate delegate(
      kHeadingText, kBodyText, kActionButtonText, kDismissButtonText);
  delegate.set_default_dialog_button(ui::mojom::DialogButton::kOk);
  ShowBubble(&delegate);

  ASSERT_TRUE(bubble()->GetOkButton());
  EXPECT_TRUE(bubble()->GetOkButton()->GetIsDefault());

  ASSERT_TRUE(bubble()->GetCancelButton());
  EXPECT_FALSE(bubble()->GetCancelButton()->GetIsDefault());

  CloseBubble();
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestShowAndCloseBubble) {
  std::unique_ptr<views::Widget> anchor_widget = CreateAnchorWidget();
  TestToolbarActionsBarBubbleDelegate delegate(
      kHeadingText, kBodyText, kActionButtonText, kDismissButtonText);
  ToolbarActionsBarBubbleViews* bubble = new ToolbarActionsBarBubbleViews(
      anchor_widget->GetContentsView(), false, delegate.GetDelegate());

  EXPECT_FALSE(delegate.shown());
  EXPECT_FALSE(delegate.close_action());
  views::Widget* bubble_widget =
      views::BubbleDialogDelegateView::CreateBubble(bubble);
  views::test::TestWidgetObserver bubble_observer(bubble_widget);
  bubble_widget->Show();
  EXPECT_TRUE(delegate.shown());
  EXPECT_FALSE(delegate.close_action());

  bubble->CancelDialog();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.close_action());
  EXPECT_EQ(ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_USER_ACTION,
            *delegate.close_action());
  EXPECT_TRUE(bubble_observer.widget_closed());
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestClickActionButton) {
  TestToolbarActionsBarBubbleDelegate delegate(
      kHeadingText, kBodyText, kActionButtonText, kDismissButtonText);
  delegate.set_learn_more_button_text(kExtraViewText);
  ShowBubble(&delegate);
  views::test::TestWidgetObserver bubble_observer(bubble_widget());

  EXPECT_FALSE(delegate.close_action());

  ClickButton(bubble()->GetOkButton());
  ASSERT_TRUE(delegate.close_action());
  EXPECT_EQ(ToolbarActionsBarBubbleDelegate::CLOSE_EXECUTE,
            *delegate.close_action());
  EXPECT_TRUE(bubble_observer.widget_closed());
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestClickDismissButton) {
  TestToolbarActionsBarBubbleDelegate delegate(
      kHeadingText, kBodyText, kActionButtonText, kDismissButtonText);
  delegate.set_learn_more_button_text(kExtraViewText);
  ShowBubble(&delegate);
  views::test::TestWidgetObserver bubble_observer(bubble_widget());

  EXPECT_FALSE(delegate.close_action());

  ClickButton(bubble()->GetCancelButton());
  ASSERT_TRUE(delegate.close_action());
  EXPECT_EQ(ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_USER_ACTION,
            *delegate.close_action());
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestClickLearnMoreLink) {
  TestToolbarActionsBarBubbleDelegate delegate(
      kHeadingText, kBodyText, kActionButtonText, kDismissButtonText);
  delegate.set_learn_more_button_text(kExtraViewText);
  ShowBubble(&delegate);
  views::test::TestWidgetObserver bubble_observer(bubble_widget());

  EXPECT_FALSE(delegate.close_action());

  ClickButton(bubble()->learn_more_button());
  ASSERT_TRUE(delegate.close_action());
  EXPECT_EQ(ToolbarActionsBarBubbleDelegate::CLOSE_LEARN_MORE,
            *delegate.close_action());
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestCloseOnDeactivation) {
  TestToolbarActionsBarBubbleDelegate delegate(kHeadingText, kBodyText,
                                               kActionButtonText);
  ShowBubble(&delegate);
  views::test::TestWidgetObserver bubble_observer(bubble_widget());

  EXPECT_FALSE(delegate.close_action());
  // Close the bubble by activating another widget. The delegate should be
  // told it was dismissed.
  anchor_widget()->Activate();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(delegate.close_action());
  EXPECT_EQ(ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_DEACTIVATION,
            *delegate.close_action());
  EXPECT_TRUE(bubble_observer.widget_closed());
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestNullExtraView) {
  TestToolbarActionsBarBubbleDelegate delegate(kHeadingText, kBodyText,
                                               kActionButtonText);
  ShowBubble(&delegate);
  EXPECT_EQ(nullptr, bubble()->GetExtraView());
  CloseBubble();
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestCreateExtraViewIconOnly) {
  TestToolbarActionsBarBubbleDelegate delegate(kHeadingText, kBodyText,
                                               kActionButtonText);
  auto extra_view_info =
      std::make_unique<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>();
  extra_view_info->resource = &vector_icons::kBusinessIcon;
  delegate.set_extra_view_info(std::move(extra_view_info));
  ShowBubble(&delegate);
  const views::View* const extra_view = bubble()->GetExtraView();
  ASSERT_TRUE(extra_view);
  ASSERT_EQ(std::string(extra_view->GetClassName()), "ImageView");
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(static_cast<const views::ImageView*>(extra_view)->GetImage()),
      gfx::Image(gfx::CreateVectorIcon(
          vector_icons::kBusinessIcon, kIconSize,
          extra_view->GetColorProvider()->GetColor(ui::kColorIcon)))));
  CloseBubble();
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestCreateExtraViewLinkedTextOnly) {
  TestToolbarActionsBarBubbleDelegate delegate(kHeadingText, kBodyText,
                                               kActionButtonText);
  auto extra_view_info =
      std::make_unique<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>();
  extra_view_info->text = kExtraViewText;
  extra_view_info->is_learn_more = true;
  delegate.set_extra_view_info(std::move(extra_view_info));

  ShowBubble(&delegate);

  const views::View* const extra_view = bubble()->GetExtraView();
  ASSERT_TRUE(extra_view);
  ASSERT_EQ(std::string(extra_view->GetClassName()), "ImageButton");
  EXPECT_EQ(extra_view->GetTooltipText(gfx::Point(0, 0)), kExtraViewText);
  CloseBubble();
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestCreateExtraViewLabelTextOnly) {
  TestToolbarActionsBarBubbleDelegate delegate(kHeadingText, kBodyText,
                                               kActionButtonText);
  auto extra_view_info =
      std::make_unique<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>();
  extra_view_info->text = kExtraViewText;
  extra_view_info->is_learn_more = false;
  delegate.set_extra_view_info(std::move(extra_view_info));

  ShowBubble(&delegate);

  const views::View* const extra_view = bubble()->GetExtraView();
  ASSERT_TRUE(extra_view);
  EXPECT_EQ(std::string(extra_view->GetClassName()), "Label");
  EXPECT_EQ(static_cast<const views::Label*>(extra_view)->GetText(),
            kExtraViewText);
  CloseBubble();
}

TEST_F(ToolbarActionsBarBubbleViewsTest, TestCreateExtraViewImageAndText) {
  TestToolbarActionsBarBubbleDelegate delegate(kHeadingText, kBodyText,
                                               kActionButtonText);
  auto extra_view_info =
      std::make_unique<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>();
  extra_view_info->resource = &vector_icons::kBusinessIcon;
  extra_view_info->text = kExtraViewText;
  extra_view_info->is_learn_more = false;
  delegate.set_extra_view_info(std::move(extra_view_info));

  ShowBubble(&delegate);

  const views::View* const extra_view = bubble()->GetExtraView();
  ASSERT_TRUE(extra_view);
  EXPECT_STREQ(extra_view->GetClassName(), "View");
  EXPECT_EQ(extra_view->children().size(), 2u);

  for (const views::View* v : extra_view->children()) {
    std::string class_name = v->GetClassName();
    if (class_name == "Label") {
      EXPECT_EQ(static_cast<const views::Label*>(v)->GetText(), kExtraViewText);
    } else {
      ASSERT_EQ(class_name, "ImageView");
      EXPECT_TRUE(gfx::test::AreImagesEqual(
          gfx::Image(static_cast<const views::ImageView*>(v)->GetImage()),
          gfx::Image(gfx::CreateVectorIcon(
              vector_icons::kBusinessIcon, kIconSize,
              v->GetColorProvider()->GetColor(ui::kColorIcon)))));
    }
  }

  CloseBubble();
}
