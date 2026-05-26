// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/email_verification_popup_view.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/autofill/email_verification_popup_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

namespace autofill {
namespace {

using ::base::test::TestFuture;

class MockEmailVerificationPopupView : public EmailVerificationPopupView {
 public:
  MockEmailVerificationPopupView(
      base::WeakPtr<EmailVerificationPopupController> controller,
      views::Widget* parent_widget,
      base::OnceCallback<void(bool)> callback)
      : EmailVerificationPopupView(
            controller,
            parent_widget,
            net::SchemefulSite(GURL("https://issuer.com")),
            u"user@example.com",
            std::move(callback)) {}

  MOCK_METHOD(void, Show, (), (override));
  MOCK_METHOD(void, Hide, (), (override));
  MOCK_METHOD(bool, OverlapsWithPictureInPictureWindow, (), (const, override));
};

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

// Tests that the popup view can be successfully shown and that hiding the popup
// correctly triggers the callback with `false` (cancelling the flow).
TEST_F(EmailVerificationPopupViewTest, Show) {
  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());

  std::unique_ptr<MockEmailVerificationPopupView> mock_view;

  controller->set_view_factory_for_testing(base::BindRepeating(
      [](std::unique_ptr<MockEmailVerificationPopupView>* mock_view,
         base::WeakPtr<EmailVerificationPopupController> delegate,
         views::Widget* parent_widget, const net::SchemefulSite& issuer_site,
         const std::u16string& email, base::OnceCallback<void(bool)> callback) {
        *mock_view = std::make_unique<MockEmailVerificationPopupView>(
            delegate, parent_widget, std::move(callback));
        return (*mock_view)->GetWeakPtr();
      },
      base::Unretained(&mock_view)));

  TestFuture<bool> confirmed_future;

  controller->Show(gfx::RectF(0, 0, 10, 10),
                   net::SchemefulSite(GURL("https://issuer.com")),
                   u"user@example.com", confirmed_future.GetCallback());

  ASSERT_TRUE(mock_view);
  EXPECT_CALL(*mock_view, Hide);

  // Verify that controller callback is invoked on hiding / closing.
  controller->Hide(SuggestionHidingReason::kTabGone);
  EXPECT_TRUE(confirmed_future.IsReady());
  EXPECT_FALSE(confirmed_future.Get());
}

}  // namespace
}  // namespace autofill
