// // Copyright 2024 The Chromium Authors
// // Use of this source code is governed by a BSD-style license that can be
// // found in the LICENSE file.
#include "chrome/browser/ui/signin/promos/signin_promo_tab_helper.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"

class SigninPromoTabHelperTest : public InProcessBrowserTest {
 public:
  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(browser()->profile());
  }

 protected:
  autofill::test::AutofillBrowserTestEnvironment autofill_test_environment_;
};

IN_PROC_BROWSER_TEST_F(SigninPromoTabHelperTest,
                       CallPasswordMoveCallbackAfterSignInFromTab) {
  // Get the sign in tab with the correct access point.
  signin_ui_util::ShowSigninPromptFromPromo(
      browser()->profile(), signin_metrics::AccessPoint::kPasswordBubble);
  content::WebContents* sign_in_tab =
      signin_ui_util::GetSignInTabWithAccessPoint(
          browser(), signin_metrics::AccessPoint::kPasswordBubble);

  // Expect the callback for enabling account storage and moving the data to be
  // called.
  base::MockOnceClosure move_callback;
  EXPECT_CALL(move_callback, Run()).Times(1);

  SigninPromoTabHelper* user_data =
      SigninPromoTabHelper::GetForWebContents(*sign_in_tab);
  user_data->InitializeDataMoveAfterSignIn(
      /*move_callback=*/move_callback.Get(),
      /*access_point=*/signin_metrics::AccessPoint::kPasswordBubble);

  // Sign in, which will execute the callback.
  signin::MakeAccountAvailable(
      IdentityManagerFactory::GetForProfile(browser()->profile()),
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithAccessPoint(signin_metrics::AccessPoint::kPasswordBubble)
          .Build("test@gmail.com"));
}

IN_PROC_BROWSER_TEST_F(SigninPromoTabHelperTest,
                       CallAddressMoveCallbackAfterReauthenticationFromTab) {
  // Initiate sign in pending state.
  AccountInfo info = signin::MakePrimaryAccountAvailable(
      identity_manager(), "test@email.com", signin::ConsentLevel::kSignin);
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager());

  // Get the reauth tab with the correct access point.
  signin_ui_util::ShowReauthForPrimaryAccountWithAuthError(
      browser()->profile(), signin_metrics::AccessPoint::kAddressBubble);
  content::WebContents* reauth_tab =
      signin_ui_util::GetSignInTabWithAccessPoint(
          browser(), signin_metrics::AccessPoint::kAddressBubble);

  // Expect the callback for enabling account storage and moving the data to be
  // called.
  base::MockOnceClosure move_callback;
  EXPECT_CALL(move_callback, Run()).Times(1);

  SigninPromoTabHelper* user_data =
      SigninPromoTabHelper::GetForWebContents(*reauth_tab);
  user_data->InitializeDataMoveAfterSignIn(
      /*move_callback=*/move_callback.Get(),
      /*access_point=*/signin_metrics::AccessPoint::kAddressBubble);

  // Set a new refresh token for the primary account, which verifies the
  // user's identity and signs them back in. The callback will be executed.
  identity_manager()->GetAccountsMutator()->AddOrUpdateAccount(
      info.gaia, info.email, "dummy_refresh_token",
      /*is_under_advanced_protection=*/false,
      signin_metrics::AccessPoint::kAddressBubble,
      signin_metrics::SourceForRefreshTokenOperation::
          kDiceResponseHandler_Signin);
}
