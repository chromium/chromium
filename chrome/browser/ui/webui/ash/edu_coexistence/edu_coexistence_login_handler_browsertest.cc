// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/edu_coexistence/edu_coexistence_login_handler.h"

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/child_accounts/edu_coexistence_tos_store_utils.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/edu_coexistence/edu_coexistence_state_tracker.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_web_ui.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace ash {

namespace {

constexpr char kResponseCallback[] = "cr.webUIResponse";

}  // namespace

class EduCoexistenceLoginHandlerBrowserTest
    : public MixinBasedInProcessBrowserTest {
 public:
  EduCoexistenceLoginHandlerBrowserTest() = default;
  EduCoexistenceLoginHandlerBrowserTest(
      const EduCoexistenceLoginHandlerBrowserTest&) = delete;
  EduCoexistenceLoginHandlerBrowserTest& operator=(
      const EduCoexistenceLoginHandlerBrowserTest&) = delete;
  ~EduCoexistenceLoginHandlerBrowserTest() override = default;

  void SetUp() override { MixinBasedInProcessBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    logged_in_user_mixin_.LogInUser();
  }

  void TearDown() override { MixinBasedInProcessBrowserTest::TearDown(); }

  std::unique_ptr<EduCoexistenceLoginHandler> SetUpHandler() {
    auto handler =
        std::make_unique<EduCoexistenceLoginHandler>(base::DoNothing());
    handler->set_web_ui_for_test(web_ui());
    handler->RegisterMessages();
    return handler;
  }

  void VerifyJavascriptCallResolved(const content::TestWebUI::CallData& data,
                                    const std::string& event_name,
                                    const std::string& call_type) {
    EXPECT_EQ(call_type, data.function_name());

    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ(event_name, data.arg1()->GetString());
  }

  void SimulateAccessTokenFetched(EduCoexistenceLoginHandler* handler,
                                  bool success = true) {
    GoogleServiceAuthError::State state =
        success ? GoogleServiceAuthError::NONE
                : GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS;

    handler->OnOAuthAccessTokensFetched(
        GoogleServiceAuthError(state),
        signin::AccessTokenInfo("access_token",
                                base::Time::Now() + base::Minutes(1), ""));
  }

  void ExpectEduCoexistenceState(
      EduCoexistenceStateTracker::FlowResult result) {
    const EduCoexistenceStateTracker::FlowState* state =
        EduCoexistenceStateTracker::Get()->GetInfoForWebUIForTest(web_ui());
    EXPECT_EQ(state->flow_result, result);
  }

  void ExpectEduCoexistenceStateHistogram(
      EduCoexistenceStateTracker::FlowResult result) {
    histograms_.ExpectUniqueSample(
        EduCoexistenceStateTracker::Get()->GetInSessionHistogramNameForTest(),
        result,
        /* expected count */ 1);
  }

 protected:
  content::TestWebUI* web_ui() { return &web_ui_; }

 private:
  LoggedInUserMixin logged_in_user_mixin_{&mixin_host_, /*test_base=*/this,
                                          embedded_test_server(),
                                          LoggedInUserMixin::LogInType::kChild};

  base::HistogramTester histograms_;

  content::TestWebUI web_ui_;
};

IN_PROC_BROWSER_TEST_F(EduCoexistenceLoginHandlerBrowserTest,
                       HandleInitializeEduCoexistenceArgs) {
  constexpr char kCallbackId[] = "coexistence-data-init";
  std::unique_ptr<EduCoexistenceLoginHandler> handler = SetUpHandler();

  ExpectEduCoexistenceState(EduCoexistenceStateTracker::FlowResult::kLaunched);

  base::Value::List list_args;
  list_args.Append(kCallbackId);
  web_ui()->HandleReceivedMessage("initializeEduArgs", list_args);
  SimulateAccessTokenFetched(handler.get());

  EXPECT_EQ(web_ui()->call_data().size(), 1u);

  const content::TestWebUI::CallData& second_call = *web_ui()->call_data()[0];

  // TODO(yilkal): verify the exact the call arguments.
  VerifyJavascriptCallResolved(second_call, kCallbackId, kResponseCallback);

  handler.reset();

  // The recorded state should be "launched" state.
  ExpectEduCoexistenceStateHistogram(
      EduCoexistenceStateTracker::FlowResult::kLaunched);
}

IN_PROC_BROWSER_TEST_F(EduCoexistenceLoginHandlerBrowserTest,
                       ErrorCallsFromWebUI) {
  std::unique_ptr<EduCoexistenceLoginHandler> handler = SetUpHandler();

  base::Value::List call_args;
  call_args.Append("error message 1");
  call_args.Append("error message 2");
  web_ui()->HandleReceivedMessage("error", call_args);

  EXPECT_TRUE(handler->in_error_state());

  handler.reset();
  ExpectEduCoexistenceStateHistogram(
      EduCoexistenceStateTracker::FlowResult::kError);
}

IN_PROC_BROWSER_TEST_F(EduCoexistenceLoginHandlerBrowserTest,
                       OAuth2AccessTokensFetchFailed) {
  std::unique_ptr<EduCoexistenceLoginHandler> handler = SetUpHandler();

  SimulateAccessTokenFetched(handler.get(), /* success */ false);

  // Error messages are not sent until initialize message is sent from js to
  // C++ handler.
  EXPECT_EQ(web_ui()->call_data().size(), 0u);

  base::Value::List call_args;
  call_args.Append("coexistence-data-init");
  web_ui()->HandleReceivedMessage("initializeEduArgs", call_args);

  EXPECT_EQ(web_ui()->call_data().size(), 1u);
  EXPECT_EQ(web_ui()->call_data()[0]->function_name(),
            "cr.webUIListenerCallback");
  const base::Value* arg1 = web_ui()->call_data()[0]->arg1();
  std::string method_call = arg1 ? arg1->GetString() : std::string();

  constexpr char kWebUICallErrorCallback[] = "show-error-screen";
  EXPECT_EQ(method_call, kWebUICallErrorCallback);

  handler.reset();

  ExpectEduCoexistenceStateHistogram(
      EduCoexistenceStateTracker::FlowResult::kError);
}

IN_PROC_BROWSER_TEST_F(EduCoexistenceLoginHandlerBrowserTest,
                       HandleConsentLogged) {
  constexpr char kConsentLoggedCallback[] = "consent-logged-callback";
  constexpr char kToSVersion[] = "12345678";
  std::unique_ptr<EduCoexistenceLoginHandler> handler = SetUpHandler();

  SimulateAccessTokenFetched(handler.get());

  base::Value::List call_args;
  call_args.Append(FakeGaiaMixin::kFakeUserEmail);
  call_args.Append(kToSVersion);

  base::Value::List list_args;
  list_args.Append(kConsentLoggedCallback);
  list_args.Append(std::move(call_args));

  web_ui()->HandleReceivedMessage("consentLogged", list_args);

  const EduCoexistenceStateTracker::FlowState* tracker =
      EduCoexistenceStateTracker::Get()->GetInfoForWebUIForTest(web_ui());

  // Expect that the tracker gets the appropriate update.
  EXPECT_NE(tracker, nullptr);
  EXPECT_TRUE(tracker->received_consent);
  EXPECT_EQ(tracker->email, FakeGaiaMixin::kFakeUserEmail);
  EXPECT_EQ(tracker->flow_result,
            EduCoexistenceStateTracker::FlowResult::kConsentLogged);

  // Simulate account added.
  CoreAccountInfo account;
  account.email = FakeGaiaMixin::kFakeUserEmail;
  account.gaia = FakeGaiaMixin::kFakeUserGaiaId;
  handler->OnRefreshTokenUpdatedForAccount(account);

  const std::string& accepted_tos = edu_coexistence::GetAcceptedToSVersion(
      ProfileManager::GetActiveUserProfile(), FakeGaiaMixin::kFakeUserGaiaId);
  EXPECT_EQ(accepted_tos, std::string(kToSVersion));

  EXPECT_EQ(web_ui()->call_data().size(), 1u);
  const content::TestWebUI::CallData& second_call = *web_ui()->call_data()[0];

  // TODO(yilkal): verify the exact the call arguments.
  VerifyJavascriptCallResolved(second_call, kConsentLoggedCallback,
                               kResponseCallback);

  handler.reset();

  ExpectEduCoexistenceStateHistogram(
      EduCoexistenceStateTracker::FlowResult::kAccountAdded);
}

IN_PROC_BROWSER_TEST_F(EduCoexistenceLoginHandlerBrowserTest,
                       TestUpdateAcceptedToSVersionPrefAccount) {
  constexpr char kVersion1[] = "123";
  constexpr char kVersion2[] = "234";
  constexpr char kUser1GaiaId[] = "user1-gaia-id";
  constexpr char kUser2GaiaId[] = "user2-gaia-id";
  constexpr char kUser3GaiaId[] = "user3-gaia-id";

  Profile* profile = ProfileManager::GetActiveUserProfile();

  edu_coexistence::UpdateAcceptedToSVersionPref(
      profile, edu_coexistence::UserConsentInfo(kUser1GaiaId, kVersion1));
  EXPECT_EQ(edu_coexistence::GetAcceptedToSVersion(profile, kUser1GaiaId),
            std::string(kVersion1));

  edu_coexistence::UpdateAcceptedToSVersionPref(
      profile, edu_coexistence::UserConsentInfo(kUser2GaiaId, kVersion1));
  EXPECT_EQ(edu_coexistence::GetAcceptedToSVersion(profile, kUser2GaiaId),
            std::string(kVersion1));

  edu_coexistence::UpdateAcceptedToSVersionPref(
      profile, edu_coexistence::UserConsentInfo(kUser3GaiaId, kVersion1));
  EXPECT_EQ(edu_coexistence::GetAcceptedToSVersion(profile, kUser3GaiaId),
            std::string(kVersion1));

  edu_coexistence::UpdateAcceptedToSVersionPref(
      profile, edu_coexistence::UserConsentInfo(kUser2GaiaId, kVersion2));
  EXPECT_EQ(edu_coexistence::GetAcceptedToSVersion(profile, kUser2GaiaId),
            std::string(kVersion2));
  EXPECT_EQ(edu_coexistence::GetAcceptedToSVersion(profile, kUser1GaiaId),
            std::string(kVersion1));
  EXPECT_EQ(edu_coexistence::GetAcceptedToSVersion(profile, kUser3GaiaId),
            std::string(kVersion1));

  edu_coexistence::UpdateAcceptedToSVersionPref(
      profile, edu_coexistence::UserConsentInfo(kUser1GaiaId, kVersion2));
  EXPECT_EQ(edu_coexistence::GetAcceptedToSVersion(profile, kUser1GaiaId),
            std::string(kVersion2));
}

}  // namespace ash
