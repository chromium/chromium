// // Copyright 2024 The Chromium Authors
// // Use of this source code is governed by a BSD-style license that can be
// // found in the LICENSE file.
#include "chrome/browser/ui/autofill/autofill_signin_promo_tab_helper.h"

#include "chrome/browser/password_manager/password_manager_test_base.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"

namespace {

class AutofillSigninPromoTabHelperTest : public PasswordManagerBrowserTestBase {
 public:
  AutofillSigninPromoTabHelperTest() = default;

  AutofillSigninPromoTabHelperTest(const AutofillSigninPromoTabHelperTest&) =
      delete;
  AutofillSigninPromoTabHelperTest& operator=(
      const AutofillSigninPromoTabHelperTest&) = delete;

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&AutofillSigninPromoTabHelperTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    // Create password stores.
    local_password_store_ = CreateAndUseTestPasswordStore(context);
    account_password_store_ = CreateAndUseTestAccountPasswordStore(context);
  }

  password_manager::PasswordForm MakePasswordForm(
      const std::string& signon_realm) {
    password_manager::PasswordForm form;
    form.url = GURL("http://www.origin.com");
    form.username_element = u"username_element";
    form.username_value = u"username_value";
    form.password_element = u"password_element";
    form.password_value = u"password_value";
    form.in_store = password_manager::PasswordForm::Store::kProfileStore;
    form.signon_realm = signon_realm;
    return form;
  }

 protected:
  const std::string kSignonRealm = "https://example.test";
  base::CallbackListSubscription create_services_subscription_;
  scoped_refptr<password_manager::TestPasswordStore> local_password_store_;
  scoped_refptr<password_manager::TestPasswordStore> account_password_store_;
};

MATCHER_P(FormMatches, form, "") {
  return form.signon_realm == arg.signon_realm && form.url == arg.url &&
         form.action == arg.action &&
         form.username_element == arg.username_element &&
         form.username_value == arg.username_value &&
         form.password_element == arg.password_element &&
         form.password_value == arg.password_value;
}

IN_PROC_BROWSER_TEST_F(AutofillSigninPromoTabHelperTest,
                       MoveSavedPasswordToAccountStoreNoExistingAccount) {
  // Create password form.
  password_manager::PasswordForm form = MakePasswordForm(kSignonRealm);

  // Save password to local store.
  auto profile_store_waiter =
      password_manager::PasswordStoreWaiter(local_password_store_.get());
  local_password_store_->AddLogin(form);
  profile_store_waiter.WaitOrReturn();

  EXPECT_EQ(1u, local_password_store_->stored_passwords().size());
  EXPECT_EQ(0u, account_password_store_->stored_passwords().size());

  // Get the sign in tab with the correct access point.
  signin_ui_util::ShowSigninPromptFromPromo(
      browser()->profile(),
      signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE);
  content::WebContents* sign_in_tab =
      signin_ui_util::GetSignInTabWithAccessPoint(
          browser(), signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE);

  // The identity manager is observed by AutofillSigninPromoTabHelper, which
  // will move the password once the primary account has changed.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());

  // Pass the password form to the AutofillSigninPromoTabHelper and start
  // observing the identity manager.
  autofill::AutofillSigninPromoTabHelper* user_data =
      autofill::AutofillSigninPromoTabHelper::GetForWebContents(*sign_in_tab);
  user_data->InitializeDataMoveAfterSignIn(
      /*move_callback=*/base::BindOnce(
          [](const password_manager::PasswordForm& form,
             password_manager::metrics_util::MoveToAccountStoreTrigger trigger,
             content::WebContents* web_contents) {
            PasswordsModelDelegateFromWebContents(web_contents)
                ->MovePendingPasswordToAccountStoreUsingHelper(form, trigger);
          },
          form,
          password_manager::metrics_util::MoveToAccountStoreTrigger::
              kUserOptedInAfterSavingLocally),
      /*access_point=*/signin_metrics::AccessPoint::
          ACCESS_POINT_PASSWORD_BUBBLE);

  auto account_store_waiter =
      password_manager::PasswordStoreWaiter(account_password_store_.get());
  signin::MakeAccountAvailable(
      identity_manager,
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithAccessPoint(
              signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE)
          .Build("test@gmail.com"));
  account_store_waiter.WaitOrReturn();

  // Check that password was moved to account store.
  EXPECT_EQ(0u, local_password_store_->stored_passwords().size());
  EXPECT_EQ(1u, account_password_store_->stored_passwords().size());

  auto found = account_password_store_->stored_passwords().find(kSignonRealm);
  ASSERT_NE(account_password_store_->stored_passwords().end(), found);
  EXPECT_THAT(found->second, testing::ElementsAre(FormMatches(form)));
}

}  // namespace
