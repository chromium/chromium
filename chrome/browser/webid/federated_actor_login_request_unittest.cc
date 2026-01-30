// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_actor_login_request.h"

#include "base/functional/callback_helpers.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

class FederatedActorLoginRequestTest : public ChromeRenderViewHostTestHarness {
 public:
  FederatedActorLoginRequestTest() = default;
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
