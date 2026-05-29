// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/email_verification_popup_view.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/ui/autofill/email_verification_popup_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/strike_databases/email_verification_strike_database.h"
#include "components/autofill/core/browser/strike_databases/payments/test_strike_database.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
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
using EmailVerificationResult =
    AutofillClient::EmailVerificationPermissionUiResult;

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
            base::NullCallback()),
        decision_callback_(std::move(callback)) {}

  base::OnceCallback<void(bool)>& decision_callback() {
    return decision_callback_;
  }

  MOCK_METHOD(void, Show, (), (override));
  MOCK_METHOD(void, Hide, (), (override));
  MOCK_METHOD(bool, OverlapsWithPictureInPictureWindow, (), (const, override));

 private:
  base::OnceCallback<void(bool)> decision_callback_;
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

    test_autofill_client_injector_[test_web_contents_.get()]
        ->set_test_strike_database(std::make_unique<TestStrikeDatabase>());
  }

  void TearDown() override {
    widget_.reset();
    test_web_contents_.reset();
    ChromeViewsTestBase::TearDown();
  }

  content::WebContents* web_contents() { return test_web_contents_.get(); }

 protected:
  void SetupMockViewFactory(
      EmailVerificationPopupController* controller,
      std::unique_ptr<MockEmailVerificationPopupView>& mock_view_out) {
    controller->set_view_factory_for_testing(base::BindRepeating(
        [](std::unique_ptr<MockEmailVerificationPopupView>* mock_view_ptr,
           base::WeakPtr<EmailVerificationPopupController> delegate,
           views::Widget* parent_widget, const net::SchemefulSite& issuer_site,
           const std::u16string& email,
           base::OnceCallback<void(bool)> callback) {
          *mock_view_ptr = std::make_unique<MockEmailVerificationPopupView>(
              delegate, parent_widget, std::move(callback));
          return (*mock_view_ptr)->GetWeakPtr();
        },
        base::Unretained(&mock_view_out)));
  }

  content::RenderViewHostTestEnabler test_render_host_factories_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  std::unique_ptr<views::Widget> widget_;
  TestAutofillClientInjector<TestContentAutofillClient>
      test_autofill_client_injector_;
};

// Tests that the popup view can be successfully shown and that hiding the popup
// correctly triggers the callback with `false` (cancelling the flow) and logs
// the correct histogram sample.
TEST_F(EmailVerificationPopupViewTest, Show) {
  base::HistogramTester histogram_tester;
  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());

  std::unique_ptr<MockEmailVerificationPopupView> mock_view;

  SetupMockViewFactory(controller.get(), mock_view);

  TestFuture<EmailVerificationResult> confirmed_future;

  controller->Show(gfx::RectF(0, 0, 10, 10),
                   net::SchemefulSite(GURL("https://issuer.com")),
                   u"user@example.com", confirmed_future.GetCallback());

  ASSERT_TRUE(mock_view);
  EXPECT_CALL(*mock_view, Hide);

  // Verify that controller callback is invoked on hiding / closing.
  controller->Hide(SuggestionHidingReason::kTabGone);
  EXPECT_TRUE(confirmed_future.IsReady());
  EXPECT_EQ(confirmed_future.Get(), EmailVerificationResult::kIgnored);

  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.PermissionUi.Status",
      EmailVerificationPopupController::EvpPermissionUiStatus::kTabGone, 1);
}

TEST_F(EmailVerificationPopupViewTest, AllowedLogged) {
  base::HistogramTester histogram_tester;
  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());

  std::unique_ptr<MockEmailVerificationPopupView> mock_view;

  SetupMockViewFactory(controller.get(), mock_view);

  TestFuture<EmailVerificationResult> confirmed_future;

  controller->Show(gfx::RectF(0, 0, 10, 10),
                   net::SchemefulSite(GURL("https://issuer.com")),
                   u"user@example.com", confirmed_future.GetCallback());

  ASSERT_TRUE(mock_view);
  EXPECT_CALL(*mock_view, Hide);

  // Simulate confirming the prompt
  std::move(mock_view->decision_callback()).Run(true);

  EXPECT_TRUE(confirmed_future.IsReady());
  EXPECT_EQ(confirmed_future.Get(), EmailVerificationResult::kAccepted);

  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.PermissionUi.Status",
      EmailVerificationPopupController::EvpPermissionUiStatus::kAllowed, 1);
}

