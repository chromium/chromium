// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_tracker.h"

#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "build/build_config.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_gaia_cookie_manager_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/signin_switches.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Mock;
using ::testing::Return;
using ::testing::ReturnRef;

namespace {

#if defined(OS_CHROMEOS)
using FakeSigninManagerForTesting = FakeSigninManagerBase;
#else
using FakeSigninManagerForTesting = FakeSigninManager;
#endif  // OS_CHROMEOS

class MockObserver : public SigninTracker::Observer {
 public:
  MockObserver() {}
  ~MockObserver() {}

  MOCK_METHOD1(SigninFailed, void(const GoogleServiceAuthError&));
  MOCK_METHOD0(SigninSuccess, void(void));
  MOCK_METHOD1(AccountAddedToCookie, void(const GoogleServiceAuthError&));
};

}  // namespace

class SigninTrackerTest : public testing::Test {
 public:
  SigninTrackerTest()
      : signin_client_(&pref_service_),
        fake_oauth2_token_service_(&pref_service_),
        fake_gaia_cookie_manager_service_(&fake_oauth2_token_service_,
                                          "signin_tracker_unittest",
                                          &signin_client_),
#if defined(OS_CHROMEOS)
        fake_signin_manager_(&signin_client_, &account_tracker_) {
#else
        fake_signin_manager_(&signin_client_,
                             &fake_oauth2_token_service_,
                             &account_tracker_,
                             &fake_gaia_cookie_manager_service_) {
#endif

    AccountTrackerService::RegisterPrefs(pref_service_.registry());
    SigninManagerBase::RegisterProfilePrefs(pref_service_.registry());
    SigninManagerBase::RegisterPrefs(pref_service_.registry());

    account_tracker_.Initialize(&pref_service_, base::FilePath());

    tracker_ = std::make_unique<SigninTracker>(
        &fake_oauth2_token_service_, &fake_signin_manager_,
        &fake_gaia_cookie_manager_service_, &observer_);
  }

  ~SigninTrackerTest() override { tracker_.reset(); }

  base::MessageLoop message_loop_;
  std::unique_ptr<SigninTracker> tracker_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  AccountTrackerService account_tracker_;
  TestSigninClient signin_client_;
  FakeProfileOAuth2TokenService fake_oauth2_token_service_;
  FakeGaiaCookieManagerService fake_gaia_cookie_manager_service_;
  FakeSigninManagerForTesting fake_signin_manager_;
  MockObserver observer_;
};

#if !defined(OS_CHROMEOS)
TEST_F(SigninTrackerTest, SignInFails) {
  const GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);

  // Signin failure should result in a SigninFailed callback.
  EXPECT_CALL(observer_, SigninSuccess()).Times(0);
  EXPECT_CALL(observer_, SigninFailed(error));

  fake_signin_manager_.FailSignin(error);
}
#endif  // !defined(OS_CHROMEOS)

TEST_F(SigninTrackerTest, SignInSucceeds) {
  EXPECT_CALL(observer_, SigninSuccess());
  EXPECT_CALL(observer_, SigninFailed(_)).Times(0);

  std::string gaia_id = "gaia_id";
  std::string email = "user@gmail.com";
  std::string account_id = account_tracker_.SeedAccountInfo(gaia_id, email);
  fake_signin_manager_.SetAuthenticatedAccountInfo(gaia_id, email);
  fake_oauth2_token_service_.UpdateCredentials(account_id, "refresh_token");
}

#if !defined(OS_CHROMEOS)
TEST_F(SigninTrackerTest, SignInSucceedsWithExistingAccount) {
  EXPECT_CALL(observer_, SigninSuccess());
  EXPECT_CALL(observer_, SigninFailed(_)).Times(0);

  std::string gaia_id = "gaia_id";
  std::string email = "user@gmail.com";
  std::string account_id = account_tracker_.SeedAccountInfo(gaia_id, email);
  fake_oauth2_token_service_.UpdateCredentials(account_id, "refresh_token");
  fake_signin_manager_.SignIn(gaia_id, email, std::string());
}
#endif
