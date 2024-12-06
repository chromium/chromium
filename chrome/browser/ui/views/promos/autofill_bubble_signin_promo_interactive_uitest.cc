// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/autofill/autofill_signin_promo_tab_helper.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/views/autofill/address_sign_in_promo_view.h"
#include "chrome/browser/ui/views/autofill/save_address_profile_view.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "chrome/browser/ui/views/passwords/password_save_update_view.h"
#include "chrome/browser/ui/views/promos/autofill_bubble_signin_promo_view.h"
#include "chrome/browser/ui/views/promos/bubble_signin_promo_signin_button_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/profile_waiter.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

using autofill::AddressDataManager;
using autofill::AddressSignInPromoView;
using autofill::AutofillProfile;
using autofill::ContentAutofillClient;
using autofill::PersonalDataManager;
using autofill::SaveAddressProfileView;

class MockAddressDataManagerObserver : public AddressDataManager::Observer {
 public:
  MOCK_METHOD(void, OnAddressDataChanged, (), (override));
};

constexpr char kButton[] = "SignInButton";

MATCHER_P(FormMatches, form, "") {
  return form.signon_realm == arg.signon_realm && form.url == arg.url &&
         form.action == arg.action &&
         form.username_element == arg.username_element &&
         form.password_element == arg.password_element;
}

MATCHER_P(AddressMatches, address, "") {
  return arg->Compare(address) == 0;
}

}  // namespace

class AutofillBubbleSignInPromoInteractiveUITest : public ManagePasswordsTest {
 public:
  DECLARE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(kAddressDataChanged);

  void SetUpInProcessBrowserTestFixture() override {
    ManagePasswordsTest::SetUpInProcessBrowserTestFixture();
    url_loader_factory_helper_.SetUp();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &AutofillBubbleSignInPromoInteractiveUITest::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop,
                              switches::kImprovedSigninUIOnDesktop},
        /*disabled_features=*/{});
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    // Create password stores.
    local_password_store_ = CreateAndUseTestPasswordStore(context);
    account_password_store_ = CreateAndUseTestAccountPasswordStore(context);
  }

  void PreRunTestOnMainThread() override {
    ManagePasswordsTest::PreRunTestOnMainThread();

    // Set the sync service to be signed out by default.
    ConfigurePasswordSync(SyncConfiguration::kNotSyncing);
  }

  // Trigger the password save by simulating an "Accept" in the password bubble,
  // and wait for it to appear in the profile store.
  void SavePassword();

  // Trigger the address save bubble. This does not save the address yet.
  void TriggerSaveAddressBubble(const AutofillProfile& address);

  // Perform a sign in with the `access_point`.
  void SignIn(signin_metrics::AccessPoint access_point);

  // Returns true if the current tab's URL is a sign in URL.
  bool IsSignInURL();

  // Returns true if there is a primary account without a refresh token in
  // persistent error state.
  bool IsSignedIn();

  // This is needed because the TestSyncService will not automatically become
  // available upon sign in.
  void ActivateSyncService(AccountInfo& info) {
    static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(browser()->profile()))
        ->SetSignedIn(signin::ConsentLevel::kSignin, info);
  }

  // Add additional account info for pixel tests.
  void ExtendAccountInfo(AccountInfo& info);

  std::vector<const AutofillProfile*> local_addresses() const {
    return address_data_manager().GetProfilesByRecordType(
        AutofillProfile::RecordType::kLocalOrSyncable);
  }

  std::vector<const AutofillProfile*> account_addresses() const {
    return address_data_manager().GetProfilesByRecordType(
        AutofillProfile::RecordType::kAccount);
  }

  ContentAutofillClient& client() const {
    return *ContentAutofillClient::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  AddressDataManager& address_data_manager() const {
    return client().GetPersonalDataManager().address_data_manager();
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return url_loader_factory_helper_.test_url_loader_factory();
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(browser()->profile());
  }

  void OnAddressDataChanged();

  void SaveAddress(autofill::AutofillClient::AddressPromptUserDecision decision,
                   base::optional_ref<const AutofillProfile> profile);

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

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

void AutofillBubbleSignInPromoInteractiveUITest::TriggerSaveAddressBubble(
    const AutofillProfile& address) {
  client().ConfirmSaveAddressProfile(
      address, nullptr, false,
      base::BindOnce(&AutofillBubbleSignInPromoInteractiveUITest::SaveAddress,
                     base::Unretained(this)));
}

void AutofillBubbleSignInPromoInteractiveUITest::SignIn(
    signin_metrics::AccessPoint access_point) {
  AccountInfo info = signin::MakeAccountAvailable(
      identity_manager(),
      signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
          .WithCookie()
          .WithAccessPoint(access_point)
          .Build("test@email.com"));

  ActivateSyncService(info);
  identity_manager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      info.account_id, signin::ConsentLevel::kSignin, access_point);
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

void AutofillBubbleSignInPromoInteractiveUITest::OnAddressDataChanged() {
  views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
      kAddressDataChanged, BrowserView::GetBrowserViewForBrowser(browser()));
}

