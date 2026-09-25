// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/email_verifier/email_verification_popup_view.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verification_controller.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verification_controller_test_api.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verification_popup_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/strike_databases/evp/email_verification_strike_database.h"
#include "components/autofill/core/browser/strike_databases/payments/test_strike_database.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace autofill {
namespace {

using ::base::test::TestFuture;
using EmailVerificationPermissionUiStatus =
    AutofillClient::EmailVerificationPermissionUiStatus;

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
  MOCK_METHOD(void, ShowLoadingState, (), (override));
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

  TestFuture<EmailVerificationPermissionUiStatus> confirmed_future;

  controller->Show(gfx::RectF(0, 0, 10, 10),
                   net::SchemefulSite(GURL("https://issuer.com")),
                   u"user@example.com", confirmed_future.GetCallback());

  ASSERT_TRUE(mock_view);
  EXPECT_CALL(*mock_view, Hide);

  // Verify that controller callback is invoked on hiding / closing.
  controller->Hide(SuggestionHidingReason::kTabGone);
  EXPECT_TRUE(confirmed_future.IsReady());
  EXPECT_EQ(confirmed_future.Get(),
            EmailVerificationPermissionUiStatus::kTabGone);

  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.PermissionUi.Status",
      EmailVerificationPermissionUiStatus::kTabGone, 1);
}

// Tests that accepting the permission prompt transitions to the loading state,
// returns kAllowed to the caller, and logs the metric.
TEST_F(EmailVerificationPopupViewTest, AllowedLogged) {
  base::HistogramTester histogram_tester;
  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());

  std::unique_ptr<MockEmailVerificationPopupView> mock_view;

  SetupMockViewFactory(controller.get(), mock_view);

  TestFuture<EmailVerificationPermissionUiStatus> confirmed_future;

  controller->Show(gfx::RectF(0, 0, 10, 10),
                   net::SchemefulSite(GURL("https://issuer.com")),
                   u"user@example.com", confirmed_future.GetCallback());

  ASSERT_TRUE(mock_view);
  EXPECT_CALL(*mock_view, ShowLoadingState);
  EXPECT_CALL(*mock_view, Hide).Times(0);

  // Simulate confirming the prompt
  std::move(mock_view->decision_callback()).Run(true);

  EXPECT_TRUE(confirmed_future.IsReady());
  EXPECT_EQ(confirmed_future.Get(),
            EmailVerificationPermissionUiStatus::kAllowed);
  EXPECT_TRUE(controller->is_loading());

  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.PermissionUi.Status",
      EmailVerificationPermissionUiStatus::kAllowed, 1);

  // Flow completion dismisses the popup.
  EXPECT_CALL(*mock_view, Hide);
  controller->Dismiss();
  EXPECT_FALSE(controller->is_loading());
}

// Tests that declining the permission prompt dismisses the view, returns
// kDeclined to the caller, and logs the metric.
TEST_F(EmailVerificationPopupViewTest, DeclinedLogged) {
  base::HistogramTester histogram_tester;
  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());

  std::unique_ptr<MockEmailVerificationPopupView> mock_view;

  SetupMockViewFactory(controller.get(), mock_view);

  TestFuture<EmailVerificationPermissionUiStatus> confirmed_future;

  controller->Show(gfx::RectF(0, 0, 10, 10),
                   net::SchemefulSite(GURL("https://issuer.com")),
                   u"user@example.com", confirmed_future.GetCallback());

  ASSERT_TRUE(mock_view);
  EXPECT_CALL(*mock_view, Hide);

  // Simulate declining the prompt
  std::move(mock_view->decision_callback()).Run(false);

  EXPECT_TRUE(confirmed_future.IsReady());
  EXPECT_EQ(confirmed_future.Get(),
            EmailVerificationPermissionUiStatus::kDeclined);

  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.PermissionUi.Status",
      EmailVerificationPermissionUiStatus::kDeclined, 1);
}

