// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/webid/federated_embedder_login_request.h"

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/webid/identity_credential_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using content::webid::FederatedEmbedderLoginRequest;
using ::testing::_;

class FederatedEmbedderLoginRequestTest
    : public ChromeRenderViewHostTestHarness {
 public:
  FederatedEmbedderLoginRequestTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~FederatedEmbedderLoginRequestTest() override = default;
};

TEST_F(FederatedEmbedderLoginRequestTest, PersistsAfterNavigation) {
  GURL idp_url("https://idp.example");
  url::Origin idp_origin = url::Origin::Create(idp_url);
  std::string account_id = "account_id123";

  FederatedEmbedderLoginRequest::Set(web_contents(), idp_origin, account_id,
                                     base::DoNothing());

  // Verify that the request is set.
  FederatedEmbedderLoginRequest* request =
      FederatedEmbedderLoginRequest::Get(web_contents());
  ASSERT_TRUE(request);
  EXPECT_EQ(request->idp_origin(), idp_origin);
  EXPECT_EQ(request->account_id(), account_id);

  // Navigate the web contents.
  NavigateAndCommit(GURL("https://rp.example/new_page"));

  // Verify that the request persists.
  request = FederatedEmbedderLoginRequest::Get(web_contents());
  ASSERT_TRUE(request);
  EXPECT_EQ(request->idp_origin(), idp_origin);
  EXPECT_EQ(request->account_id(), account_id);
}

TEST_F(FederatedEmbedderLoginRequestTest, Timeout) {
  GURL idp_url("https://idp.example");
  url::Origin idp_origin = url::Origin::Create(idp_url);
  std::string account_id = "account_id123";
  base::MockCallback<
      base::OnceCallback<void(content::webid::FederatedLoginResult)>>
      mock_callback;

  FederatedEmbedderLoginRequest::Set(web_contents(), idp_origin, account_id,
                                     mock_callback.Get());
  ASSERT_TRUE(FederatedEmbedderLoginRequest::Get(web_contents()));

  EXPECT_CALL(mock_callback,
              Run(content::webid::FederatedLoginResult::kTimeout));
  task_environment()->FastForwardBy(base::Seconds(20));
  EXPECT_EQ(FederatedEmbedderLoginRequest::Get(web_contents()), nullptr);
}

TEST_F(FederatedEmbedderLoginRequestTest, TimeoutAfterSecondSet) {
  GURL idp_url("https://idp.example");
  url::Origin idp_origin = url::Origin::Create(idp_url);
  std::string account_id = "account_id123";
  base::MockCallback<
      base::OnceCallback<void(content::webid::FederatedLoginResult)>>
      mock_callback1;
  base::MockCallback<
      base::OnceCallback<void(content::webid::FederatedLoginResult)>>
      mock_callback2;

  FederatedEmbedderLoginRequest::Set(web_contents(), idp_origin, account_id,
                                     mock_callback1.Get());
  ASSERT_TRUE(FederatedEmbedderLoginRequest::Get(web_contents()));

  task_environment()->FastForwardBy(base::Seconds(10));

  FederatedEmbedderLoginRequest::Set(web_contents(), idp_origin, account_id,
                                     mock_callback2.Get());

  EXPECT_CALL(mock_callback1, Run).Times(0);
  task_environment()->FastForwardBy(base::Seconds(15));

  EXPECT_CALL(mock_callback2,
              Run(content::webid::FederatedLoginResult::kTimeout));
  task_environment()->FastForwardBy(base::Seconds(5));
  EXPECT_EQ(FederatedEmbedderLoginRequest::Get(web_contents()), nullptr);
}

TEST_F(FederatedEmbedderLoginRequestTest, ContinuationIsTerminal) {
  GURL idp_url("https://idp.example");
  url::Origin idp_origin = url::Origin::Create(idp_url);
  std::string account_id = "account_id123";
  base::MockCallback<
      base::OnceCallback<void(content::webid::FederatedLoginResult)>>
      mock_callback;

  FederatedEmbedderLoginRequest::Set(web_contents(), idp_origin, account_id,
                                     mock_callback.Get());
  FederatedEmbedderLoginRequest* request =
      FederatedEmbedderLoginRequest::Get(web_contents());
  ASSERT_TRUE(request);

  EXPECT_CALL(mock_callback,
              Run(content::webid::FederatedLoginResult::kContinuation));
  request->OnFederatedResultReceived(
      content::webid::FederatedLoginResult::kContinuation);
  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  // Request should be unset after kContinuation.
  EXPECT_EQ(FederatedEmbedderLoginRequest::Get(web_contents()), nullptr);
}

TEST_F(FederatedEmbedderLoginRequestTest, OpenerCheck) {
  GURL idp_url("https://idp.example");
  url::Origin idp_origin = url::Origin::Create(idp_url);
  std::string account_id = "account_id123";

  FederatedEmbedderLoginRequest::Set(web_contents(), idp_origin, account_id,
                                     base::DoNothing());

  // Create a popup without an opener.
  std::unique_ptr<content::WebContents> no_opener_popup =
      content::WebContents::Create(content::WebContents::CreateParams(
          web_contents()->GetBrowserContext()));
  EXPECT_FALSE(FederatedEmbedderLoginRequest::Get(no_opener_popup.get()));

  // Create a popup with an opener.
  content::WebContents::CreateParams params(
      web_contents()->GetBrowserContext());
  content::GlobalRenderFrameHostId opener_id =
      web_contents()->GetPrimaryMainFrame()->GetGlobalId();
  params.opener_render_process_id = opener_id.child_id.GetUnsafeValue();
  params.opener_render_frame_id = opener_id.frame_routing_id;
  std::unique_ptr<content::WebContents> popup =
      content::WebContents::Create(params);

  // Verify that the request IS found if opener is set.
  FederatedEmbedderLoginRequest* request =
      FederatedEmbedderLoginRequest::Get(popup.get());
  ASSERT_TRUE(request);
  EXPECT_EQ(request->idp_origin(), idp_origin);
  EXPECT_EQ(request->account_id(), account_id);
}
