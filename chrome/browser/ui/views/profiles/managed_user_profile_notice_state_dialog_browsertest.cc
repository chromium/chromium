// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/views/profiles/managed_user_profile_notice_test_utils.h"
#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_handler.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/any_widget_observer.h"
#include "url/gurl.h"

namespace {

using ::managed_user_profile_notice_test::ManagedUserProfileNoticeSelectors;
using ::managed_user_profile_notice_test::WaitForAndClickProceedButton;
using ::managed_user_profile_notice_test::WaitForScreenRendered;
using ::testing::AssertionResult;

using State = ::ManagedUserProfileNoticeHandler::State;
using ChoiceFuture =
    ::base::test::TestFuture<::signin::SigninChoice,
                             ::signin::SigninChoiceOperationDoneCallback,
                             ::signin::SigninChoiceOperationRetryCallback>;

// Returns the selector of the element shown for `state`. Only the states that
// have a dedicated OIDC screen are supported.
std::string GetStateScreenSelector(bool use_refreshed_ui, State state) {
  std::string_view legacy_selector;
  switch (state) {
    case State::kProcessing:
      legacy_selector = "#processing";
      break;
    case State::kSuccess:
      legacy_selector = "#success";
      break;
    case State::kTimeout:
      legacy_selector = "#timeout";
      break;
    case State::kError:
      legacy_selector = "#error";
      break;
    case State::kDisclosure:
    case State::kValueProposition:
    case State::kUserDataHandling:
    case State::kSignalsDisclaimer:
      NOTREACHED();
  }
  if (use_refreshed_ui) {
    // The refreshed app uses a single element for all states and reflects the
    // current state as an attribute.
    return base::StrCat(
        {"#state[state=\"", base::NumberToString(state), "\"]"});
  }
  return std::string(legacy_selector);
}

}  // namespace

// Tests the OIDC enrollment (`kEnterpriseOIDC`) modal dialog and its state
// screens (processing, success, timeout and error).
// The test parameter indicates whether the refreshed UI is enabled.
class ManagedUserProfileNoticeStateDialogBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  ManagedUserProfileNoticeStateDialogBrowserTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{switches::kFirstRunDesktopRefresh, UseRefreshedUi()},
         {switches::kFirstRunDesktopChoiceScreenRefresh, UseRefreshedUi()},
         {switches::kEnterpriseSigninDialogsRefresh, UseRefreshedUi()}});
  }

 protected:
  bool UseRefreshedUi() const { return GetParam(); }

  const ManagedUserProfileNoticeSelectors& GetSelectors() const {
    return managed_user_profile_notice_test::GetSelectors(UseRefreshedUi());
  }

  GURL GetExpectedUrl() const {
    return UseRefreshedUi()
               ? GURL(chrome::kChromeUIManagedUserProfileNoticeRefreshURL)
               : GURL(chrome::kChromeUIManagedUserProfileNoticeUrl);
  }

  // Shows the OIDC enrollment dialog, configured like
  // `OidcEnterpriseSigninInterceptionHandle` does, and returns its
  // `WebContents` once loaded, or `nullptr` on failure.
  content::WebContents* ShowOidcDialog(
      base::OnceClosure done_callback,
      base::RepeatingClosure retry_callback = base::DoNothing()) {
    views::NamedWidgetShownWaiter widget_waiter(
        views::test::AnyWidgetTestPasskey{},
        "SigninViewControllerDelegateViews");
    SigninViewController::From(browser())->ShowModalManagedUserNoticeDialog(
        std::make_unique<signin::EnterpriseProfileCreationDialogParams>(
            AccountInfo(),
            /*is_oidc_account=*/true,
            /*user_already_signed_in=*/false,
            /*profile_creation_required_by_policy=*/true,
            /*show_link_data_option=*/false,
            /*process_user_choice_callback=*/choice_future_.GetCallback(),
            std::move(done_callback), std::move(retry_callback)));
    if (!widget_waiter.WaitIfNeededAndGet()) {
      return nullptr;
    }
    content::WebContents* web_contents =
        SigninViewController::From(browser())
            ->GetModalDialogWebContentsForTesting();
    if (!web_contents || !content::WaitForLoadStop(web_contents)) {
      return nullptr;
    }
    return web_contents;
  }

  // Waits until the screen for `state` is rendered in the app's shadow root.
  AssertionResult WaitForStateScreen(content::WebContents* web_contents,
                                     State state) const {
    return WaitForScreenRendered(
        web_contents, GetSelectors().app,
        GetStateScreenSelector(UseRefreshedUi(), state));
  }

  // Clicks "Proceed" on the OIDC disclosure screen and waits for the
  // processing screen. Returns the callbacks the handler passed along with the
  // user choice.
  AssertionResult ProceedFromOidcDisclosure(
      content::WebContents* web_contents,
      signin::SigninChoiceOperationDoneCallback& done_callback,
      signin::SigninChoiceOperationRetryCallback& retry_callback) {
    if (AssertionResult result =
            WaitForAndClickProceedButton(web_contents, GetSelectors());
        !result) {
      return result;
    }
    auto [choice, done, retry] = choice_future_.Take();
    if (choice != signin::SIGNIN_CHOICE_NEW_PROFILE) {
      return testing::AssertionFailure()
             << "Expected SIGNIN_CHOICE_NEW_PROFILE, got " << choice;
    }
    done_callback = std::move(done);
    retry_callback = std::move(retry);
    return WaitForStateScreen(web_contents, State::kProcessing);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  ChoiceFuture choice_future_;
};

