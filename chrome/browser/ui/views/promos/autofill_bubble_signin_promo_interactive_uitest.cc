// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/autofill/autofill_signin_promo_tab_helper.h"
#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "chrome/browser/ui/views/passwords/password_save_update_view.h"
#include "chrome/browser/ui/views/promos/autofill_bubble_signin_promo_view.h"
#include "chrome/browser/ui/views/promos/bubble_signin_promo_signin_button_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/profile_waiter.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_auth_consumer.h"

namespace {

constexpr char kButton[] = "SignInButton";

MATCHER_P(FormMatches, form, "") {
  return form.signon_realm == arg.signon_realm && form.url == arg.url &&
         form.action == arg.action &&
         form.username_element == arg.username_element &&
         form.password_element == arg.password_element;
}

class AutofillBubbleSignInPromoInteractiveUITest : public ManagePasswordsTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    ManagePasswordsTest::SetUpInProcessBrowserTestFixture();
    url_loader_factory_helper_.SetUp();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &AutofillBubbleSignInPromoInteractiveUITest::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    // Create password stores.
    local_password_store_ = CreateAndUseTestPasswordStore(context);
    account_password_store_ = CreateAndUseTestAccountPasswordStore(context);
  }

  // Trigger the password save by simulating an "Accept" in the password bubble,
  // and wait for it to appear in the profile store.
  void SavePassword();

  // Perform a sign in with the password bubble access point and wait for an
  // event in the account store.
  void SignIn();

  // Returns true if the current tab's URL is a sign in URL.
  bool IsSignInURL();

  // Returns true if there is a primary account without a refresh token in
  // persistent error state.
  bool IsSignedIn();

  // Add additional account info for pixel tests.
  void ExtendAccountInfo(AccountInfo& info);

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return url_loader_factory_helper_.test_url_loader_factory();
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(browser()->profile());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kExplicitBrowserSigninUIOnDesktop};

  ChromeSigninClientWithURLLoaderHelper url_loader_factory_helper_;
  base::CallbackListSubscription create_services_subscription_;
  scoped_refptr<password_manager::TestPasswordStore> local_password_store_;
  scoped_refptr<password_manager::TestPasswordStore> account_password_store_;
};

void AutofillBubbleSignInPromoInteractiveUITest::SavePassword() {
  password_manager::PasswordStoreWaiter store_waiter(
      local_password_store_.get());

  PasswordBubbleViewBase* bubble =
      PasswordBubbleViewBase::manage_password_bubble();
  bubble->AcceptDialog();

  store_waiter.WaitOrReturn();
}

void AutofillBubbleSignInPromoInteractiveUITest::SignIn() {
  signin::MakeAccountAvailable(
      identity_manager(),
      signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
          .WithCookie()
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithAccessPoint(
              signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE)
          .Build("test@email.com"));
}

bool AutofillBubbleSignInPromoInteractiveUITest::IsSignInURL() {
  DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  return tab_helper->IsChromeSigninPage();
}

bool AutofillBubbleSignInPromoInteractiveUITest::IsSignedIn() {
  return signin_util::GetSignedInState(identity_manager()) ==
         signin_util::SignedInState::kSignedIn;
}

void AutofillBubbleSignInPromoInteractiveUITest::ExtendAccountInfo(
    AccountInfo& info) {
  info.given_name = "FirstName";
  info.full_name = "FirstName LastName";
  signin::UpdateAccountInfoForAccount(identity_manager(), info);
}