void AutofillBubbleSignInPromoInteractiveUITest::SaveAddress(
    autofill::AutofillClient::AddressPromptUserDecision decision,
    base::optional_ref<const AutofillProfile> profile) {
  address_data_manager().AddProfile(*profile);
}

DEFINE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(
    AutofillBubbleSignInPromoInteractiveUITest,
    kAddressDataChanged);

/////////////////////////////////////////////////////////////////
///// Password Sign in Promo

IN_PROC_BROWSER_TEST_F(AutofillBubbleSignInPromoInteractiveUITest,
                       PasswordSignInPromoNoAccountPresent) {
  base::HistogramTester histogram_tester;
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
  SignIn(signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE);
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

  // Signin metrics - Offered/Started/Completed are recorded, but no values for
  // WebSignin (WithDefault).
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Offered",
      signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE, 1);
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Offered.NewAccountNoExistingAccount",
      signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE, 1);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Offered.WithDefault", 0);
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Started",
      signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE, 1);
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Completed",
      signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE, 1);
  histogram_tester.ExpectTotalCount("Signin.WebSignin.SourceToChromeSignin", 0);
}

IN_PROC_BROWSER_TEST_F(AutofillBubbleSignInPromoInteractiveUITest,
                       PasswordSignInPromoWithWebSignedInAccount) {
  base::HistogramTester histogram_tester;
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
  ActivateSyncService(info);
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

  // Signin metrics - WebSignin (WithDefault) metrics are also recorded.
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Offered",
      signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE, 1);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Started", 0);
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Completed",
      signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE, 1);
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Offered",
      signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE, 1);
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Offered.WithDefault",
      signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.SignIn.Offered.NewAccountNoExistingAccount", 0);
  histogram_tester.ExpectBucketCount(
      "Signin.WebSignin.SourceToChromeSignin",
      signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE, 1);
}

IN_PROC_BROWSER_TEST_F(AutofillBubbleSignInPromoInteractiveUITest,
                       PasswordSignInPromoWithAccountSignInPending) {
  // Sign in with an account, and put its refresh token into an error
  // state. This simulates the "sign in pending" state.
  AccountInfo info = signin::MakePrimaryAccountAvailable(
      identity_manager(), "test@email.com", signin::ConsentLevel::kSignin);
  ExtendAccountInfo(info);
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager());

  // Set up password and password stores.
  GetController()->OnPasswordSubmitted(CreateFormManager(
      local_password_store_.get(), account_password_store_.get()));

  // Start recording metrics after signing in.
  base::HistogramTester histogram_tester;

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
  ActivateSyncService(info);
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

  // Signin metrics - nothing should be recorded for reauth.
  histogram_tester.ExpectTotalCount("Signin.SignIn.Offered", 0);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Started", 0);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Completed", 0);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Offered.WithDefault", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.SignIn.Offered.NewAccountNoExistingAccount", 0);
  histogram_tester.ExpectTotalCount("Signin.WebSignin.SourceToChromeSignin", 0);
}

/////////////////////////////////////////////////////////////////
///// Address Sign in Promo