TEST_F(EmailVerificationPopupViewTest, DeclinedLogged) {
  base::HistogramTester histogram_tester;
  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());

  std::unique_ptr<MockEmailVerificationPopupView> mock_view;

  SetupMockViewFactory(controller.get(), mock_view);

  TestFuture<EmailVerificationResult> confirmed_future;

  controller->Show(gfx::RectF(0, 0, 10, 10),
                   net::SchemefulSite(GURL("https://issuer.com")),
                   u"user@example.com", confirmed_future.GetCallback());

  ASSERT_TRUE(mock_view);
  EXPECT_CALL(*mock_view, Hide);

  // Simulate declining the prompt
  std::move(mock_view->decision_callback()).Run(false);

  EXPECT_TRUE(confirmed_future.IsReady());
  EXPECT_EQ(confirmed_future.Get(), EmailVerificationResult::kDeclined);

  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.PermissionUi.Status",
      EmailVerificationPopupController::EvpPermissionUiStatus::kDeclined, 1);
}

TEST_F(EmailVerificationPopupViewTest, ClickOutsideLogged) {
  base::HistogramTester histogram_tester;
  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());

  std::unique_ptr<MockEmailVerificationPopupView> mock_view;

  SetupMockViewFactory(controller.get(), mock_view);

  TestFuture<EmailVerificationResult> confirmed_future;

  controller->Show(gfx::RectF(0, 0, 10, 10),
                   net::SchemefulSite(GURL("https://issuer.com")),
                   u"user@example.com", confirmed_future.GetCallback());

  ASSERT_TRUE(mock_view);
  EXPECT_CALL(*mock_view, Hide);

  // Simulate clicking outside the popup UI
  blink::WebMouseEvent event;
  controller->DidGetUserInteraction(event);

  EXPECT_TRUE(confirmed_future.IsReady());
  EXPECT_EQ(confirmed_future.Get(), EmailVerificationResult::kIgnored);

  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.PermissionUi.Status",
      EmailVerificationPopupController::EvpPermissionUiStatus::kUserAborted, 1);
}

TEST_F(EmailVerificationPopupViewTest, FocusChangedLogged) {
  base::HistogramTester histogram_tester;
  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());

  std::unique_ptr<MockEmailVerificationPopupView> mock_view;

  SetupMockViewFactory(controller.get(), mock_view);

  TestFuture<EmailVerificationResult> confirmed_future;

  controller->Show(gfx::RectF(0, 0, 10, 10),
                   net::SchemefulSite(GURL("https://issuer.com")),
                   u"user@example.com", confirmed_future.GetCallback());

  ASSERT_TRUE(mock_view);
  EXPECT_CALL(*mock_view, Hide);

  // Simulate focus loss (e.g. user clicking outside or focusing another window)
  controller->Hide(SuggestionHidingReason::kFocusChanged);

  EXPECT_TRUE(confirmed_future.IsReady());
  EXPECT_EQ(confirmed_future.Get(), EmailVerificationResult::kIgnored);

  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.PermissionUi.Status",
      EmailVerificationPopupController::EvpPermissionUiStatus::kUserAborted, 1);
}

