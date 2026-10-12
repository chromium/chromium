// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/email_verifier/email_verification_popup_view.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verification_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/email_verifier/email_verification_popup_view_impl.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace autofill {
namespace {

class EmailVerificationPopupViewTest : public ChromeViewsTestBase {
 public:
  EmailVerificationPopupViewTest() = default;
  ~EmailVerificationPopupViewTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    test_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);

    // Create a widget to host the parent widget.
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    auto* web_view =
        widget_->SetContentsView(std::make_unique<views::WebView>(&profile_));
    web_view->SetWebContents(test_web_contents_.get());
    widget_->Show();
  }

  void TearDown() override {
    widget_.reset();
    test_web_contents_.reset();
    ChromeViewsTestBase::TearDown();
  }

  content::WebContents* web_contents() { return test_web_contents_.get(); }

 protected:
  content::RenderViewHostTestEnabler test_render_host_factories_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  std::unique_ptr<views::Widget> widget_;
};

// Verifies that neither button receives initial focus upon appearance.
// For non-blocking popups, no button receives initial focus to prevent
// unexpected focus shifts and eliminate UI keyjacking vectors.
TEST_F(EmailVerificationPopupViewTest, NoInitialButtonFocus) {
  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());
  auto view = std::make_unique<EmailVerificationPopupViewImpl>(
      controller->GetWeakPtr(), widget_.get(),
      net::SchemefulSite(GURL("https://issuer.com")), u"user@example.com",
      base::DoNothing());

  views::MdTextButton* cancel_button = views::AsViewClass<views::MdTextButton>(
      view->GetViewByID(static_cast<int>(
          EmailVerificationPopupViewImpl::PopupViewId::kCancelButton)));
  ASSERT_THAT(cancel_button, testing::NotNull());

  views::MdTextButton* confirm_button = views::AsViewClass<views::MdTextButton>(
      view->GetViewByID(static_cast<int>(
          EmailVerificationPopupViewImpl::PopupViewId::kConfirmButton)));
  ASSERT_THAT(confirm_button, testing::NotNull());

  EXPECT_EQ(view->GetInitiallyFocusedView(), nullptr);
}

// Tests that ShowLoadingState() transitions the confirm button into its loading
// state with an active spinner, disables the cancel button, and keeps the
// confirm button's size and color while hiding its text.
TEST_F(EmailVerificationPopupViewTest, ShowLoadingStateRealView) {
  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());
  base::WeakPtr<EmailVerificationPopupViewImpl> view =
      EmailVerificationPopupViewImpl::Show(
          controller->GetWeakPtr(), widget_.get(),
          net::SchemefulSite(GURL("https://example.com")), u"user@example.com",
          base::DoNothing());

  ASSERT_TRUE(view);
  ASSERT_TRUE(view->confirm_button_for_testing());
  ASSERT_TRUE(view->cancel_button_for_testing());
  EXPECT_TRUE(view->confirm_button_for_testing()->GetEnabled());
  EXPECT_TRUE(view->cancel_button_for_testing()->GetEnabled());
  EXPECT_EQ(view->throbber_for_testing(), nullptr);

  const gfx::Size confirm_button_size =
      view->confirm_button_for_testing()->GetPreferredSize();

  view->ShowLoadingState();

  // The confirm button keeps its size and prominent color; only its label is
  // swapped out for the spinner.
  EXPECT_TRUE(view->confirm_button_for_testing()->GetEnabled());
  EXPECT_EQ(view->confirm_button_for_testing()->GetPreferredSize(),
            confirm_button_size);
  EXPECT_TRUE(view->confirm_button_for_testing()->GetText().empty());
  EXPECT_EQ(
      view->confirm_button_for_testing()
          ->GetViewAccessibility()
          .GetCachedName(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_EMAIL_VERIFIER_PROMPT_VERIFY));
  EXPECT_FALSE(view->cancel_button_for_testing()->GetEnabled());
  ASSERT_TRUE(view->throbber_for_testing());
  EXPECT_TRUE(view->throbber_for_testing()->GetVisible());
  EXPECT_EQ(view->throbber_for_testing()->GetColorId(),
            ui::kColorButtonForegroundProminent);

  view->GetWidget()->CloseNow();
}

}  // namespace
}  // namespace autofill
