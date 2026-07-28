// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/signin/signin_utils_handler.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_ui_delegate.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockSigninUiDelegate : public signin_ui_util::SigninUiDelegate {
 public:
  MOCK_METHOD(void,
              ShowSigninUI,
              (Profile*,
               bool,
               signin_metrics::AccessPoint,
               signin_metrics::PromoAction,
               const std::string&),
              (override));
  MOCK_METHOD(void,
              ShowReauthUI,
              (Profile*,
               const std::string&,
               bool,
               signin_metrics::AccessPoint,
               signin_metrics::PromoAction),
              (override));
  MOCK_METHOD(void,
              ShowCrossDeviceSigninQrBubble,
              (BrowserWindowInterface*, base::OnceClosure),
              (override));
};

std::unique_ptr<KeyedService> BuildTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

}  // namespace

class SigninUtilsHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  TestingProfile::TestingFactories GetTestingFactories() const override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());

    sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildTestSyncService)));

    handler_ = std::make_unique<SigninUtilsHandler>(
        remote_.BindNewPipeAndPassReceiver(), profile());
  }

  void TearDown() override {
    handler_.reset();
    sync_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  SigninUtilsHandler* handler() { return handler_.get(); }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

 protected:
  mojo::Remote<signin::mojom::SigninPageHandler> remote_;
  std::unique_ptr<SigninUtilsHandler> handler_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  raw_ptr<syncer::TestSyncService> sync_service_;
};

TEST_F(SigninUtilsHandlerTest, StartSignin) {
  testing::StrictMock<MockSigninUiDelegate> mock_signin_ui_delegate;
  base::AutoReset<signin_ui_util::SigninUiDelegate*> delegate_auto_reset =
      signin_ui_util::SetSigninUiDelegateForTesting(&mock_signin_ui_delegate);

  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));

  EXPECT_CALL(mock_signin_ui_delegate,
              ShowSigninUI(profile(), /*enable_sync=*/true,
                           signin_metrics::AccessPoint::kSettings,
                           signin_metrics::PromoAction::
                               PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT,
                           /*extension_name=*/""));

  handler()->StartSignin(signin::mojom::ChromeSigninAccessPoint::SETTINGS);
}

TEST_F(SigninUtilsHandlerTest, HandleStartSigninManaged) {
  testing::StrictMock<MockSigninUiDelegate> mock_signin_ui_delegate;
  base::AutoReset<signin_ui_util::SigninUiDelegate*> delegate_auto_reset =
      signin_ui_util::SetSigninUiDelegateForTesting(&mock_signin_ui_delegate);

  const char kManagedEmail[] = "user@managedchrome.com";
  AccountInfo account = identity_test_env()->MakePrimaryAccountAvailable(
      kManagedEmail, signin::ConsentLevel::kSignin);
  // Make the account managed and disallow signout.
  account = AccountInfo::Builder(account)
                .SetHostedDomain("managedchrome.com")
                .Build();
  AccountCapabilitiesTestMutator mutator(&account);
  mutator.set_is_subject_to_enterprise_features(true);
  identity_test_env()->UpdateAccountInfoForAccount(account);
  SigninClient* client = ChromeSigninClientFactory::GetForProfile(profile());
  client->set_is_clear_primary_account_allowed_for_testing(
      SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED);
  ASSERT_FALSE(client->IsClearPrimaryAccountAllowed());

  // Inject the error.
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();

  EXPECT_CALL(
      mock_signin_ui_delegate,
      ShowReauthUI(profile(), kManagedEmail, /*enable_sync=*/false,
                   signin_metrics::AccessPoint::kSettings,
                   signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO));

  handler()->StartSignin(signin::mojom::ChromeSigninAccessPoint::SETTINGS);
}

TEST_F(SigninUtilsHandlerTest, SigninWithAccount) {
  const char kEmail[] = "user@example.com";
  identity_test_env()->MakeAccountAvailable(kEmail);
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));

  handler()->SigninWithAccount(signin::mojom::ChromeSigninAccessPoint::SETTINGS,
                               kEmail,
                               /*is_default_promo_account=*/true);

  EXPECT_TRUE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
}

TEST_F(SigninUtilsHandlerTest, RecordSigninOffered) {
  base::HistogramTester histogram_tester;

  // Test with no accounts (NewAccountNoExistingAccount)
  {
    handler()->RecordSigninOffered(
        signin::mojom::ChromeSigninAccessPoint::SETTINGS);

    histogram_tester.ExpectUniqueSample(
        "Signin.SignIn.Offered", signin_metrics::AccessPoint::kSettings, 1);
    histogram_tester.ExpectUniqueSample(
        "Signin.SignIn.Offered.NewAccountNoExistingAccount",
        signin_metrics::AccessPoint::kSettings, 1);
  }

  // Test with an account (WithDefault)
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      "user2@example.com", signin::ConsentLevel::kSignin);

  {
    handler()->RecordSigninOffered(
        signin::mojom::ChromeSigninAccessPoint::SETTINGS_YOUR_SAVED_INFO);

    histogram_tester.ExpectBucketCount(
        "Signin.SignIn.Offered",
        signin_metrics::AccessPoint::kSettingsYourSavedInfo, 1);
    histogram_tester.ExpectBucketCount(
        "Signin.SignIn.Offered.WithDefault",
        signin_metrics::AccessPoint::kSettingsYourSavedInfo, 1);
  }
}

TEST_F(SigninUtilsHandlerTest, RecordSigninPendingOffered) {
  base::HistogramTester histogram_tester;

  handler()->RecordSigninPendingOffered();

  histogram_tester.ExpectUniqueSample("Signin.SigninPending.Offered",
                                      signin_metrics::AccessPoint::kSettings,
                                      1);
}
