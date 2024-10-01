// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/ash/signin_helper.h"

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/account_manager/account_apps_availability.h"
#include "chrome/browser/ash/account_manager/account_apps_availability_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_mojo_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

namespace ash {

class SigninHelperTest;

namespace {

const char kFakePrimaryGaiaId[] = "primary_account_gaia";
const char kFakePrimaryEmail[] = "primary@example.com";
const char kFakeGaiaId[] = "fake_gaia_id";
const char kFakeEmail[] = "fake_email@gmail.com";
const char kFakeAuthCode[] = "fake_auth_code";
const char kFakeDeviceId[] = "fake_device_id";
const char kFakeRefreshToken[] = "fake_refresh_token";
const char kFakeEnterpriseGaiaId[] = "fake_enterprise_gaia_id";
const char kFakeEnterpriseEmail[] = "fake_enterprise@example.com";
const char kFakeEnterpriseDomain[] = "example.com";

const char kSecureConnectApiGetSecondaryGoogleAccountUsageURL[] =
    "https://secureconnect-pa.clients6.google.com/"
    "v1:getManagedAccountsSigninRestriction?policy_name="
    "SecondaryGoogleAccountUsage";

const char kSecureConnectApiGetSecondaryAccountAllowedInArcPolicyURL[] =
    "https://secureconnect-pa.clients6.google.com/"
    "v1:getManagedAccountsSigninRestriction?policy_name="
    "SecondaryAccountAllowedInArcPolicy";

// Fake responses for the URL requests that are part of the sign-in flow.
const char kOnClientOAuthSuccessBody[] =
    R"({
            "refresh_token": "refresh_token",
            "access_token": "access_token",
            "expires_in": 99999
       })";
const char kUserInfoURLBodyWithHostedDomain[] = R"({"hd": "%s"})";
const char kUserInfoURLBodyWithoutHostedDomain[] = R"({})";
const char kSecureConnectApiGetSecondaryGoogleAccountUsageURLBody[] =
    R"({"policyValue": "%s"})";

constexpr char kSecondaryGoogleAccountUsageHistogramName[] =
    "Enterprise.SecondaryGoogleAccountUsage.PolicyFetch.Status";
constexpr char kSecondaryGoogleAccountUsageLatencyHistogramName[] =
    "Enterprise.SecondaryGoogleAccountUsage.PolicyFetch.ResponseLatency";

void NotReached() {
  NOTREACHED_IN_MIGRATION();
}

class TestSigninHelper : public SigninHelper {
 public:
  TestSigninHelper(
      const base::RepeatingClosure& delete_closure,
      account_manager::AccountManager* account_manager,
      crosapi::AccountManagerMojoService* account_manager_mojo_service,
      const base::RepeatingClosure& close_dialog_closure,
      const base::RepeatingCallback<
          void(const std::string&, const std::string&)>& show_signin_error,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<ArcHelper> arc_helper,
      const std::string& gaia_id,
      const std::string& email,
      const std::string& auth_code,
      const std::string& signin_scoped_device_id)
      : SigninHelper(account_manager,
                     account_manager_mojo_service,
                     close_dialog_closure,
                     show_signin_error,
                     url_loader_factory,
                     std::move(arc_helper),
                     gaia_id,
                     email,
                     auth_code,
                     signin_scoped_device_id) {
    delete_closure_ = std::move(delete_closure);
  }

  ~TestSigninHelper() override;

 private:
  base::RepeatingClosure delete_closure_;
};

TestSigninHelper::~TestSigninHelper() {
  std::move(delete_closure_).Run();
}

}  // namespace