// Tests that clicking outside the popup dismisses the view, returns
// kUserAborted, and logs the metric.
TEST_F(EmailVerificationPopupViewTest, ClickOutsideLogged) {
  base::HistogramTester histogram_tester;
  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());

  std::unique_ptr<MockEmailVerificationPopupView> mock_view;

  SetupMockViewFactory(controller.get(), mock_view);

  TestFuture<EmailVerificationPermissionUiStatus> confirmed_future;

  controller->Show(gfx::RectF(0, 0, 10, 10),
                   net::SchemefulSite(GURL("https://issuer.com")),
                   u"user@example.com", confirmed_future.GetCallback());

  ASSERT_TRUE(mock_view);
  EXPECT_CALL(*mock_view, Hide);

  // Simulate clicking outside the popup UI
  blink::WebMouseEvent event;
  controller->DidGetUserInteraction(event);

  EXPECT_TRUE(confirmed_future.IsReady());
  EXPECT_EQ(confirmed_future.Get(),
            EmailVerificationPermissionUiStatus::kUserAborted);

  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.PermissionUi.Status",
      EmailVerificationPermissionUiStatus::kUserAborted, 1);
}

// Tests that losing focus on the target field dismisses the view, returns
// kUserAborted, and logs the metric.
TEST_F(EmailVerificationPopupViewTest, FocusChangedLogged) {
  base::HistogramTester histogram_tester;
  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());

  std::unique_ptr<MockEmailVerificationPopupView> mock_view;

  SetupMockViewFactory(controller.get(), mock_view);

  TestFuture<EmailVerificationPermissionUiStatus> confirmed_future;

  controller->Show(gfx::RectF(0, 0, 10, 10),
                   net::SchemefulSite(GURL("https://issuer.com")),
                   u"user@example.com", confirmed_future.GetCallback());

  ASSERT_TRUE(mock_view);
  EXPECT_CALL(*mock_view, Hide);

  // Simulate focus loss (e.g. user clicking outside or focusing another window)
  controller->Hide(SuggestionHidingReason::kFocusChanged);

  EXPECT_TRUE(confirmed_future.IsReady());
  EXPECT_EQ(confirmed_future.Get(),
            EmailVerificationPermissionUiStatus::kUserAborted);

  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.PermissionUi.Status",
      EmailVerificationPermissionUiStatus::kUserAborted, 1);
}

// Tests that accepting the permission prompt correctly records the user's
// consent in autofill preferences.
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

  TestFuture<EmailVerificationPermissionUiStatus> confirmed_future;
  std::u16string email = u"user@example.com";
  net::SchemefulSite issuer_site(GURL("https://issuer.com"));

  controller->Show(
      gfx::RectF(0, 0, 10, 10), issuer_site, email,
      base::BindOnce(
          [](PrefService* prefs,
             base::OnceCallback<void(EmailVerificationPermissionUiStatus)> cb,
             EmailVerificationPermissionUiStatus status) {
            bool accepted =
                (status == EmailVerificationPermissionUiStatus::kAllowed);
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
            std::move(cb).Run(status);
          },
          profile_.GetPrefs(), confirmed_future.GetCallback()));

  ASSERT_TRUE(mock_view);
  ASSERT_TRUE(saved_callback);
  EXPECT_CALL(*mock_view, ShowLoadingState);

  std::move(saved_callback).Run(true);  // Simulate accept

  EXPECT_TRUE(confirmed_future.IsReady());
  EXPECT_EQ(confirmed_future.Get(),
            EmailVerificationPermissionUiStatus::kAllowed);

  PrefService* prefs = profile_.GetPrefs();
  const auto& state = prefs->GetDict(prefs::kAutofillEmailVerificationState);
  const auto* email_data = state.FindDict("user@example.com");
  ASSERT_TRUE(email_data);
  EXPECT_TRUE(email_data->FindBool("allowed").value_or(false));
  EXPECT_EQ(*email_data->FindString("issuer_site"), issuer_site.Serialize());
  EXPECT_TRUE(email_data->Find("timestamp"));  // Just check it exists
}