TEST_F(EmailVerificationPopupViewTest, AcceptUpdatesPrefs) {
  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());

  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  base::OnceCallback<void(bool)> saved_callback;

  controller->set_view_factory_for_testing(base::BindRepeating(
      [](std::unique_ptr<MockEmailVerificationPopupView>* mock_view,
         base::OnceCallback<void(bool)>* saved_callback,
         base::WeakPtr<EmailVerificationPopupController> delegate,
         views::Widget* parent_widget, const net::SchemefulSite& issuer_site,
         const std::u16string& email, base::OnceCallback<void(bool)> callback) {
        *saved_callback = std::move(callback);
        *mock_view = std::make_unique<MockEmailVerificationPopupView>(
            delegate, parent_widget, base::DoNothing());
        return (*mock_view)->GetWeakPtr();
      },
      base::Unretained(&mock_view), base::Unretained(&saved_callback)));

  TestFuture<EmailVerificationResult> confirmed_future;
  std::u16string email = u"user@example.com";
  net::SchemefulSite issuer_site(GURL("https://issuer.com"));

  controller->Show(
      gfx::RectF(0, 0, 10, 10), issuer_site, email,
      base::BindOnce(
          [](PrefService* prefs,
             base::OnceCallback<void(EmailVerificationResult)> cb,
             EmailVerificationResult result) {
            bool accepted = (result == EmailVerificationResult::kAccepted);
            if (accepted) {
              base::DictValue state =
                  prefs->GetDict(prefs::kAutofillEmailVerificationState)
                      .Clone();
              base::DictValue email_dict;
              email_dict.Set("allowed", true);
              email_dict.Set("issuer_site", "https://issuer.com");
              email_dict.Set("timestamp", true);
              state.Set("user@example.com", std::move(email_dict));
              prefs->SetDict(prefs::kAutofillEmailVerificationState,
                             std::move(state));
            }
            std::move(cb).Run(result);
          },
          profile_.GetPrefs(), confirmed_future.GetCallback()));

  ASSERT_TRUE(mock_view);
  ASSERT_TRUE(saved_callback);

  std::move(saved_callback).Run(true);  // Simulate accept

  EXPECT_TRUE(confirmed_future.IsReady());
  EXPECT_EQ(confirmed_future.Get(), EmailVerificationResult::kAccepted);

  PrefService* prefs = profile_.GetPrefs();
  const auto& state = prefs->GetDict(prefs::kAutofillEmailVerificationState);
  const auto* email_data = state.FindDict("user@example.com");
  ASSERT_TRUE(email_data);
  EXPECT_TRUE(email_data->FindBool("allowed").value_or(false));
  EXPECT_EQ(*email_data->FindString("issuer_site"), issuer_site.Serialize());
  EXPECT_TRUE(email_data->Find("timestamp"));  // Just check it exists
}

TEST_F(EmailVerificationPopupViewTest, IncrementsDeclineCount) {
  std::u16string email = u"test@example.com";
  base::RunLoop run_loop;
  TestContentAutofillClient* client =
      test_autofill_client_injector_[web_contents()];
  auto callback = base::BindOnce(
      [](base::OnceClosure quit_closure, TestContentAutofillClient* client,
         std::string email, EmailVerificationResult result) {
        EXPECT_EQ(result, EmailVerificationResult::kDeclined);
        if (result == EmailVerificationResult::kDeclined) {
          EmailVerificationStrikeDatabase strike_db(
              client->GetStrikeDatabase());
          strike_db.AddStrike(EmailVerificationStrikeDatabase::GetId(email));
        }
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure(), client, "test@example.com");

  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());

  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  base::OnceCallback<void(bool)> saved_callback;

  controller->set_view_factory_for_testing(base::BindRepeating(
      [](std::unique_ptr<MockEmailVerificationPopupView>* mock_view,
         base::OnceCallback<void(bool)>* saved_callback,
         base::WeakPtr<EmailVerificationPopupController> delegate,
         views::Widget* parent_widget, const net::SchemefulSite& issuer_site,
         const std::u16string& email, base::OnceCallback<void(bool)> callback) {
        *saved_callback = std::move(callback);
        *mock_view = std::make_unique<MockEmailVerificationPopupView>(
            delegate, parent_widget, base::DoNothing());
        return (*mock_view)->GetWeakPtr();
      },
      base::Unretained(&mock_view), base::Unretained(&saved_callback)));

  controller->Show(gfx::RectF(0, 0, 10, 10),
                   net::SchemefulSite(GURL("https://example.com")), email,
                   std::move(callback));

  ASSERT_TRUE(mock_view);
  ASSERT_TRUE(saved_callback);

  std::move(saved_callback).Run(false);  // Simulate decline

  run_loop.Run();

  // Check strikes.
  EmailVerificationStrikeDatabase strike_db(client->GetStrikeDatabase());
  EXPECT_EQ(strike_db.GetStrikes(
                EmailVerificationStrikeDatabase::GetId("test@example.com")),
            1);
}

