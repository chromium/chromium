// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/email_verifier/email_verification_popup_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
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
      url::Origin::Create(GURL("https://issuer.com")), gfx::Image(),
      u"user@example.com", base::DoNothing());

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
          url::Origin::Create(GURL("https://example.com")), gfx::Image(),
          u"user@example.com", base::DoNothing());

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

// An `EmailVerificationPopupController::ViewFactoryForTesting` that shows a
// real popup view in `parent_widget` and stores it in `view_out`. The
// controller's decision callback is stored in `decision_callback_out` instead
// of being passed to the view, so that tests can confirm the popup directly.
base::WeakPtr<EmailVerificationPopupView> ShowPopupViewForTesting(
    views::Widget* parent_widget,
    base::WeakPtr<EmailVerificationPopupViewImpl>* view_out,
    base::OnceCallback<void(bool)>* decision_callback_out,
    base::WeakPtr<EmailVerificationPopupController> controller,
    views::Widget* /*web_contents_widget*/,
    const url::Origin& issuer_origin,
    const gfx::Image& issuer_logo,
    const std::u16string& email,
    base::OnceCallback<void(bool)> decision_callback) {
  *decision_callback_out = std::move(decision_callback);
  *view_out = EmailVerificationPopupViewImpl::Show(controller, parent_widget,
                                                   issuer_origin, issuer_logo,
                                                   email, base::DoNothing());
  return *view_out;
}

// Tests that `IsActive()` reports whether the popup's widget is active. The
// popup takes activation when it's shown, and loses it when another widget is
// activated.
TEST_F(EmailVerificationPopupViewTest, IsActiveReflectsWidgetActivation) {
  EmailVerificationPopupController controller(web_contents());
  base::WeakPtr<EmailVerificationPopupViewImpl> view;
  base::OnceCallback<void(bool)> decision_callback;
  controller.set_view_factory_for_testing(base::BindRepeating(
      &ShowPopupViewForTesting, widget_.get(), &view, &decision_callback));
  // The popup closes right away if the element it's anchored to is outside of
  // the web contents, which isn't at the screen origin on every platform.
  controller.Show(gfx::RectF(10, 10, 100, 20) +
                      web_contents()->GetContainerBounds().OffsetFromOrigin(),
                  url::Origin::Create(GURL("https://issuer.com")), gfx::Image(),
                  u"user@example.com", base::DoNothing());
  ASSERT_TRUE(view);
  EXPECT_TRUE(view->IsActive());

  // Confirm the popup so that it enters the loading state, in which it stays
  // open when it loses focus.
  std::move(decision_callback).Run(/*confirmed=*/true);
  ASSERT_TRUE(controller.is_loading());

  widget_->Activate();
  ASSERT_TRUE(view);
  EXPECT_FALSE(view->IsActive());

  view->GetWidget()->CloseNow();
}

// Tests that the issuer logo is shown in the 20 DIP headline icon, and that a
// 40px logo provides a full-resolution 2x representation instead of being
// upscaled from 1x.
TEST_F(EmailVerificationPopupViewTest, ShowsIssuerLogo) {
  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());
  auto view = std::make_unique<EmailVerificationPopupViewImpl>(
      controller->GetWeakPtr(), widget_.get(),
      url::Origin::Create(GURL("https://accounts.issuer.com")),
      gfx::test::CreateImage(40, 40), u"user@example.com", base::DoNothing());

  ASSERT_TRUE(view->icon_view_for_testing());
  const ui::ImageModel& image_model =
      view->icon_view_for_testing()->GetImageModel();
  ASSERT_TRUE(image_model.IsImage());
  EXPECT_EQ(image_model.Size(), gfx::Size(20, 20));
  const gfx::ImageSkia image = image_model.GetImage().AsImageSkia();
  EXPECT_EQ(image.GetRepresentation(1.0f).pixel_size(), gfx::Size(20, 20));
  EXPECT_EQ(image.GetRepresentation(2.0f).pixel_size(), gfx::Size(40, 40));
}

// Tests that the generic email icon is shown if the issuer has no logo.
TEST_F(EmailVerificationPopupViewTest, ShowsGenericIconWithoutLogo) {
  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());
  auto view = std::make_unique<EmailVerificationPopupViewImpl>(
      controller->GetWeakPtr(), widget_.get(),
      url::Origin::Create(GURL("https://accounts.issuer.com")), gfx::Image(),
      u"user@example.com", base::DoNothing());

  ASSERT_TRUE(view->icon_view_for_testing());
  EXPECT_TRUE(view->icon_view_for_testing()->GetImageModel().IsVectorIcon());
}

// Tests that a non-square logo is not shown, since it would be distorted.
TEST_F(EmailVerificationPopupViewTest, ShowsGenericIconForNonSquareLogo) {
  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());
  auto view = std::make_unique<EmailVerificationPopupViewImpl>(
      controller->GetWeakPtr(), widget_.get(),
      url::Origin::Create(GURL("https://accounts.issuer.com")),
      gfx::test::CreateImage(40, 20), u"user@example.com", base::DoNothing());

  ASSERT_TRUE(view->icon_view_for_testing());
  EXPECT_TRUE(view->icon_view_for_testing()->GetImageModel().IsVectorIcon());
}

// Tests that a square logo smaller than the 20 DIP headline icon is not shown,
// since it would be upscaled.
TEST_F(EmailVerificationPopupViewTest, ShowsGenericIconForUndersizedLogo) {
  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());
  auto view = std::make_unique<EmailVerificationPopupViewImpl>(
      controller->GetWeakPtr(), widget_.get(),
      url::Origin::Create(GURL("https://accounts.issuer.com")),
      gfx::test::CreateImage(16, 16), u"user@example.com", base::DoNothing());

  ASSERT_TRUE(view->icon_view_for_testing());
  EXPECT_TRUE(view->icon_view_for_testing()->GetImageModel().IsVectorIcon());
}

// Tests that a logo larger than the 1024 px limit is not shown, since it would
// be resized on the UI thread and kept in memory.
TEST_F(EmailVerificationPopupViewTest, ShowsGenericIconForOversizedLogo) {
  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());
  auto view = std::make_unique<EmailVerificationPopupViewImpl>(
      controller->GetWeakPtr(), widget_.get(),
      url::Origin::Create(GURL("https://accounts.issuer.com")),
      gfx::test::CreateImage(1025, 1025), u"user@example.com",
      base::DoNothing());

  ASSERT_TRUE(view->icon_view_for_testing());
  EXPECT_TRUE(view->icon_view_for_testing()->GetImageModel().IsVectorIcon());
}

// Tests that a logo of exactly the 20 DIP headline icon size is tagged as a 1x
// representation instead of an unscaled one, so that a HiDPI display resamples
// it instead of painting the 1x bitmap as is.
TEST_F(EmailVerificationPopupViewTest, IconSizedIssuerLogoIsNotUnscaled) {
  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());
  auto view = std::make_unique<EmailVerificationPopupViewImpl>(
      controller->GetWeakPtr(), widget_.get(),
      url::Origin::Create(GURL("https://accounts.issuer.com")),
      gfx::test::CreateImage(20, 20), u"user@example.com", base::DoNothing());

  ASSERT_TRUE(view->icon_view_for_testing());
  const ui::ImageModel& image_model =
      view->icon_view_for_testing()->GetImageModel();
  ASSERT_TRUE(image_model.IsImage());
  EXPECT_FALSE(
      image_model.GetImage().AsImageSkia().GetRepresentation(2.0f).unscaled());
}

}  // namespace
}  // namespace autofill
