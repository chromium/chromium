// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_actor_login_request.h"

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/webid/identity_credential_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::_;

class FederatedActorLoginRequestTest : public ChromeRenderViewHostTestHarness {
 public:
  FederatedActorLoginRequestTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~FederatedActorLoginRequestTest() override = default;
};

TEST_F(FederatedActorLoginRequestTest, PersistsAfterNavigation) {
  GURL idp_url("https://idp.example");
  url::Origin idp_origin = url::Origin::Create(idp_url);
  std::string account_id = "account_id123";

  FederatedActorLoginRequest::Set(web_contents(), idp_origin, account_id,
                                  base::DoNothing());

  // Verify that the request is set.
  FederatedActorLoginRequest* request =
      FederatedActorLoginRequest::Get(web_contents());
  ASSERT_TRUE(request);
  EXPECT_EQ(request->idp_origin(), idp_origin);
  EXPECT_EQ(request->account_id(), account_id);

  // Navigate the web contents.
  NavigateAndCommit(GURL("https://rp.example/new_page"));

  // Verify that the request persists.
  request = FederatedActorLoginRequest::Get(web_contents());
  ASSERT_TRUE(request);
  EXPECT_EQ(request->idp_origin(), idp_origin);
  EXPECT_EQ(request->account_id(), account_id);

  FederatedActorLoginRequest::Unset(web_contents());
}

TEST_F(FederatedActorLoginRequestTest, Timeout) {
  GURL idp_url("https://idp.example");
  url::Origin idp_origin = url::Origin::Create(idp_url);
  std::string account_id = "account_id123";
  base::MockCallback<OnFederatedResultReceivedCallback> mock_callback;

  FederatedActorLoginRequest::Set(web_contents(), idp_origin, account_id,
                                  mock_callback.Get());
  ASSERT_TRUE(FederatedActorLoginRequest::Get(web_contents()));

  EXPECT_CALL(mock_callback,
              Run(content::webid::FederatedLoginResult::kTimeout));
  task_environment()->FastForwardBy(base::Seconds(20));
  EXPECT_EQ(FederatedActorLoginRequest::Get(web_contents()), nullptr);
}

TEST_F(FederatedActorLoginRequestTest, TimeoutAfterSecondSet) {
  GURL idp_url("https://idp.example");
  url::Origin idp_origin = url::Origin::Create(idp_url);
  std::string account_id = "account_id123";
  base::MockCallback<OnFederatedResultReceivedCallback> mock_callback1;
  base::MockCallback<OnFederatedResultReceivedCallback> mock_callback2;

  FederatedActorLoginRequest::Set(web_contents(), idp_origin, account_id,
                                  mock_callback1.Get());
  ASSERT_TRUE(FederatedActorLoginRequest::Get(web_contents()));

  task_environment()->FastForwardBy(base::Seconds(10));

  FederatedActorLoginRequest::Set(web_contents(), idp_origin, account_id,
                                  mock_callback2.Get());

  EXPECT_CALL(mock_callback1, Run(_)).Times(0);
  task_environment()->FastForwardBy(base::Seconds(15));

  EXPECT_CALL(mock_callback2,
              Run(content::webid::FederatedLoginResult::kTimeout));
  task_environment()->FastForwardBy(base::Seconds(5));
  EXPECT_EQ(FederatedActorLoginRequest::Get(web_contents()), nullptr);
}

TEST_F(FederatedActorLoginRequestTest, UnsetNoTimeout) {
  GURL idp_url("https://idp.example");
  url::Origin idp_origin = url::Origin::Create(idp_url);
  std::string account_id = "account_id123";
  base::MockCallback<OnFederatedResultReceivedCallback> mock_callback;

  FederatedActorLoginRequest::Set(web_contents(), idp_origin, account_id,
                                  mock_callback.Get());
  ASSERT_TRUE(FederatedActorLoginRequest::Get(web_contents()));

  FederatedActorLoginRequest::Unset(web_contents());
  EXPECT_EQ(FederatedActorLoginRequest::Get(web_contents()), nullptr);

  EXPECT_CALL(mock_callback, Run(_)).Times(0);
  task_environment()->FastForwardBy(base::Seconds(20));
}

TEST_F(FederatedActorLoginRequestTest, NoTimeoutIfCallbackRun) {
  GURL idp_url("https://idp.example");
  url::Origin idp_origin = url::Origin::Create(idp_url);
  std::string account_id = "account_id123";
  base::MockCallback<OnFederatedResultReceivedCallback> mock_callback;

  FederatedActorLoginRequest::Set(web_contents(), idp_origin, account_id,
                                  mock_callback.Get());
  FederatedActorLoginRequest* request =
      FederatedActorLoginRequest::Get(web_contents());
  ASSERT_TRUE(request);

  EXPECT_CALL(mock_callback,
              Run(content::webid::FederatedLoginResult::kContinuation));
  request->OnFederatedResultReceived(
      content::webid::FederatedLoginResult::kContinuation);
  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  EXPECT_CALL(mock_callback, Run(_)).Times(0);
  task_environment()->FastForwardBy(base::Seconds(30));
  EXPECT_NE(FederatedActorLoginRequest::Get(web_contents()), nullptr);

  EXPECT_CALL(mock_callback,
              Run(content::webid::FederatedLoginResult::kSuccess));
  request->OnFederatedResultReceived(
      content::webid::FederatedLoginResult::kSuccess);
}