class SigninHelperTest : public InProcessBrowserTest,
                         public account_manager::AccountManager::Observer {
 public:
  SigninHelperTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  void SetUpOnMainThread() override {
    auto* profile = browser()->profile();
    auto* factory =
        g_browser_process->platform_part()->GetAccountManagerFactory();
    account_manager_ = factory->GetAccountManager(profile->GetPath().value());
    account_manager_mojo_service_ =
        factory->GetAccountManagerMojoService(profile->GetPath().value());
    account_manager_->SetUrlLoaderFactoryForTests(shared_url_loader_factory());
    account_manager_->AddObserver(this);

    // Setup the main account:
    account_manager::AccountKey kPrimaryAccountKey{
        kFakePrimaryGaiaId, account_manager::AccountType::kGaia};
    account_manager()->UpsertAccount(kPrimaryAccountKey, kFakePrimaryEmail,
                                     "access_token");
    base::RunLoop().RunUntilIdle();
    on_token_upserted_call_count_ = 0;
    on_token_upserted_account_ = std::nullopt;
  }

  void TearDownOnMainThread() override {
    account_manager_->RemoveObserver(this);
    on_token_upserted_call_count_ = 0;
    on_token_upserted_account_ = std::nullopt;
  }

  void CreateSigninHelper(const base::RepeatingClosure& exit_closure,
                          const base::RepeatingClosure& close_dialog_closure) {
    std::unique_ptr<SigninHelper::ArcHelper> arc_helper =
        std::make_unique<SigninHelper::ArcHelper>(
            /*is_available_in_arc=*/false, /*is_account_addition=*/false,
            /*account_apps_availability=*/nullptr);
    new TestSigninHelper(exit_closure, account_manager(),
                         account_manager_mojo_service(), close_dialog_closure,
                         /*show_signin_error=*/base::DoNothing(),
                         shared_url_loader_factory(), std::move(arc_helper),
                         kFakeGaiaId, kFakeEmail, kFakeAuthCode, kFakeDeviceId);
  }

  void CreateSigninHelperWithSiginErrorClosure(
      const base::RepeatingClosure& exit_closure,
      const base::RepeatingClosure& show_signin_error) {
    std::unique_ptr<SigninHelper::ArcHelper> arc_helper =
        std::make_unique<SigninHelper::ArcHelper>(
            /*is_available_in_arc=*/false, /*is_account_addition=*/false,
            /*account_apps_availability=*/nullptr);
    new TestSigninHelper(
        exit_closure, account_manager(), account_manager_mojo_service(),
        /*close_dialog_closure=*/base::DoNothing(),
        base::IgnoreArgs<const std::string&, const std::string&>(
            show_signin_error),
        shared_url_loader_factory(), std::move(arc_helper), kFakeGaiaId,
        kFakeEmail, kFakeAuthCode, kFakeDeviceId);
  }

  GaiaAuthConsumer::ClientOAuthResult GetFakeOAuthResult() {
    return GaiaAuthConsumer::ClientOAuthResult(
        kFakeRefreshToken, /*access_token=*/"",
        /*expires_in_secs=*/0, /*is_child_account=*/false,
        /*is_under_advanced_protection=*/false, /*is_bound_to_key=*/false);
  }

  int on_token_upserted_call_count() { return on_token_upserted_call_count_; }

  std::optional<account_manager::Account> on_token_upserted_account() {
    return on_token_upserted_account_;
  }

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory() {
    return test_shared_loader_factory_;
  }

  account_manager::AccountManager* account_manager() {
    return account_manager_;
  }

  crosapi::AccountManagerMojoService* account_manager_mojo_service() {
    return account_manager_mojo_service_;
  }

 protected:
  void AddResponseClientOAuthSuccess() {
    loader_factory().AddResponse(
        GaiaUrls::GetInstance()->oauth2_token_url().spec(),
        /*content=*/kOnClientOAuthSuccessBody, net::HTTP_OK);
  }

  void AddResponseClientOAuthFailure() {
    loader_factory().AddResponse(
        GaiaUrls::GetInstance()->oauth2_token_url().spec(),
        /*content=*/R"({})", net::HTTP_BAD_REQUEST);
  }

  void AddResponseGetSecondaryGoogleAccountUsage(
      const std::string& policy_value) {
    loader_factory().AddResponse(
        kSecureConnectApiGetSecondaryGoogleAccountUsageURL,
        /*content=*/
        base::StringPrintf(
            kSecureConnectApiGetSecondaryGoogleAccountUsageURLBody,
            policy_value.c_str()),
        net::HTTP_OK);
  }

  void AddResponseGetSecondaryAccountAllowedInArcPolicy(
      const std::string& policy_value) {
    loader_factory().AddResponse(
        kSecureConnectApiGetSecondaryAccountAllowedInArcPolicyURL,
        /*content=*/
        base::StringPrintf(
            kSecureConnectApiGetSecondaryGoogleAccountUsageURLBody,
            policy_value.c_str()),
        net::HTTP_OK);
  }

  void AddResponseGetUserInfoWithHostedDomain(
      const std::string& hosted_domain) {
    loader_factory().AddResponse(
        GaiaUrls::GetInstance()->oauth_user_info_url().spec(),
        /*content=*/
        base::StringPrintf(kUserInfoURLBodyWithHostedDomain,
                           hosted_domain.c_str()),
        net::HTTP_OK);
  }

  void AddResponseGetUserInfoWithoutHostedDomain() {
    loader_factory().AddResponse(
        GaiaUrls::GetInstance()->oauth_user_info_url().spec(),
        /*content=*/kUserInfoURLBodyWithoutHostedDomain, net::HTTP_OK);
  }

  void AddResponseRevokeGaiaTokenOnServer() {
    loader_factory().AddResponse(
        GaiaUrls::GetInstance()->oauth2_revoke_url().spec(),
        /*content=*/std::string(), net::HTTP_OK);
  }

  network::TestURLLoaderFactory& loader_factory() {
    return test_url_loader_factory_;
  }

 private:
  // account_manager::AccountManager::Observer overrides:
  void OnTokenUpserted(const account_manager::Account& account) override {
    ++on_token_upserted_call_count_;
    on_token_upserted_account_ = account;
  }

  void OnAccountRemoved(const account_manager::Account& account) override {}

  raw_ptr<account_manager::AccountManager, DanglingUntriaged> account_manager_ =
      nullptr;
  raw_ptr<crosapi::AccountManagerMojoService, DanglingUntriaged>
      account_manager_mojo_service_ = nullptr;
  int on_token_upserted_call_count_ = 0;
  std::optional<account_manager::Account> on_token_upserted_account_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
};

