// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/email_verifier/email_verification_popup_view.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/test/gmock_move_support.h"
#include "chrome/browser/favicon/large_icon_service_factory.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verification_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/email_verifier/email_verification_popup_view_impl.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon/core/test/mock_large_icon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {
namespace {

using ::favicon::MockLargeIconService;
using ::testing::_;

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
      url::Origin::Create(GURL("https://issuer.com")), u"user@example.com",
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
          url::Origin::Create(GURL("https://example.com")), u"user@example.com",
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

// Tests that the popup requests the favicon using the full `issuer_origin` URL
// (preserving subdomains) via `GetIconRawBitmapOrFallbackStyleForPageUrl` and
// updates the headline icon when the bitmap loads.
TEST_F(EmailVerificationPopupViewTest, LoadsFaviconUsingIssuerOrigin) {
  auto* mock_large_icon_service = static_cast<MockLargeIconService*>(
      LargeIconServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          &profile_,
          base::BindRepeating(
              [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
                return std::make_unique<MockLargeIconService>();
              })));

  favicon_base::LargeIconCallback saved_callback;
  EXPECT_CALL(*mock_large_icon_service,
              GetIconRawBitmapOrFallbackStyleForPageUrl(
                  GURL("https://accounts.issuer.com/"), 20, _, _))
      .WillOnce(MoveArgAndReturn<2>(&saved_callback,
                                    base::CancelableTaskTracker::kBadTaskId));

  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());
  auto view = std::make_unique<EmailVerificationPopupViewImpl>(
      controller->GetWeakPtr(), widget_.get(),
      url::Origin::Create(GURL("https://accounts.issuer.com")),
      u"user@example.com", base::DoNothing());

  ASSERT_TRUE(view->icon_view_for_testing());
  EXPECT_TRUE(view->icon_view_for_testing()->GetImageModel().IsVectorIcon());
  ASSERT_TRUE(saved_callback);

  gfx::Image test_image = gfx::test::CreateImage(20, 20);
  std::optional<std::vector<uint8_t>> encoded =
      gfx::PNGCodec::EncodeBGRASkBitmap(*test_image.ToSkBitmap(),
                                        /*discard_transparency=*/false);
  favicon_base::FaviconRawBitmapResult bitmap_result;
  bitmap_result.bitmap_data =
      base::MakeRefCounted<base::RefCountedBytes>(std::move(encoded.value()));
  bitmap_result.pixel_size = gfx::Size(20, 20);
  bitmap_result.icon_url = GURL("https://accounts.issuer.com/favicon.ico");

  std::move(saved_callback).Run(favicon_base::LargeIconResult(bitmap_result));
  EXPECT_TRUE(view->icon_view_for_testing()->GetImageModel().IsImage());
}

}  // namespace
}  // namespace autofill
