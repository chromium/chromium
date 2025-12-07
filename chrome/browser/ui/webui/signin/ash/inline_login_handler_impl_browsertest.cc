// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/ash/inline_login_handler_impl.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/ash/account_manager/account_apps_availability.h"
#include "chrome/browser/ash/account_manager/account_apps_availability_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/test/child_logged_in_browser_test_mixin.h"
#include "chrome/browser/ash/test/regular_logged_in_browser_test_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/webui/ash/edu_coexistence/edu_coexistence_login_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/mock_account_manager_facade.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Eq;
using testing::IsEmpty;
using testing::IsNull;
using testing::Ne;
using testing::Not;

namespace ash {

namespace {

constexpr char kEmailKey[] = "email";
constexpr char kPasswordKey[] = "password";
constexpr char kGaiaIdKey[] = "gaiaId";
constexpr char kIsAvailableInArcKey[] = "isAvailableInArc";
constexpr char kSecondaryAccount1Email[] = "secondary1@gmail.com";
constexpr char kSecondaryAccountOAuthCode[] = "fake_oauth_code";
constexpr char kSecondaryAccountRefreshToken[] = "fake_refresh_token";
constexpr char kCompleteLoginMessage[] = "completeLogin";
constexpr char kGetDeviceIdMessage[] = "getDeviceId";
constexpr char kHandleFunctionName[] = "handleFunctionName";
constexpr char kConsentLoggedCallback[] = "consent-logged-callback";
constexpr char kToSVersion[] = "12345678";
constexpr char kFakeDeviceId[] = "fake-device-id";
constexpr char kCrosAddAccountFlow[] = "crosAddAccount";
constexpr char kCrosAddAccountEduFlow[] = "crosAddAccountEdu";

struct DeviceAccountInfo {
  std::string id;
  std::string email;

  user_manager::UserType user_type;
  account_manager::AccountType account_type;
  std::string token;

  friend std::ostream& operator<<(std::ostream& stream,
                                  const DeviceAccountInfo& device_account_info);
};

std::ostream& operator<<(std::ostream& stream,
                         const DeviceAccountInfo& device_account_info) {
  return stream << "{email: " << device_account_info.email
                << ", user_type: " << device_account_info.user_type << "}";
}

DeviceAccountInfo GetGaiaDeviceAccountInfo() {
  return {signin::GetTestGaiaIdForEmail("primary@gmail.com").ToString() /*id*/,
          "primary@gmail.com" /*email*/,
          user_manager::UserType::kRegular /*user_type*/,
          account_manager::AccountType::kGaia /*account_type*/,
          "device-account-token" /*token*/};
}

DeviceAccountInfo GetChildDeviceAccountInfo() {
  return {supervised_user::kChildAccountSUID /*id*/,
          "child@gmail.com" /*email*/,
          user_manager::UserType::kChild /*user_type*/,
          account_manager::AccountType::kGaia /*account_type*/,
          "device-account-token" /*token*/};
}

base::Value GetCompleteLoginArgs(const std::string& email) {
  base::Value::Dict dict;
  dict.Set(kEmailKey, base::Value(email));
  dict.Set(kPasswordKey, base::Value("fake password"));
  dict.Set(kGaiaIdKey,
           base::Value(signin::GetTestGaiaIdForEmail(email).ToString()));
  dict.Set(kIsAvailableInArcKey, base::Value(true));
  return base::Value(std::move(dict));
}

MATCHER_P(AccountEmailEq, expected_email, "") {
  return testing::ExplainMatchResult(
      testing::Field(&account_manager::Account::raw_email,
                     testing::StrEq(expected_email)),
      arg, result_listener);
}

class TestInlineLoginHandler : public InlineLoginHandlerImpl {
 public:
  TestInlineLoginHandler(const base::RepeatingClosure& close_dialog_closure,
                         content::WebUI* web_ui)
      : InlineLoginHandlerImpl(close_dialog_closure) {
    set_web_ui(web_ui);
  }

  TestInlineLoginHandler(const TestInlineLoginHandler&) = delete;
  TestInlineLoginHandler& operator=(const TestInlineLoginHandler&) = delete;

