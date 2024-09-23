// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/autofill/autofill_bubble_signin_promo_controller.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"

class AutofillBubbleSignInPromoControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override;

  autofill::AutofillBubbleSignInPromoController* autofill_bubble_controller() {
    return autofill_bubble_controller_.get();
  }

  base::MockCallback<base::OnceCallback<void(content::WebContents*)>>
      mock_callback_;

 private:
  base::test::ScopedFeatureList feature_list_{
      switches::kExplicitBrowserSigninUIOnDesktop};
  std::unique_ptr<autofill::AutofillBubbleSignInPromoController>
      autofill_bubble_controller_;
};

void AutofillBubbleSignInPromoControllerTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  autofill_bubble_controller_ =
      std::make_unique<autofill::AutofillBubbleSignInPromoController>(
          *web_contents(),
          signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE,
          mock_callback_.Get());
}

TEST_F(AutofillBubbleSignInPromoControllerTest,
       RunsCallbackUponSignInWithExistingAccount) {
  // Simulate a sign in to the web.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());

  AccountInfo account_info = signin::MakeAccountAvailable(
      identity_manager,
      signin::AccountAvailabilityOptionsBuilder()
          .WithAccessPoint(signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN)
          .Build("test@gmail.com"));

  // Check that the move callback will be called.
  EXPECT_CALL(mock_callback_, Run(web_contents()));

  // This should sign in the user to Chrome directly, as there is already an
  // account on the web.
  autofill_bubble_controller()->OnSignInToChromeClicked(account_info);

  // Check that the user was signed in to Chrome.
  EXPECT_TRUE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
}