IN_PROC_BROWSER_TEST_F(AutofillBubbleSignInPromoInteractiveUITest,
                       AddressSignInPromoNoAccountPresent) {
  // Trigger the address save bubble.
  AutofillProfile address = autofill::test::GetFullProfile();
  TriggerSaveAddressBubble(address);

  // Set up observer in order to ensure that `OnAddressDataChanged` is called
  // twice. Fire an event the first time it is called, as this is coming from
  // when the first address save bubble was accepted. The second time it is
  // called will be for the address migration.
  testing::NiceMock<MockAddressDataManagerObserver> observer;
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnAddressDataChanged)
      .Times(2)
      .WillOnce([&] { OnAddressDataChanged(); })
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  base::ScopedObservation<AddressDataManager, AddressDataManager::Observer>
      observation{&observer};
  observation.Observe(&address_data_manager());

  // Accept the save bubble, wait for it to be replaced with the sign in promo
  // and click the sign in button.
  RunTestSequence(
      PressButton(views::DialogClientView::kOkButtonElementId),
      InParallel(
          WaitForEvent(kBrowserViewElementId, kAddressDataChanged),
          WaitForEvent(BubbleSignInPromoSignInButtonView::kPromoSignInButton,
                       kBubbleSignInPromoSignInButtonHasCallback)),
      EnsureNotPresent(SaveAddressProfileView::kTopViewId),
      EnsurePresent(AddressSignInPromoView::kBubbleFrameViewId),
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshot can only run in pixel_tests on Windows."),
      Screenshot(AddressSignInPromoView::kBubbleFrameViewId, std::string(),
                 "5860426"),
      NameChildViewByType<views::MdTextButton>(
          BubbleSignInPromoSignInButtonView::kPromoSignInButton, kButton),
      PressButton(kButton).SetMustRemainVisible(false),
      EnsureNotPresent(AddressSignInPromoView::kBubbleFrameViewId));

  // Check that address was saved to local store.
  EXPECT_EQ(1u, local_addresses().size());
  EXPECT_EQ(0u, account_addresses().size());

  // Check that clicking the sign in button navigated to a sign in page.
  EXPECT_TRUE(IsSignInURL());

  // Check that there is a helper attached to the sign in tab, because the
  // address still needs to be moved.
  EXPECT_TRUE(autofill::AutofillSigninPromoTabHelper::GetForWebContents(
                  *browser()->tab_strip_model()->GetActiveWebContents())
                  ->IsInitializedForTesting());

  // Simulate a sign in event with the correct access point, which will move the
  // address.
  SignIn(signin_metrics::AccessPoint::ACCESS_POINT_ADDRESS_BUBBLE);

  // Wait for the address to be moved.
  run_loop.Run();

  // Check that the sign in was successful.
  EXPECT_TRUE(IsSignedIn());

  // Check that the address was moved to account store.
  EXPECT_EQ(0u, local_addresses().size());
  EXPECT_EQ(1u, account_addresses().size());
  EXPECT_THAT(account_addresses(),
              testing::ElementsAre(AddressMatches(address)));
}

IN_PROC_BROWSER_TEST_F(AutofillBubbleSignInPromoInteractiveUITest,
                       AddressSignInPromoWithWebSignedInAccount) {
  // Sign in with an account, but only on the web. The primary account is not
  // set.
  AccountInfo info = signin::MakeAccountAvailable(
      identity_manager(),
      signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
          .WithCookie()
          .WithAccessPoint(signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN)
          .Build("test@email.com"));
  ExtendAccountInfo(info);

  // Trigger the address save bubble.
  AutofillProfile address = autofill::test::GetFullProfile();
  TriggerSaveAddressBubble(address);

  // Set up observer in order to ensure that `OnAddressDataChanged` is called
  // twice. Fire an event the first time it is called, as this is coming from
  // when the first address save bubble was accepted. The second time it is
  // called will be for the address migration.
  testing::NiceMock<MockAddressDataManagerObserver> observer;
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnAddressDataChanged)
      .Times(2)
      .WillOnce([&] { OnAddressDataChanged(); })
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  base::ScopedObservation<AddressDataManager, AddressDataManager::Observer>
      observation{&observer};
  observation.Observe(&address_data_manager());

  // Accept the save bubble, wait for the save bubble to be replaced with the
  // sign in promo and click the sign in button. This should directly sign the
  // user in and move the address.
  ActivateSyncService(info);
  RunTestSequence(
      PressButton(views::DialogClientView::kOkButtonElementId),
      InParallel(
          WaitForEvent(kBrowserViewElementId, kAddressDataChanged),
          WaitForEvent(BubbleSignInPromoSignInButtonView::kPromoSignInButton,
                       kBubbleSignInPromoSignInButtonHasCallback)),
      EnsureNotPresent(SaveAddressProfileView::kTopViewId),
      EnsurePresent(AddressSignInPromoView::kBubbleFrameViewId),
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshot can only run in pixel_tests on Windows."),
      Screenshot(AddressSignInPromoView::kBubbleFrameViewId, std::string(),
                 "5860426"),
      NameChildViewByType<views::MdTextButton>(
          BubbleSignInPromoSignInButtonView::kPromoSignInButton, kButton),
      PressButton(kButton).SetMustRemainVisible(false),
      EnsureNotPresent(AddressSignInPromoView::kBubbleFrameViewId));

  // Wait for the address to be moved.
  run_loop.Run();

  // Check that there is no helper attached to the sign in tab, because the
  // password was already moved.
  EXPECT_FALSE(autofill::AutofillSigninPromoTabHelper::GetForWebContents(
                   *browser()->tab_strip_model()->GetActiveWebContents())
                   ->IsInitializedForTesting());

  // Check that the sign in was successful.
  EXPECT_TRUE(IsSignedIn());

  // Check that the address was moved to account store.
  EXPECT_EQ(0u, local_addresses().size());
  EXPECT_EQ(1u, account_addresses().size());
  EXPECT_THAT(account_addresses(),
              testing::ElementsAre(AddressMatches(address)));
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_AddressSignInPromoWithAccountSignInPending \
  DISABLED_AddressSignInPromoWithAccountSignInPending