  void SetExtraInitParams(base::Value::Dict& params) override {
    InlineLoginHandlerImpl::SetExtraInitParams(params);
  }
};

class MockAccountAppsAvailabilityObserver
    : public AccountAppsAvailability::Observer {
 public:
  MockAccountAppsAvailabilityObserver() = default;
  ~MockAccountAppsAvailabilityObserver() override = default;

  MOCK_METHOD(void,
              OnAccountAvailableInArc,
              (const account_manager::Account&),
              (override));
  MOCK_METHOD(void,
              OnAccountUnavailableInArc,
              (const account_manager::Account&),
              (override));
};

std::unique_ptr<InProcessBrowserTestMixin> CreateLoginMixin(
    const DeviceAccountInfo& info,
    InProcessBrowserTestMixinHost* host) {
  switch (info.user_type) {
    case user_manager::UserType::kRegular:
      return std::make_unique<ash::RegularLoggedInBrowserTestMixin>(
          host, AccountId::FromUserEmailGaiaId(info.email, GaiaId(info.id)));
    case user_manager::UserType::kChild:
      return std::make_unique<ash::ChildLoggedInBrowserTestMixin>(
          host, AccountId::FromUserEmailGaiaId(info.email, GaiaId(info.id)));
    case user_manager::UserType::kGuest:
    case user_manager::UserType::kPublicAccount:
    case user_manager::UserType::kKioskChromeApp:
    case user_manager::UserType::kKioskWebApp:
    case user_manager::UserType::kKioskIWA:
    case user_manager::UserType::kKioskArcvmApp:
      NOTREACHED();
  }
}

}  // namespace

class InlineLoginHandlerTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<DeviceAccountInfo> {
 public:
  InlineLoginHandlerTest()
      : login_mixin_(CreateLoginMixin(GetDeviceAccountInfo(), &mixin_host_)),
        embedded_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    embedded_test_server_.RegisterRequestHandler(base::BindRepeating(
        &FakeGaia::HandleRequest, base::Unretained(&fake_gaia_)));
  }
  ~InlineLoginHandlerTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server_.InitializeAndListen());
    MixinBasedInProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);

    // Configure embedded test server.
    const GURL& base_url = embedded_test_server_.base_url();
    command_line->AppendSwitchASCII(::switches::kGaiaUrl, base_url.spec());
    command_line->AppendSwitchASCII(::switches::kLsoUrl, base_url.spec());
    command_line->AppendSwitchASCII(::switches::kGoogleApisUrl,
                                    base_url.spec());
    fake_gaia_.Initialize();
  }

  void SetUpLocalStatePrefService(PrefService* local_state) override {
    MixinBasedInProcessBrowserTest::SetUpLocalStatePrefService(local_state);

    // Setup a default device id for the primary user.
    const auto& info = GetDeviceAccountInfo();
    user_manager::KnownUser known_user(local_state);
    known_user.SetDeviceId(
        AccountId::FromUserEmailGaiaId(info.email, GaiaId(info.id)),
        kFakeDeviceId);
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();

    embedded_test_server_.StartAcceptingConnections();

    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    AccountInfo account_info =
        identity_test_env_profile_adaptor_->identity_test_env()
            ->MakePrimaryAccountAvailable(GetDeviceAccountInfo().email,
                                          signin::ConsentLevel::kSignin);

    if (GetDeviceAccountInfo().user_type == user_manager::UserType::kChild) {
      profile()->GetPrefs()->SetString(prefs::kSupervisedUserId,
                                       supervised_user::kChildAccountSUID);
      // This is required for Child users, otherwise an account cannot be added.
      edu_handler_ =
          std::make_unique<EduCoexistenceLoginHandler>(base::DoNothing());
      edu_handler_->set_web_ui_for_test(web_ui());
      edu_handler_->RegisterMessages();
    }

    // Setup web ui with cookies.
    web_ui_.set_web_contents(web_contents());
    net::CookieOptions options;
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::MakeInclusive());
    auto url = GaiaUrls::GetInstance()->gaia_url();
    auto cookie_obj = net::CanonicalCookie::CreateForTesting(
        url, std::string("oauth_code=") + kSecondaryAccountOAuthCode,
        base::Time::Now());
    content::StoragePartition* partition =
        signin::GetSigninPartition(web_contents()->GetBrowserContext());
    base::test::TestFuture<net::CookieAccessResult> future;
    partition->GetCookieManagerForBrowserProcess()->SetCanonicalCookie(
        *cookie_obj, url, options, future.GetCallback());
    EXPECT_TRUE(future.Wait());

    // Setup fake Gaia.
    FakeGaia::Configuration params;
    params.emails = {kSecondaryAccount1Email};
    params.refresh_token = kSecondaryAccountRefreshToken;
    params.auth_code = kSecondaryAccountOAuthCode;
    fake_gaia_.UpdateConfiguration(params);

    // Setup handlers.
    handler_ =
        std::make_unique<TestInlineLoginHandler>(base::DoNothing(), &web_ui_);
    handler_->RegisterMessages();
    handler_->AllowJavascriptForTesting();
    base::RunLoop().RunUntilIdle();
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    MixinBasedInProcessBrowserTest::SetUpBrowserContextKeyedServices(context);
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
    if (GetDeviceAccountInfo().user_type == user_manager::UserType::kChild) {
      Profile::FromBrowserContext(context)
          ->GetProfilePolicyConnector()
          ->OverrideIsManagedForTesting(true);
    }
  }

  void TearDownOnMainThread() override {
    handler_.reset();
    identity_test_env_profile_adaptor_.reset();
    edu_handler_.reset();
    base::RunLoop().RunUntilIdle();
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

  void CompleteConsentLogForChildUser(const std::string& secondary_email) {
    base::Value::List call_args;
    call_args.Append(secondary_email);
    call_args.Append(kToSVersion);

    base::Value::List list_args;
    list_args.Append(kConsentLoggedCallback);
    list_args.Append(std::move(call_args));

    web_ui()->HandleReceivedMessage("consentLogged", list_args);
  }

  void SetExtraInitParamsInHandler(base::Value::Dict& dict) {
    handler_->SetExtraInitParams(dict);
  }

  std::string GetDeviceIdFromWebview() {
    // Call "getDeviceId".
    base::Value::List args;
    args.Append(kHandleFunctionName);
    web_ui()->HandleReceivedMessage(kGetDeviceIdMessage, args);
    base::RunLoop().RunUntilIdle();

    EXPECT_THAT(web_ui()->call_data(), Not(IsEmpty()));
    const content::TestWebUI::CallData& call_data =
        *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", call_data.function_name());
    EXPECT_EQ(kHandleFunctionName, call_data.arg1()->GetString());
    EXPECT_TRUE(call_data.arg2()->GetBool());

    // Get results from JS callback.
    return call_data.arg3()->GetString();
  }

  DeviceAccountInfo GetDeviceAccountInfo() const { return GetParam(); }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  Profile* profile() { return browser()->profile(); }

  content::TestWebUI* web_ui() { return &web_ui_; }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

  const AccountId& primary_account_id() {
    return user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId();
  }

 private:
  // Mixin structure to set up a user with the appropriate type.
  std::unique_ptr<InProcessBrowserTestMixin> login_mixin_;

  std::unique_ptr<TestInlineLoginHandler> handler_;
  std::unique_ptr<EduCoexistenceLoginHandler> edu_handler_;
  content::TestWebUI web_ui_;
  net::EmbeddedTestServer embedded_test_server_;
  FakeGaia fake_gaia_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_P(InlineLoginHandlerTest, NewAccountAdditionSuccess) {
  account_manager::MockAccountManagerFacadeObserver observer;
  base::ScopedObservation<account_manager::AccountManagerFacade,
                          account_manager::AccountManagerFacade::Observer>
      observation{&observer};
  observation.Observe(AccountManagerFactory::Get()->GetAccountManagerFacade(
      profile()->GetPath().value()));

  // Call "completeLogin".
  base::Value::List args;
  args.Append(GetCompleteLoginArgs(kSecondaryAccount1Email));
  web_ui()->HandleReceivedMessage(kCompleteLoginMessage, args);

  if (GetDeviceAccountInfo().user_type == user_manager::UserType::kChild) {
    // Consent logging is required for secondary accounts.
    CompleteConsentLogForChildUser(kSecondaryAccount1Email);
  }

  // Wait until account is added.
  base::test::TestFuture<void> future;
  EXPECT_CALL(observer,
              OnAccountUpserted(AccountEmailEq(kSecondaryAccount1Email)))
      .WillOnce(base::test::RunOnceClosure(future.GetCallback()));
  EXPECT_TRUE(future.Wait());
}

IN_PROC_BROWSER_TEST_P(InlineLoginHandlerTest, PrimaryReauthenticationSuccess) {
  account_manager::MockAccountManagerFacadeObserver observer;
  base::ScopedObservation<account_manager::AccountManagerFacade,
                          account_manager::AccountManagerFacade::Observer>
      observation{&observer};
  observation.Observe(AccountManagerFactory::Get()->GetAccountManagerFacade(
      profile()->GetPath().value()));

  // Call "completeLogin".
  base::Value::List args;
  args.Append(GetCompleteLoginArgs(GetDeviceAccountInfo().email));
  web_ui()->HandleReceivedMessage(kCompleteLoginMessage, args);

  // Wait until account is added.
  base::test::TestFuture<void> future;
  EXPECT_CALL(observer,
              OnAccountUpserted(AccountEmailEq(GetDeviceAccountInfo().email)))
      .WillOnce(base::test::RunOnceClosure(future.GetCallback()));
  EXPECT_TRUE(future.Wait());
}

IN_PROC_BROWSER_TEST_P(InlineLoginHandlerTest,
                       GetDeviceIdReturnsANonEmptyString) {
  const std::string device_id = GetDeviceIdFromWebview();
  EXPECT_THAT(device_id, Not(IsEmpty()));
}

IN_PROC_BROWSER_TEST_P(InlineLoginHandlerTest,
                       GetDeviceIdReturnsKnownUserDeviceIdForDeviceAccount) {
  user_manager::KnownUser known_user{g_browser_process->local_state()};
  known_user.SetDeviceId(primary_account_id(), kFakeDeviceId);
  base::Value::Dict params;
  params.Set("email", primary_account_id().GetUserEmail());
  SetExtraInitParamsInHandler(params);

  EXPECT_THAT(GetDeviceIdFromWebview(), Eq(kFakeDeviceId));
}

IN_PROC_BROWSER_TEST_P(
    InlineLoginHandlerTest,
    GetDeviceIdDoesNotReturnKnownUserDeviceIdForSecondaryAccount) {
  user_manager::KnownUser known_user{g_browser_process->local_state()};
  known_user.SetDeviceId(primary_account_id(), kFakeDeviceId);
  base::Value::Dict params;
  params.Set("email", kSecondaryAccount1Email);
  SetExtraInitParamsInHandler(params);

  EXPECT_THAT(GetDeviceIdFromWebview(), Ne(kFakeDeviceId));
}

IN_PROC_BROWSER_TEST_P(
    InlineLoginHandlerTest,
    GetDeviceIdDoesNotReturnKnownUserDeviceIdForAccountAdditions) {
  // Device Account cannot be added inline. So if an account addition is taking
  // place, it must be for a Secondary Account - in which case, we should not
  // generate the device id for the Device Account.
  user_manager::KnownUser known_user{g_browser_process->local_state()};
  known_user.SetDeviceId(primary_account_id(), kFakeDeviceId);

  EXPECT_THAT(GetDeviceIdFromWebview(), Ne(kFakeDeviceId));
}

IN_PROC_BROWSER_TEST_P(InlineLoginHandlerTest,
                       FlowNameForDeviceAccountReauthentication) {
  base::Value::Dict params;
  params.Set("email", primary_account_id().GetUserEmail());
  SetExtraInitParamsInHandler(params);

  std::string* flow_name = params.FindString("flow");
  ASSERT_THAT(flow_name, Not(IsNull()));
  EXPECT_THAT(*flow_name, Eq(kCrosAddAccountFlow));
}

IN_PROC_BROWSER_TEST_P(InlineLoginHandlerTest,
                       FlowNameForRegularSecondaryAccountAddition) {
  if (GetDeviceAccountInfo().user_type == user_manager::UserType::kChild) {
    return;
  }

  base::Value::Dict params;
  SetExtraInitParamsInHandler(params);

  std::string* flow_name = params.FindString("flow");
  ASSERT_THAT(flow_name, Not(IsNull()));
  EXPECT_THAT(*flow_name, Eq(kCrosAddAccountFlow));
}

IN_PROC_BROWSER_TEST_P(InlineLoginHandlerTest,
                       FlowNameForRegularSecondaryAccountReauthentication) {
  if (GetDeviceAccountInfo().user_type == user_manager::UserType::kChild) {
    return;
  }

  base::Value::Dict params;
  params.Set("email", kSecondaryAccount1Email);
  SetExtraInitParamsInHandler(params);

  std::string* flow_name = params.FindString("flow");
  ASSERT_THAT(flow_name, Not(IsNull()));
  EXPECT_THAT(*flow_name, Eq(kCrosAddAccountFlow));
}

IN_PROC_BROWSER_TEST_P(InlineLoginHandlerTest,
                       FlowNameForChildEduAccountAddition) {
  if (GetDeviceAccountInfo().user_type != user_manager::UserType::kChild) {
    return;
  }

  base::Value::Dict params;
  SetExtraInitParamsInHandler(params);

  std::string* flow_name = params.FindString("flow");
  ASSERT_THAT(flow_name, Not(IsNull()));
  EXPECT_THAT(*flow_name, Eq(kCrosAddAccountEduFlow));
}

IN_PROC_BROWSER_TEST_P(InlineLoginHandlerTest,
                       FlowNameForChildEduAccountReauthentication) {
  if (GetDeviceAccountInfo().user_type != user_manager::UserType::kChild) {
    return;
  }

  base::Value::Dict params;
  params.Set("email", kSecondaryAccount1Email);
  SetExtraInitParamsInHandler(params);

  std::string* flow_name = params.FindString("flow");
  ASSERT_THAT(flow_name, Not(IsNull()));
  EXPECT_THAT(*flow_name, Eq(kCrosAddAccountEduFlow));
}

INSTANTIATE_TEST_SUITE_P(InlineLoginHandlerTestSuite,
                         InlineLoginHandlerTest,
                         ::testing::Values(GetGaiaDeviceAccountInfo(),
                                           GetChildDeviceAccountInfo()));
}  // namespace ash