IN_PROC_BROWSER_TEST_P(ManagedUserProfileNoticeStateDialogBrowserTest,
                       ProcessingThenSuccess) {
  base::test::TestFuture<void> done_future;
  content::WebContents* web_contents =
      ShowOidcDialog(done_future.GetCallback());
  ASSERT_TRUE(web_contents);
  EXPECT_EQ(web_contents->GetLastCommittedURL(), GetExpectedUrl());

  signin::SigninChoiceOperationDoneCallback done_callback;
  signin::SigninChoiceOperationRetryCallback retry_callback;
  ASSERT_TRUE(
      ProceedFromOidcDisclosure(web_contents, done_callback, retry_callback));

  std::move(done_callback)
      .Run(signin::SIGNIN_CONFIRM_SUCCESS,
           signin::SigninChoiceErrorType::kNoError);
  ASSERT_TRUE(WaitForStateScreen(web_contents, State::kSuccess));

  // "Continue" on the success screen finishes the flow.
  ASSERT_TRUE(WaitForAndClickProceedButton(web_contents, GetSelectors()));
  EXPECT_TRUE(done_future.Wait());
}

IN_PROC_BROWSER_TEST_P(ManagedUserProfileNoticeStateDialogBrowserTest,
                       TimeoutThenRetry) {
  base::test::TestFuture<void> retry_future;
  content::WebContents* web_contents =
      ShowOidcDialog(base::DoNothing(), retry_future.GetRepeatingCallback());
  ASSERT_TRUE(web_contents);

  signin::SigninChoiceOperationDoneCallback done_callback;
  signin::SigninChoiceOperationRetryCallback retry_callback;
  ASSERT_TRUE(
      ProceedFromOidcDisclosure(web_contents, done_callback, retry_callback));

  retry_callback.Run(signin::SIGNIN_TIMEOUT,
                     signin::SigninChoiceErrorType::kNoError);
  ASSERT_TRUE(WaitForStateScreen(web_contents, State::kTimeout));

  // "Try again" on the timeout screen retries and goes back to processing.
  ASSERT_TRUE(WaitForAndClickProceedButton(web_contents, GetSelectors()));
  EXPECT_TRUE(retry_future.Wait());
  EXPECT_TRUE(WaitForStateScreen(web_contents, State::kProcessing));
}

IN_PROC_BROWSER_TEST_P(ManagedUserProfileNoticeStateDialogBrowserTest,
                       ErrorThenClose) {
  base::test::TestFuture<void> done_future;
  content::WebContents* web_contents =
      ShowOidcDialog(done_future.GetCallback());
  ASSERT_TRUE(web_contents);

  signin::SigninChoiceOperationDoneCallback done_callback;
  signin::SigninChoiceOperationRetryCallback retry_callback;
  ASSERT_TRUE(
      ProceedFromOidcDisclosure(web_contents, done_callback, retry_callback));

  std::move(done_callback)
      .Run(signin::SIGNIN_ERROR, signin::SigninChoiceErrorType::kUnknown);
  ASSERT_TRUE(WaitForStateScreen(web_contents, State::kError));

  // "Close" on the error screen finishes the flow.
  ASSERT_TRUE(WaitForAndClickProceedButton(web_contents, GetSelectors()));
  EXPECT_TRUE(done_future.Wait());
}

INSTANTIATE_TEST_SUITE_P(,
                         ManagedUserProfileNoticeStateDialogBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Refreshed" : "Legacy";
                         });