IN_PROC_BROWSER_TEST_F(AutofillBubbleSignInPromoInteractiveUITest,
                       SignInPromoNoAccountPresent) {
  // Set up password and password stores.
  GetController()->OnPasswordSubmitted(CreateFormManager(
      local_password_store_.get(), account_password_store_.get()));

  // Save the password and check that it was properly saved to profile store.
  SavePassword();
  EXPECT_EQ(1u, local_password_store_->stored_passwords().size());
  EXPECT_EQ(0u, account_password_store_->stored_passwords().size());

  // Wait for the bubble to be replaced with the sign in promo and click the
  // sign in button.
  RunTestSequence(
      WaitForEvent(BubbleSignInPromoSignInButtonView::kPromoSignInButton,
                   kBubbleSignInPromoSignInButtonHasCallback),
      EnsurePresent(PasswordSaveUpdateView::kPasswordBubble),
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshot can only run in pixel_tests on Windows."),
      Screenshot(PasswordSaveUpdateView::kPasswordBubble, std::string(),
                 "5455375"),
      NameChildViewByType<views::MdTextButton>(
          BubbleSignInPromoSignInButtonView::kPromoSignInButton, kButton),
      PressButton(kButton).SetMustRemainVisible(false),
      EnsureNotPresent(PasswordSaveUpdateView::kPasswordBubble));

  // Check that clicking the sign in button navigated to a sign in page.
  EXPECT_TRUE(IsSignInURL());

  // Check that there is a helper attached to the sign in tab, because the
  // password still needs to be moved.
  EXPECT_TRUE(autofill::AutofillSigninPromoTabHelper::GetForWebContents(
                  *browser()->tab_strip_model()->GetActiveWebContents())
                  ->IsInitializedForTesting());

  // Simulate a sign in event with the correct access point, which will move the
  // password. Wait for the password to show up in account store.
  auto account_store_waiter =
      password_manager::PasswordStoreWaiter(account_password_store_.get());
  SignIn();
  account_store_waiter.WaitOrReturn();

  // Check that the sign in was successful.
  EXPECT_TRUE(IsSignedIn());

  // Check that password was moved to account store.
  EXPECT_EQ(0u, local_password_store_->stored_passwords().size());
  EXPECT_EQ(1u, account_password_store_->stored_passwords().size());

  auto found = account_password_store_->stored_passwords().find(
      test_form()->signon_realm);
  EXPECT_NE(account_password_store_->stored_passwords().end(), found);
  EXPECT_THAT(found->second, testing::ElementsAre(FormMatches(*test_form())));
}

IN_PROC_BROWSER_TEST_F(AutofillBubbleSignInPromoInteractiveUITest,
                       SignInPromoWithWebSignedInAccount) {
  // Sign in with an account, but only on the web. The primary account is not
  // set.
  AccountInfo info = signin::MakeAccountAvailable(
      identity_manager(),
      signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
          .WithCookie()
          .WithAccessPoint(signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN)
          .Build("test@email.com"));
  ExtendAccountInfo(info);

  // Set up password and password stores.
  GetController()->OnPasswordSubmitted(CreateFormManager(
      local_password_store_.get(), account_password_store_.get()));

  // Save the password and check that it was properly saved to profile store.
  SavePassword();
  EXPECT_EQ(1u, local_password_store_->stored_passwords().size());
  EXPECT_EQ(0u, account_password_store_->stored_passwords().size());

  // Wait for the bubble to be replaced with the sign in promo and click the
  // sign in button. This should directly sign the user in and move the
  // password.
  auto account_store_waiter =
      password_manager::PasswordStoreWaiter(account_password_store_.get());
  RunTestSequence(
      WaitForEvent(BubbleSignInPromoSignInButtonView::kPromoSignInButton,
                   kBubbleSignInPromoSignInButtonHasCallback),
      EnsurePresent(PasswordSaveUpdateView::kPasswordBubble),
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshot can only run in pixel_tests on Windows."),
      Screenshot(PasswordSaveUpdateView::kPasswordBubble, std::string(),
                 "5455375"),
      NameChildViewByType<views::MdTextButton>(
          BubbleSignInPromoSignInButtonView::kPromoSignInButton, kButton),
      PressButton(kButton).SetMustRemainVisible(false),
      EnsureNotPresent(PasswordSaveUpdateView::kPasswordBubble));
  account_store_waiter.WaitOrReturn();

  // Check that there is no helper attached to the sign in tab, because the
  // password was already moved.
  EXPECT_FALSE(autofill::AutofillSigninPromoTabHelper::GetForWebContents(
                   *browser()->tab_strip_model()->GetActiveWebContents())
                   ->IsInitializedForTesting());

  // Check that the sign in was successful.
  EXPECT_TRUE(IsSignedIn());

  // Check that password was moved to account store.
  EXPECT_EQ(0u, local_password_store_->stored_passwords().size());
  EXPECT_EQ(1u, account_password_store_->stored_passwords().size());

  auto found = account_password_store_->stored_passwords().find(
      test_form()->signon_realm);
  EXPECT_NE(account_password_store_->stored_passwords().end(), found);
  EXPECT_THAT(found->second, testing::ElementsAre(FormMatches(*test_form())));
}