// Tests that declining the prompt records a strike in the email verification
// strike database.
TEST_F(EmailVerificationPopupViewTest, IncrementsDeclineCount) {
  std::u16string email = u"test@example.com";
  base::RunLoop run_loop;
  TestContentAutofillClient* client =
      test_autofill_client_injector_[web_contents()];
  auto callback = base::BindOnce(
      [](base::OnceClosure quit_closure, TestContentAutofillClient* client,
         std::string email, EmailVerificationPermissionUiStatus status) {
        EXPECT_EQ(status, EmailVerificationPermissionUiStatus::kDeclined);
        if (status == EmailVerificationPermissionUiStatus::kDeclined) {
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

// Tests that the prompt is still shown when the user has declined fewer than 3
// times (the maximum strike limit).
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
         std::string email, EmailVerificationPermissionUiStatus status) {
        EXPECT_EQ(status, EmailVerificationPermissionUiStatus::kDeclined);
        if (status == EmailVerificationPermissionUiStatus::kDeclined) {
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

// Tests that dismissing the popup via navigation/tab change does not count as a
// decline strike.
TEST_F(EmailVerificationPopupViewTest, DismissalDoesNotIncrementDeclineCount) {
  std::u16string email = u"test@example.com";
  base::RunLoop run_loop;
  TestContentAutofillClient* client =
      test_autofill_client_injector_[web_contents()];
  auto callback = base::BindOnce(
      [](base::OnceClosure quit_closure, TestContentAutofillClient* client,
         std::string email, EmailVerificationPermissionUiStatus status) {
        EXPECT_EQ(status, EmailVerificationPermissionUiStatus::kTabGone);
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

// Verifies that neither button receives initial focus upon appearance.
// For non-blocking popups, no button receives initial focus to prevent
// unexpected focus shifts and eliminate UI keyjacking vectors.
TEST_F(EmailVerificationPopupViewTest, NoInitialButtonFocus) {
  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());
  auto view = std::make_unique<EmailVerificationPopupView>(
      controller->GetWeakPtr(), widget_.get(),
      net::SchemefulSite(GURL("https://issuer.com")), u"user@example.com",
      base::DoNothing());

  views::MdTextButton* cancel_button = views::AsViewClass<views::MdTextButton>(
      view->GetViewByID(static_cast<int>(
          EmailVerificationPopupView::PopupViewId::kCancelButton)));
  ASSERT_THAT(cancel_button, testing::NotNull());

  views::MdTextButton* confirm_button = views::AsViewClass<views::MdTextButton>(
      view->GetViewByID(static_cast<int>(
          EmailVerificationPopupView::PopupViewId::kConfirmButton)));
  ASSERT_THAT(confirm_button, testing::NotNull());

  EXPECT_EQ(view->GetInitiallyFocusedView(), nullptr);
}

// Tests that ShowLoadingState() transitions the confirm button into its loading
// state with an active spinner, disables the cancel button, and keeps the
// confirm button's size and color while hiding its text.
TEST_F(EmailVerificationPopupViewTest, ShowLoadingStateRealView) {
  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());
  base::WeakPtr<EmailVerificationPopupView> view =
      EmailVerificationPopupView::Show(
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

// Tests that while the popup is in the loading state, transient hide reasons
// (such as focus changes, clicking outside, ending editing, scrolling, or
// widget resizes) are suppressed so that the spinner remains visible.
TEST_F(EmailVerificationPopupViewTest,
       LoadingStateIgnoresInteractionsUntilDismissed) {
  auto controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());

  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  SetupMockViewFactory(controller.get(), mock_view);

  TestFuture<EmailVerificationPermissionUiStatus> confirmed_future;
  controller->Show(gfx::RectF(0, 0, 10, 10),
                   net::SchemefulSite(GURL("https://issuer.com")),
                   u"user@example.com", confirmed_future.GetCallback());

  ASSERT_TRUE(mock_view);
  EXPECT_CALL(*mock_view, ShowLoadingState);
  EXPECT_CALL(*mock_view, Hide).Times(0);

  std::move(mock_view->decision_callback()).Run(true);
  EXPECT_TRUE(controller->is_loading());

  // Non-fatal hiding events are ignored while loading.
  controller->Hide(SuggestionHidingReason::kFocusChanged);
  controller->Hide(SuggestionHidingReason::kUserAborted);
  controller->Hide(SuggestionHidingReason::kEndEditing);
  controller->Hide(SuggestionHidingReason::kWidgetChanged);
  controller->Hide(SuggestionHidingReason::kContentAreaMoved);
  controller->Hide(SuggestionHidingReason::kElementOutsideOfContentArea);
  controller->Hide(SuggestionHidingReason::kRendererEvent);
  controller->Hide(SuggestionHidingReason::kInsufficientSpace);
  blink::WebMouseEvent mouse_event;
  controller->DidGetUserInteraction(mouse_event);
  EXPECT_TRUE(controller->is_loading());

  // Explicit dismissal closes the view.
  EXPECT_CALL(*mock_view, Hide);
  controller->Dismiss();
  EXPECT_FALSE(controller->is_loading());
}

// Tests that HidePopup() enforces the minimum display duration (800ms) while
// the popup is in the loading state before dismissing the view.
TEST_F(EmailVerificationPopupViewTest, MinimumLoadingDurationEnforced) {
  auto evp_controller =
      std::make_unique<EmailVerificationController>(web_contents());
  auto popup_controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());

  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  SetupMockViewFactory(popup_controller.get(), mock_view);

  EmailVerificationPopupController* raw_popup_controller =
      popup_controller.get();
  test_api(*evp_controller).set_popup_controller(std::move(popup_controller));

  TestFuture<EmailVerificationPermissionUiStatus> confirmed_future;
  evp_controller->ShowPopup(
      gfx::RectF(0, 0, 10, 10), net::SchemefulSite(GURL("https://issuer.com")),
      u"user@example.com", confirmed_future.GetCallback());

  ASSERT_TRUE(mock_view);
  EXPECT_CALL(*mock_view, ShowLoadingState);
  EXPECT_CALL(*mock_view, Hide).Times(0);

  std::move(mock_view->decision_callback()).Run(true);
  EXPECT_TRUE(raw_popup_controller->is_loading());

  // Calling HidePopup before minimum duration should NOT immediately hide the
  // popup.
  evp_controller->HidePopup();
  EXPECT_TRUE(raw_popup_controller->is_loading());

  // Fast forward by half the duration; popup should still be loading.
  task_environment()->FastForwardBy(
      EmailVerificationController::kMinimumLoadingDuration / 2);
  EXPECT_TRUE(raw_popup_controller->is_loading());

  // Fast forward the remaining duration; now it should hide.
  EXPECT_CALL(*mock_view, Hide);
  task_environment()->FastForwardBy(
      EmailVerificationController::kMinimumLoadingDuration / 2);
  EXPECT_FALSE(raw_popup_controller->is_loading());
  EXPECT_FALSE(test_api(*evp_controller).loading_start_time().has_value());
}

// Tests that if a new popup is requested while a previous popup's hide timer
// is pending, the timer is canceled and the previous view is dismissed.
TEST_F(EmailVerificationPopupViewTest, NewPopupCancelsPendingHidePopupTimer) {
  auto evp_controller =
      std::make_unique<EmailVerificationController>(web_contents());
  auto popup_controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());

  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  SetupMockViewFactory(popup_controller.get(), mock_view);

  EmailVerificationPopupController* raw_popup_controller =
      popup_controller.get();
  test_api(*evp_controller).set_popup_controller(std::move(popup_controller));

  TestFuture<EmailVerificationPermissionUiStatus> first_future;
  evp_controller->ShowPopup(gfx::RectF(0, 0, 10, 10),
                            net::SchemefulSite(GURL("https://issuer.com")),
                            u"user@example.com", first_future.GetCallback());

  ASSERT_TRUE(mock_view);
  EXPECT_CALL(*mock_view, ShowLoadingState);
  EXPECT_CALL(*mock_view, Hide).Times(0);

  std::move(mock_view->decision_callback()).Run(true);
  EXPECT_TRUE(raw_popup_controller->is_loading());

  // Call HidePopup, starting the anti-flicker delay timer.
  evp_controller->HidePopup();
  EXPECT_TRUE(test_api(*evp_controller).is_hide_popup_timer_running());

  // Rapidly trigger a new popup before the 800ms timer fires.
  // The first view should be torn down when the new popup is created.
  EXPECT_CALL(*mock_view, Hide);
  std::unique_ptr<MockEmailVerificationPopupView> new_mock_view;
  SetupMockViewFactory(raw_popup_controller, new_mock_view);

  TestFuture<EmailVerificationPermissionUiStatus> second_future;
  evp_controller->ShowPopup(gfx::RectF(20, 20, 10, 10),
                            net::SchemefulSite(GURL("https://issuer.com")),
                            u"user@example.com", second_future.GetCallback());

  // ShowPopup must have cancelled the pending hide timer and reset state.
  EXPECT_FALSE(test_api(*evp_controller).is_hide_popup_timer_running());
  EXPECT_FALSE(test_api(*evp_controller).loading_start_time().has_value());

  // Fast forward past 800ms; the new popup view should NOT be dismissed by the
  // previous timer.
  EXPECT_CALL(*new_mock_view, Hide).Times(0);
  task_environment()->FastForwardBy(base::Milliseconds(900));

  // Explicitly dismiss at the end of test.
  EXPECT_CALL(*new_mock_view, Hide);
  raw_popup_controller->Dismiss();
}

// Tests that calling ShowVerifiedToast() while loading is in progress defers
// displaying the toast until the minimum display duration (800ms) has elapsed.
TEST_F(EmailVerificationPopupViewTest, ShowVerifiedToastDefersWhenLoading) {
  auto evp_controller =
      std::make_unique<EmailVerificationController>(web_contents());
  auto popup_controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());

  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  SetupMockViewFactory(popup_controller.get(), mock_view);

  test_api(*evp_controller).set_popup_controller(std::move(popup_controller));

  TestFuture<EmailVerificationPermissionUiStatus> future;
  evp_controller->ShowPopup(gfx::RectF(0, 0, 10, 10),
                            net::SchemefulSite(GURL("https://issuer.com")),
                            u"user@example.com", future.GetCallback());

  ASSERT_TRUE(mock_view);
  EXPECT_CALL(*mock_view, ShowLoadingState);
  std::move(mock_view->decision_callback()).Run(true);
  EXPECT_TRUE(test_api(*evp_controller).loading_start_time().has_value());

  // Calling ShowVerifiedToast while loading must defer display.
  evp_controller->ShowVerifiedToast(GURL("https://issuer.com"));
  EXPECT_TRUE(test_api(*evp_controller).is_toast_timer_running());

  // Halfway through, timer is still running.
  task_environment()->FastForwardBy(
      EmailVerificationController::kMinimumLoadingDuration / 2);
  EXPECT_TRUE(test_api(*evp_controller).is_toast_timer_running());

  // Once full duration elapses, timer fires and completes.
  task_environment()->FastForwardBy(
      EmailVerificationController::kMinimumLoadingDuration / 2);
  EXPECT_FALSE(test_api(*evp_controller).is_toast_timer_running());
  EXPECT_FALSE(test_api(*evp_controller).loading_start_time().has_value());
}

// Tests that calling ShowErrorToast() while loading is in progress defers
// displaying the error toast until the minimum display duration (800ms) has
// elapsed.
TEST_F(EmailVerificationPopupViewTest, ShowErrorToastDefersWhenLoading) {
  auto evp_controller =
      std::make_unique<EmailVerificationController>(web_contents());
  auto popup_controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());

  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  SetupMockViewFactory(popup_controller.get(), mock_view);

  test_api(*evp_controller).set_popup_controller(std::move(popup_controller));

  TestFuture<EmailVerificationPermissionUiStatus> future;
  evp_controller->ShowPopup(gfx::RectF(0, 0, 10, 10),
                            net::SchemefulSite(GURL("https://issuer.com")),
                            u"user@example.com", future.GetCallback());

  ASSERT_TRUE(mock_view);
  EXPECT_CALL(*mock_view, ShowLoadingState);
  std::move(mock_view->decision_callback()).Run(true);
  EXPECT_TRUE(test_api(*evp_controller).loading_start_time().has_value());

  // Calling ShowErrorToast while loading must defer display.
  evp_controller->ShowErrorToast();
  EXPECT_TRUE(test_api(*evp_controller).is_toast_timer_running());

  // Halfway through, timer is still running.
  task_environment()->FastForwardBy(
      EmailVerificationController::kMinimumLoadingDuration / 2);
  EXPECT_TRUE(test_api(*evp_controller).is_toast_timer_running());

  // Once full duration elapses, timer fires and completes.
  task_environment()->FastForwardBy(
      EmailVerificationController::kMinimumLoadingDuration / 2);
  EXPECT_FALSE(test_api(*evp_controller).is_toast_timer_running());
  EXPECT_FALSE(test_api(*evp_controller).loading_start_time().has_value());
}

// Tests that if an error toast is requested while a verified toast is pending,
// the pending timer is superseded and only the error toast is scheduled.
TEST_F(EmailVerificationPopupViewTest, ToastTimersCancelEachOther) {
  auto evp_controller =
      std::make_unique<EmailVerificationController>(web_contents());
  auto popup_controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());

  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  SetupMockViewFactory(popup_controller.get(), mock_view);

  test_api(*evp_controller).set_popup_controller(std::move(popup_controller));

  TestFuture<EmailVerificationPermissionUiStatus> future;
  evp_controller->ShowPopup(gfx::RectF(0, 0, 10, 10),
                            net::SchemefulSite(GURL("https://issuer.com")),
                            u"user@example.com", future.GetCallback());

  ASSERT_TRUE(mock_view);
  EXPECT_CALL(*mock_view, ShowLoadingState);
  std::move(mock_view->decision_callback()).Run(true);

  // Trigger verified toast first.
  evp_controller->ShowVerifiedToast(GURL("https://issuer.com"));
  EXPECT_TRUE(test_api(*evp_controller).is_toast_timer_running());

  // Trigger error toast; must supersede the pending verified toast.
  evp_controller->ShowErrorToast();
  EXPECT_TRUE(test_api(*evp_controller).is_toast_timer_running());
}

// Tests that calling HidePopup() when the popup is not loading (e.g. during a
// subsequent run where only the loading toast is active) does not reset the
// loading start time.
TEST_F(EmailVerificationPopupViewTest,
       HidePopupDoesNotResetLoadingStartTimeWhenPopupNotLoading) {
  auto evp_controller =
      std::make_unique<EmailVerificationController>(web_contents());
  auto popup_controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());

  test_api(*evp_controller).set_popup_controller(std::move(popup_controller));

  // In subsequent run, ShowLoadingToast starts loading_start_time_.
  evp_controller->ShowLoadingToast();
  EXPECT_TRUE(test_api(*evp_controller).loading_start_time().has_value());

  // Calling HidePopup (which EmailVerifierDelegate does unconditionally)
  // must NOT wipe out the loading_start_time_ since the popup was not loading.
  evp_controller->HidePopup();
  EXPECT_TRUE(test_api(*evp_controller).loading_start_time().has_value());

  // Subsequent completion toasts must still defer.
  evp_controller->ShowVerifiedToast(GURL("https://issuer.com"));
  EXPECT_TRUE(test_api(*evp_controller).is_toast_timer_running());
}

// A `WebContentsViewDelegate` that records calls to `WebContents::Focus()`. It
// is installed through `FocusRecordingContentBrowserClient` because the test
// `RenderWidgetHostView` does not track focus.
class FocusRecordingWebContentsViewDelegate
    : public content::WebContentsViewDelegate {
 public:
  explicit FocusRecordingWebContentsViewDelegate(int& focus_count)
      : focus_count_(focus_count) {}

  // `WebContentsView::Focus()` calls this on all desktop platforms (unlike
  // `Focus()`, which the Mac implementation does not forward to the delegate).
  void ResetStoredFocus() override { ++*focus_count_; }

 private:
  raw_ref<int> focus_count_;
};

class FocusRecordingContentBrowserClient
    : public content::ContentBrowserClient {
 public:
  std::unique_ptr<content::WebContentsViewDelegate> GetWebContentsViewDelegate(
      content::WebContents* web_contents) override {
    return std::make_unique<FocusRecordingWebContentsViewDelegate>(
        focus_count_);
  }

  int focus_count() const { return focus_count_; }

 private:
  int focus_count_ = 0;
};

class EmailVerificationPopupViewFocusTest
    : public EmailVerificationPopupViewTest {
 public:
  void SetUp() override {
    // The client must be installed before the test WebContents is created so
    // that its view picks up the recording delegate.
    scoped_client_setting_ =
        std::make_unique<content::ScopedContentBrowserClientSetting>(
            &browser_client_);
    EmailVerificationPopupViewTest::SetUp();
  }

  void TearDown() override {
    EmailVerificationPopupViewTest::TearDown();
    scoped_client_setting_.reset();
  }

  int focus_count() const { return browser_client_.focus_count(); }

 private:
  FocusRecordingContentBrowserClient browser_client_;
  std::unique_ptr<content::ScopedContentBrowserClientSetting>
      scoped_client_setting_;
};

// Tests that dismissing the popup after it entered the loading state returns
// focus to the WebContents, so that the form field regains focus instead of
// the completion toast.
TEST_F(EmailVerificationPopupViewFocusTest,
       HidePopupReturnsFocusToWebContentsWhenLoading) {
  auto evp_controller =
      std::make_unique<EmailVerificationController>(web_contents());
  auto popup_controller =
      std::make_unique<EmailVerificationPopupController>(web_contents());

  std::unique_ptr<MockEmailVerificationPopupView> mock_view;
  SetupMockViewFactory(popup_controller.get(), mock_view);
  test_api(*evp_controller).set_popup_controller(std::move(popup_controller));

  TestFuture<EmailVerificationPermissionUiStatus> future;
  evp_controller->ShowPopup(gfx::RectF(0, 0, 10, 10),
                            net::SchemefulSite(GURL("https://issuer.com")),
                            u"user@example.com", future.GetCallback());

  ASSERT_TRUE(mock_view);
  EXPECT_CALL(*mock_view, ShowLoadingState);
  std::move(mock_view->decision_callback()).Run(true);

  // Let the minimum loading duration elapse so that HidePopup() dismisses the
  // popup immediately.
  task_environment()->FastForwardBy(
      EmailVerificationController::kMinimumLoadingDuration);

  const int focus_count_before = focus_count();
  EXPECT_CALL(*mock_view, Hide);
  evp_controller->HidePopup();
  EXPECT_EQ(focus_count(), focus_count_before + 1);
}

}  // namespace
}  // namespace autofill