#else
#define MAYBE_AddressSignInPromoWithAccountSignInPending \
  AddressSignInPromoWithAccountSignInPending
#endif

IN_PROC_BROWSER_TEST_F(AutofillBubbleSignInPromoInteractiveUITest,
                       MAYBE_AddressSignInPromoWithAccountSignInPending) {
  // Sign in with an account, and put its refresh token into an error
  // state. This simulates the "sign in pending" state.
  AccountInfo info = signin::MakePrimaryAccountAvailable(
      identity_manager(), "test@email.com", signin::ConsentLevel::kSignin);
  ExtendAccountInfo(info);
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager());

  // Trigger the address save bubble.
  AutofillProfile address = autofill::test::GetFullProfile();
  TriggerSaveAddressBubble(address);

  // Set up observer in order to ensure that `OnAddressDataChanged` is called
  // twice. Fire an event the first time it is called, as this is coming from
  // when the first address save bubble was accepted. The second time it is
  // called will be for the address migration.
  testing::NiceMock<MockAddressDataManagerObserver> observer;
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnAddressDataChanged)
      .Times(2)
      .WillOnce([&] { OnAddressDataChanged(); })
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  base::ScopedObservation<AddressDataManager, AddressDataManager::Observer>
      observation{&observer};
  observation.Observe(&address_data_manager());

  // Accept the save bubble, wait for the save bubble to be replaced with the
  // sign in promo and click the sign in button.
  RunTestSequence(
      PressButton(views::DialogClientView::kOkButtonElementId),
      InParallel(
          WaitForEvent(kBrowserViewElementId, kAddressDataChanged),
          WaitForEvent(BubbleSignInPromoSignInButtonView::kPromoSignInButton,
                       kBubbleSignInPromoSignInButtonHasCallback)),
      EnsureNotPresent(SaveAddressProfileView::kTopViewId),
      EnsurePresent(AddressSignInPromoView::kBubbleFrameViewId),
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshot can only run in pixel_tests on Windows."),
      Screenshot(AddressSignInPromoView::kBubbleFrameViewId, std::string(),
                 "5860426"),
      NameChildViewByType<views::MdTextButton>(
          BubbleSignInPromoSignInButtonView::kPromoSignInButton, kButton),
      PressButton(kButton).SetMustRemainVisible(false),
      EnsureNotPresent(AddressSignInPromoView::kBubbleFrameViewId));

  // Check that address was saved to local store.
  EXPECT_EQ(1u, local_addresses().size());
  EXPECT_EQ(0u, account_addresses().size());

  // Check that clicking the sign in button navigated to a sign in page.
  EXPECT_TRUE(IsSignInURL());

  // Check that there is a helper attached to the sign in tab, because the
  // address still needs to be moved.
  EXPECT_TRUE(autofill::AutofillSigninPromoTabHelper::GetForWebContents(
                  *browser()->tab_strip_model()->GetActiveWebContents())
                  ->IsInitializedForTesting());

  // Set a new refresh token for the primary account, which verifies the
  // user's identity and signs them back in. The address will be moved to
  // account store.
  ActivateSyncService(info);
  identity_manager()->GetAccountsMutator()->AddOrUpdateAccount(
      info.gaia, info.email, "dummy_refresh_token",
      /*is_under_advanced_protection=*/false,
      signin_metrics::AccessPoint::ACCESS_POINT_ADDRESS_BUBBLE,
      signin_metrics::SourceForRefreshTokenOperation::
          kDiceResponseHandler_Signin);

  // Wait for the address to be moved.
  run_loop.Run();

  // Check that the sign in was successful.
  EXPECT_TRUE(IsSignedIn());

  // Check that the address was moved to account store.
  EXPECT_EQ(0u, local_addresses().size());
  EXPECT_EQ(1u, account_addresses().size());
  EXPECT_THAT(account_addresses(),
              testing::ElementsAre(AddressMatches(address)));
}
