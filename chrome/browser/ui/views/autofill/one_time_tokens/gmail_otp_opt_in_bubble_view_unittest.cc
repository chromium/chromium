// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/one_time_tokens/gmail_otp_opt_in_bubble_view.h"

#include <memory>
#include <string>

#include "base/test/mock_callback.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/test_event.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace autofill {
namespace {

constexpr std::u16string_view kTestEmail = u"elisa.g.beckett@gmail.com";

class GmailOtpOptInBubbleViewTest : public ChromeViewsTestBase {
 public:
  GmailOtpOptInBubbleViewTest() = default;
  ~GmailOtpOptInBubbleViewTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                     views::Widget::InitParams::TYPE_WINDOW);
    anchor_widget_ = std::make_unique<views::Widget>();
    anchor_widget_->Init(std::move(params));
    anchor_widget_->Show();
  }

  void TearDown() override {
    bubble_ = nullptr;
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void CreateAndShowBubble(
      base::RepeatingClosure link_callback = base::DoNothing()) {
    auto bubble = std::make_unique<GmailOtpOptInBubbleView>(
        views::BubbleAnchor(anchor_widget_->GetContentsView()),
        /*web_contents=*/nullptr, std::u16string(kTestEmail),
        std::move(link_callback));
    bubble_ = bubble.get();
    views::BubbleDialogDelegateView::CreateBubble(std::move(bubble))->Show();
  }

  GmailOtpOptInBubbleView* bubble() { return bubble_; }

 private:
  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<GmailOtpOptInBubbleView> bubble_ = nullptr;
};

TEST_F(GmailOtpOptInBubbleViewTest, RendersTitleHeaderAndButtons) {
  CreateAndShowBubble();

  ASSERT_NE(bubble()->GetBubbleFrameView()->GetHeaderViewForTesting(), nullptr);
  ASSERT_NE(bubble()->GetBubbleFrameView()->title(), nullptr);

  ASSERT_NE(bubble()->GetOkButton(), nullptr);
  EXPECT_EQ(
      bubble()->GetOkButton()->GetText(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_GMAIL_OTP_OPT_IN_TURN_ON_BUTTON));
  EXPECT_EQ(bubble()->GetOkButton()->GetProperty(views::kElementIdentifierKey),
            GmailOtpOptInBubbleView::kTurnOnButtonId);

  ASSERT_NE(bubble()->GetCancelButton(), nullptr);
  EXPECT_EQ(bubble()->GetCancelButton()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_GMAIL_OTP_OPT_IN_NO_THANKS_BUTTON));
  EXPECT_EQ(
      bubble()->GetCancelButton()->GetProperty(views::kElementIdentifierKey),
      GmailOtpOptInBubbleView::kNoThanksButtonId);

  ASSERT_NE(bubble()->GetBubbleFrameView()->close_button(), nullptr);
  EXPECT_EQ(bubble()->GetBubbleFrameView()->close_button()->GetProperty(
                views::kElementIdentifierKey),
            GmailOtpOptInBubbleView::kCloseButtonId);
}

TEST_F(GmailOtpOptInBubbleViewTest, FormatsDescriptionAndTriggersLinkCallback) {
  base::MockRepeatingClosure link_callback;
  CreateAndShowBubble(link_callback.Get());

  views::StyledLabel* styled_label = bubble()->GetDescriptionLabelForTesting();
  ASSERT_NE(styled_label, nullptr);
  const std::u16string link_text =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_GMAIL_OTP_OPT_IN_LEARN_MORE_LINK);
  EXPECT_EQ(
      styled_label->GetText(),
      l10n_util::GetStringFUTF16(IDS_AUTOFILL_GMAIL_OTP_OPT_IN_DESCRIPTION,
                                 std::u16string(kTestEmail), link_text));

  views::Link* link_view = styled_label->GetFirstLinkForTesting();
  ASSERT_NE(link_view, nullptr);

  EXPECT_CALL(link_callback, Run());
  link_view->OnKeyPressed(
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_SPACE, ui::EF_NONE));
}

}  // namespace
}  // namespace autofill