IN_PROC_BROWSER_TEST_F(SigninHelperTest,
                       NoAccountAddedWhenAuthTokenFetchFails) {
  base::test::RepeatingTestFuture exit_future, signin_error_future;
  // Set auth token fetch to fail.
  AddResponseClientOAuthFailure();
  CreateSigninHelperWithSiginErrorClosure(exit_future.GetCallback(),
                                          signin_error_future.GetCallback());
  // Make sure the show_signin_error was called.
  EXPECT_TRUE(signin_error_future.Wait());
  EXPECT_TRUE(exit_future.Wait());
  // No account should be added.
  EXPECT_EQ(on_token_upserted_call_count(), 0);
}

IN_PROC_BROWSER_TEST_F(SigninHelperTest,
                       AccountAddedWhenAuthTokenFetchSucceeds) {
  base::test::RepeatingTestFuture exit_future, close_dialog_future;
  CreateSigninHelper(exit_future.GetCallback(),
                     close_dialog_future.GetCallback());
  // Set auth token fetch to succeed.
  AddResponseClientOAuthSuccess();
  // Make sure the close_dialog_closure was called.
  EXPECT_TRUE(close_dialog_future.Wait());
  // Wait until SigninHelper finishes and deletes itself.
  EXPECT_TRUE(exit_future.Wait());
  // 1 account should be added.
  EXPECT_EQ(on_token_upserted_call_count(), 1);
  auto account = on_token_upserted_account();
  ASSERT_TRUE(account.has_value());
  EXPECT_EQ(account.value().raw_email, kFakeEmail);
}

class SigninHelperTestSecondaryGoogleAccountUsage : public SigninHelperTest {
 public:
  SigninHelperTestSecondaryGoogleAccountUsage() {
    feature_list_.InitWithFeatures(
        {features::kSecondaryAccountAllowedInArcPolicy},
        /*disabled_features=*/{});
  }

  ~SigninHelperTestSecondaryGoogleAccountUsage() override = default;

