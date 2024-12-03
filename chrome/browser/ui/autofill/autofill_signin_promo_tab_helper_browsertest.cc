// // Copyright 2024 The Chromium Authors
// // Use of this source code is governed by a BSD-style license that can be
// // found in the LICENSE file.
#include "chrome/browser/ui/autofill/autofill_signin_promo_tab_helper.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"

class AutofillSigninPromoTabHelperTest : public InProcessBrowserTest {
 public:
  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(browser()->profile());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kImprovedSigninUIOnDesktop};
  autofill::test::AutofillBrowserTestEnvironment autofill_test_environment_;
};

IN_PROC_BROWSER_TEST_F(AutofillSigninPromoTabHelperTest,
                       CallPasswordMoveCallbackAfterSignInFromTab) {
  // Get the sign in tab with the correct access point.
  signin_ui_util::ShowSigninPromptFromPromo(
      browser()->profile(),
      signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE);
  content::WebContents* sign_in_tab =
      signin_ui_util::GetSignInTabWithAccessPoint(
          browser(), signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE);

  // Initialize the data move and expect a callback call.
  base::MockOnceCallback<void(content::WebContents*)> move_callback;
  EXPECT_CALL(move_callback, Run(sign_in_tab)).Times(1);

  autofill::AutofillSigninPromoTabHelper* user_data =
      autofill::AutofillSigninPromoTabHelper::GetForWebContents(*sign_in_tab);
  user_data->InitializeDataMoveAfterSignIn(
      /*move_callback=*/move_callback.Get(),
      /*access_point=*/signin_metrics::AccessPoint::
          ACCESS_POINT_PASSWORD_BUBBLE);

  // Sign in, which will execute the callback.
  signin::MakeAccountAvailable(
      IdentityManagerFactory::GetForProfile(browser()->profile()),
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithAccessPoint(
              signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE)
          .Build("test@gmail.com"));
}

IN_PROC_BROWSER_TEST_F(AutofillSigninPromoTabHelperTest,
                       CallAddressMoveCallbackAfterReauthenticationFromTab) {
  // Initiate sign in pending state.
  AccountInfo info = signin::MakePrimaryAccountAvailable(
      identity_manager(), "test@email.com", signin::ConsentLevel::kSignin);
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager());

  // Get the reauth tab with the correct access point.
  signin_ui_util::ShowReauthForPrimaryAccountWithAuthError(
      browser()->profile(),
      signin_metrics::AccessPoint::ACCESS_POINT_ADDRESS_BUBBLE);
  content::WebContents* reauth_tab =
      signin_ui_util::GetSignInTabWithAccessPoint(
          browser(), signin_metrics::AccessPoint::ACCESS_POINT_ADDRESS_BUBBLE);

  // Initialize the data move and expect a callback call.
  base::MockOnceCallback<void(content::WebContents*)> move_callback;
  EXPECT_CALL(move_callback, Run(reauth_tab)).Times(1);

  autofill::AutofillSigninPromoTabHelper* user_data =
      autofill::AutofillSigninPromoTabHelper::GetForWebContents(*reauth_tab);
  user_data->InitializeDataMoveAfterSignIn(
      /*move_callback=*/move_callback.Get(),
      /*access_point=*/signin_metrics::AccessPoint::
          ACCESS_POINT_ADDRESS_BUBBLE);

  // Set a new refresh token for the primary account, which verifies the
  // user's identity and signs them back in. The callback will be executed.
  identity_manager()->GetAccountsMutator()->AddOrUpdateAccount(
      info.gaia, info.email, "dummy_refresh_token",
      /*is_under_advanced_protection=*/false,
      signin_metrics::AccessPoint::ACCESS_POINT_ADDRESS_BUBBLE,
      signin_metrics::SourceForRefreshTokenOperation::
          kDiceResponseHandler_Signin);
}