IN_PROC_BROWSER_TEST_F(AutofillBubbleSignInPromoInteractiveUITest,
                       SignInPromoWithAccountSignInPaused) {
  // Sign in with an account, and put its refresh token into an error
  // state. This simulates the "sign in paused" state.
  AccountInfo info = signin::MakePrimaryAccountAvailable(
      identity_manager(), "test@email.com", signin::ConsentLevel::kSignin);
  ExtendAccountInfo(info);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), info.account_id,
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::USER_NOT_SIGNED_UP));
  // Set up password and password stores.
  GetController()->OnPasswordSubmitted(CreateFormManager(
      local_password_store_.get(), account_password_store_.get()));

  // Save the password and check that it was properly saved to profile store.
  SavePassword();
  EXPECT_EQ(1u, local_password_store_->stored_passwords().size());
  EXPECT_EQ(0u, account_password_store_->stored_passwords().size());

  // Wait for the bubble to be replaced with the sign in promo and click
  // the sign in button.
  RunTestSequence(
      WaitForEvent(BubbleSignInPromoSignInButtonView::kPromoSignInButton,
                   kBubbleSignInPromoSignInButtonHasCallback),
      EnsurePresent(PasswordSaveUpdateView::kPasswordBubble),
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshot can only run in pixel_tests on Windows."),
      Screenshot(PasswordSaveUpdateView::kPasswordBubble, std::string(),
                 "5455375"),
      NameChildViewByType<views::MdTextButton>(
          BubbleSignInPromoSignInButtonView::kPromoSignInButton, kButton),
      PressButton(kButton).SetMustRemainVisible(false),
      EnsureNotPresent(PasswordSaveUpdateView::kPasswordBubble));

  // Check that clicking the sign in button navigated to a sign in page.
  EXPECT_TRUE(IsSignInURL());

  // Check that there is a helper attached to the sign in tab, because the
  // password still needs to be moved.
  EXPECT_TRUE(autofill::AutofillSigninPromoTabHelper::GetForWebContents(
                  *browser()->tab_strip_model()->GetActiveWebContents())
                  ->IsInitializedForTesting());
  EXPECT_FALSE(IsSignedIn());

  // Set a new refresh token for the primary account, which verifies the
  // user's identity and signs them back in. The password will be moved to
  // account store.
  auto account_store_waiter =
      password_manager::PasswordStoreWaiter(account_password_store_.get());
  identity_manager()->GetAccountsMutator()->AddOrUpdateAccount(
      info.gaia, info.email, "dummy_refresh_token",
      /*is_under_advanced_protection=*/false,
      signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE,
      signin_metrics::SourceForRefreshTokenOperation::
          kDiceResponseHandler_Signin);
  account_store_waiter.WaitOrReturn();

  // Check that the sign in was successful.
  EXPECT_TRUE(IsSignedIn());

  // Check that password was moved to account store.
  EXPECT_EQ(0u, local_password_store_->stored_passwords().size());
  EXPECT_EQ(1u, account_password_store_->stored_passwords().size());

  auto found = account_password_store_->stored_passwords().find(
      test_form()->signon_realm);
  EXPECT_NE(account_password_store_->stored_passwords().end(), found);
  EXPECT_THAT(found->second, testing::ElementsAre(FormMatches(*test_form())));
}

}  // namespace