  TestSigninHelper* CreateSigninHelper(
      const base::RepeatingClosure& exit_closure,
      const base::RepeatingClosure& close_dialog_closure,
      const base::RepeatingClosure& show_signin_error,
      const std::string& gaia_id,
      const std::string& email) {
    std::unique_ptr<SigninHelper::ArcHelper> arc_helper =
        std::make_unique<SigninHelper::ArcHelper>(
            /*is_available_in_arc=*/false, /*is_account_addition=*/false,
            /*account_apps_availability=*/nullptr);
    // The `TestSigninHelper` deletes itself after its work is complete.
    return new TestSigninHelper(
        exit_closure, account_manager(), account_manager_mojo_service(),
        /*close_dialog_closure=*/close_dialog_closure,
        /*show_signin_error=*/
        base::IgnoreArgs<const std::string&, const std::string&>(
            show_signin_error),
        shared_url_loader_factory(), std::move(arc_helper), gaia_id, email,
        kFakeAuthCode, kFakeDeviceId);
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SigninHelperTestSecondaryGoogleAccountUsage,
                       AccountAddedForNonEnterpriseAccount) {
  // Set auth token fetch to succeed.
  AddResponseClientOAuthSuccess();
  // Set no hosted domain for user info request.
  AddResponseGetUserInfoWithoutHostedDomain();

  base::test::RepeatingTestFuture exit_future, close_dialog_future;
  // Non Enterprise account tries to sign in.
  CreateSigninHelper(exit_future.GetCallback(),
                     close_dialog_future.GetCallback(),
                     /*show_signin_error=*/
                     base::BindRepeating(&NotReached), kFakeGaiaId, kFakeEmail);

  // Make sure the close_dialog_closure was called.
  EXPECT_TRUE(close_dialog_future.Wait());
  // Wait until SigninHelper finishes and deletes itself.
  EXPECT_TRUE(exit_future.Wait());

  // 1 account should be added.
  EXPECT_EQ(on_token_upserted_call_count(), 1);

  auto account = on_token_upserted_account();
  ASSERT_TRUE(account.has_value());
  EXPECT_EQ(account.value().raw_email, kFakeEmail);
  histogram_tester_.ExpectBucketCount(
      kSecondaryGoogleAccountUsageHistogramName,
      ash::UserCloudSigninRestrictionPolicyFetcher::Status::
          kUnsupportedAccountTypeError,
      1);
}

IN_PROC_BROWSER_TEST_F(SigninHelperTestSecondaryGoogleAccountUsage,
                       AccountAddedForEnterpriseAccountWithNoPolicySet) {
  // Set auth token fetch to succeed.
  AddResponseClientOAuthSuccess();
  // Set user info response with hosted domain (hd) value.
  AddResponseGetUserInfoWithHostedDomain(kFakeEnterpriseDomain);
  // Set SecondaryGoogleAccountUsage policy fetch to unset.
  AddResponseGetSecondaryGoogleAccountUsage("unset");
  // Set SecondaryAccountAllowedInArcPolicy fetch to true.
  AddResponseGetSecondaryAccountAllowedInArcPolicy("true");

  base::test::RepeatingTestFuture exit_future, close_dialog_future;
  // Enterprise account tries to sign in.
  raw_ptr<TestSigninHelper> signin_helper = CreateSigninHelper(
      exit_future.GetCallback(), close_dialog_future.GetCallback(),
      /*show_signin_error=*/
      base::BindRepeating(&NotReached), kFakeEnterpriseGaiaId,
      kFakeEnterpriseEmail);
  // Make sure the close_dialog_closure was called.
  EXPECT_TRUE(close_dialog_future.Wait());

  EXPECT_TRUE(signin_helper->IsAvailableInArc());

  signin_helper = nullptr;
  // Wait until SigninHelper finishes and deletes itself.
  EXPECT_TRUE(exit_future.Wait());

  // 1 account should be added.
  EXPECT_EQ(on_token_upserted_call_count(), 1);

  auto account = on_token_upserted_account();
  ASSERT_TRUE(account.has_value());
  EXPECT_EQ(account.value().raw_email, kFakeEnterpriseEmail);
  histogram_tester_.ExpectBucketCount(
      kSecondaryGoogleAccountUsageHistogramName,
      ash::UserCloudSigninRestrictionPolicyFetcher::Status::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(
      kSecondaryGoogleAccountUsageLatencyHistogramName, 1);
}

IN_PROC_BROWSER_TEST_F(
    SigninHelperTestSecondaryGoogleAccountUsage,
    AccountAddedForEnterpriseAccountWithPolicyValueAllUsages) {
  // Set auth token fetch to succeed.
  AddResponseClientOAuthSuccess();
  // Set user info response with hosted domain (hd) value.
  AddResponseGetUserInfoWithHostedDomain(kFakeEnterpriseDomain);
  // Set SecondaryGoogleAccountUsage policy fetch to all.
  AddResponseGetSecondaryGoogleAccountUsage("all");
  // Set SecondaryAccountAllowedInArcPolicy fetch to true.
  AddResponseGetSecondaryAccountAllowedInArcPolicy("false");

  base::test::RepeatingTestFuture exit_future, close_dialog_future;
  // Enterprise account tries to sign in.
  raw_ptr<TestSigninHelper> signin_helper = CreateSigninHelper(
      exit_future.GetCallback(), close_dialog_future.GetCallback(),
      /*show_signin_error=*/
      base::BindRepeating(&NotReached), kFakeEnterpriseGaiaId,
      kFakeEnterpriseEmail);
  // Make sure the close_dialog_closure was called.
  EXPECT_TRUE(close_dialog_future.Wait());

  EXPECT_FALSE(signin_helper->IsAvailableInArc());

  signin_helper = nullptr;
  // Wait until SigninHelper finishes and deletes itself.
  EXPECT_TRUE(exit_future.Wait());

  // 1 account should be added.
  EXPECT_EQ(on_token_upserted_call_count(), 1);

  auto account = on_token_upserted_account();
  ASSERT_TRUE(account.has_value());
  EXPECT_EQ(account.value().raw_email, kFakeEnterpriseEmail);
  histogram_tester_.ExpectBucketCount(
      kSecondaryGoogleAccountUsageHistogramName,
      ash::UserCloudSigninRestrictionPolicyFetcher::Status::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(
      kSecondaryGoogleAccountUsageLatencyHistogramName, 1);
}

IN_PROC_BROWSER_TEST_F(
    SigninHelperTestSecondaryGoogleAccountUsage,
    NoAccountAddedForEnterpriseAccountWithPolicyValuePrimaryAccountSignin) {
  // Set auth token fetch to succeed.
  AddResponseClientOAuthSuccess();
  // Set user info response with hosted domain (hd) value.
  AddResponseGetUserInfoWithHostedDomain(kFakeEnterpriseDomain);
  // Set SecondaryGoogleAccountUsage policy fetch to primary_account_signin.
  AddResponseGetSecondaryGoogleAccountUsage("primary_account_signin");
  // Set response for token revocation.
  AddResponseRevokeGaiaTokenOnServer();

  base::test::RepeatingTestFuture exit_future, show_signin_error_future;
  // Enterprise account tries to sign in.
  CreateSigninHelper(exit_future.GetCallback(),
                     /*close_dialog_closure=*/base::BindRepeating(&NotReached),
                     /*show_signin_error=*/
                     show_signin_error_future.GetCallback(),
                     kFakeEnterpriseGaiaId, kFakeEnterpriseEmail);
  // Make sure the show_signin_blocked_error_closure_run_loop was called.
  EXPECT_TRUE(show_signin_error_future.Wait());
  // Wait until SigninHelper finishes and deletes itself.
  EXPECT_TRUE(exit_future.Wait());

  // 0 account should be added.
  EXPECT_EQ(on_token_upserted_call_count(), 0);
  histogram_tester_.ExpectBucketCount(
      kSecondaryGoogleAccountUsageHistogramName,
      ash::UserCloudSigninRestrictionPolicyFetcher::Status::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(
      kSecondaryGoogleAccountUsageLatencyHistogramName, 1);
}

IN_PROC_BROWSER_TEST_F(SigninHelperTestSecondaryGoogleAccountUsage,
                       ReauthForInitialPrimaryEnterpriseAccount) {
  // Set auth token fetch to succeed.
  AddResponseClientOAuthSuccess();

  base::test::RepeatingTestFuture exit_future, close_dialog_closure;
  CreateSigninHelper(exit_future.GetCallback(),
                     close_dialog_closure.GetCallback(),
                     /*show_signin_error=*/
                     base::BindRepeating(&NotReached),
                     user_manager::UserManager::Get()
                         ->GetPrimaryUser()
                         ->GetAccountId()
                         .GetGaiaId(),
                     kFakePrimaryEmail);

  // Make sure the close_dialog_closure was called.
  EXPECT_TRUE(close_dialog_closure.Wait());
  // Wait until SigninHelper finishes and deletes itself.
  EXPECT_TRUE(exit_future.Wait());

  // 1 account should be upserted.
  EXPECT_EQ(on_token_upserted_call_count(), 1);
  histogram_tester_.ExpectBucketCount(
      kSecondaryGoogleAccountUsageHistogramName,
      ash::UserCloudSigninRestrictionPolicyFetcher::Status::
          kUnsupportedAccountTypeError,
      0);
}

}  // namespace ash