TEST_F(EmailVerificationPopupViewTest, ShowsPopupIfDeclinedLessThanThreeTimes) {
  std::u16string email = u"test2@example.com";
  TestContentAutofillClient* client =
      test_autofill_client_injector_[web_contents()];
  EmailVerificationStrikeDatabase strike_db(client->GetStrikeDatabase());
  strike_db.AddStrikes(
      2, EmailVerificationStrikeDatabase::GetId("test2@example.com"));

  base::RunLoop run_loop;
  auto callback = base::BindOnce(
      [](base::OnceClosure quit_closure, TestContentAutofillClient* client,
         std::string email, EmailVerificationResult result) {
        EXPECT_EQ(result, EmailVerificationResult::kDeclined);
        if (result == EmailVerificationResult::kDeclined) {
          EmailVerificationStrikeDatabase strike_db(
              client->GetStrikeDatabase());
          strike_db.AddStrike(EmailVerificationStrikeDatabase::GetId(email));
        }
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure(), client, "test2@example.com");

  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());

  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  base::OnceCallback<void(bool)> saved_callback;

  controller->set_view_factory_for_testing(base::BindRepeating(
      [](std::unique_ptr<MockEmailVerificationPopupView>* mock_view,
         base::OnceCallback<void(bool)>* saved_callback,
         base::WeakPtr<EmailVerificationPopupController> delegate,
         views::Widget* parent_widget, const net::SchemefulSite& issuer_site,
         const std::u16string& email, base::OnceCallback<void(bool)> callback) {
        *saved_callback = std::move(callback);
        *mock_view = std::make_unique<MockEmailVerificationPopupView>(
            delegate, parent_widget, base::DoNothing());
        return (*mock_view)->GetWeakPtr();
      },
      base::Unretained(&mock_view), base::Unretained(&saved_callback)));

  controller->Show(gfx::RectF(0, 0, 10, 10),
                   net::SchemefulSite(GURL("https://example.com")), email,
                   std::move(callback));

  ASSERT_TRUE(mock_view);
  ASSERT_TRUE(saved_callback);

  std::move(saved_callback).Run(false);  // Simulate decline

  run_loop.Run();

  // Check that declines incremented to 3.
  EXPECT_EQ(strike_db.GetStrikes(
                EmailVerificationStrikeDatabase::GetId("test2@example.com")),
            3);
}

TEST_F(EmailVerificationPopupViewTest, DismissalDoesNotIncrementDeclineCount) {
  std::u16string email = u"test@example.com";
  base::RunLoop run_loop;
  TestContentAutofillClient* client =
      test_autofill_client_injector_[web_contents()];
  auto callback = base::BindOnce(
      [](base::OnceClosure quit_closure, TestContentAutofillClient* client,
         std::string email, EmailVerificationResult result) {
        EXPECT_EQ(result, EmailVerificationResult::kIgnored);
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure(), client, "test@example.com");

  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());

  std::unique_ptr<MockEmailVerificationPopupView> mock_view;

  SetupMockViewFactory(controller.get(), mock_view);

  controller->Show(gfx::RectF(0, 0, 10, 10),
                   net::SchemefulSite(GURL("https://example.com")), email,
                   std::move(callback));

  ASSERT_TRUE(mock_view);
  EXPECT_CALL(*mock_view, Hide);

  // Simulate dismissal by focus loss / tab gone.
  controller->Hide(SuggestionHidingReason::kTabGone);

  run_loop.Run();

  // Check strikes are still 0.
  EmailVerificationStrikeDatabase strike_db(client->GetStrikeDatabase());
  EXPECT_EQ(strike_db.GetStrikes(
                EmailVerificationStrikeDatabase::GetId("test@example.com")),
            0);
}

}  // namespace
}  // namespace autofill
