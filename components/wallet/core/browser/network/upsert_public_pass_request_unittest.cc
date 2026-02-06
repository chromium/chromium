// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/network/upsert_public_pass_request.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/version_info/version_info.h"
#include "components/wallet/core/browser/proto/api_v1.pb.h"
#include "components/wallet/core/browser/proto/pass.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace wallet {

namespace {

class UpsertPublicPassRequestTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

using UpsertPublicPassCallback = base::test::TestFuture<
    const base::expected<std::string, WalletHttpClient::WalletRequestError>&>;

// Tests that GetRequestUrlPath returns the correct URL path.
TEST_F(UpsertPublicPassRequestTest, GetRequestUrlPath) {
  UpsertPublicPassCallback callback;
  UpsertPublicPassRequest request(Pass(), callback.GetCallback());

  EXPECT_EQ(request.GetRequestUrlPath(), "v1/passes:upsert");
}

// Tests that GetRequestContent generates the correct proto request body
// for a LoyaltyCard pass.
TEST_F(UpsertPublicPassRequestTest, GetRequestContent_LoyaltyCard) {
  Pass pass;
  Pass_LoyaltyCard* loyalty_card = pass.mutable_loyalty_card();
  loyalty_card->set_program_name("p1");
  loyalty_card->set_merchant_name("i1");
  loyalty_card->set_loyalty_number("m1");

  UpsertPublicPassCallback callback;
  UpsertPublicPassRequest request(pass, callback.GetCallback());

  std::string request_body = request.GetRequestContent();
  api::UpsertPassRequest request_proto;
  ASSERT_TRUE(request_proto.ParseFromString(request_body));

  // Verify pass
  ASSERT_TRUE(request_proto.has_pass());
  const Pass& pass_proto = request_proto.pass();

  // Verify loyalty_card
  ASSERT_TRUE(pass_proto.has_loyalty_card());
  EXPECT_EQ(pass_proto.loyalty_card().merchant_name(), "i1");
  EXPECT_EQ(pass_proto.loyalty_card().loyalty_number(), "m1");
  EXPECT_EQ(pass_proto.loyalty_card().program_name(), "p1");

  // Verify client_info
  ASSERT_TRUE(request_proto.has_client_info());
  EXPECT_EQ(request_proto.client_info().chrome_client_info().version(),
            version_info::GetVersionNumber());
}

// Tests that GetRequestContent correctly includes the pass_id when
// available.
TEST_F(UpsertPublicPassRequestTest, GetRequestContent_WithPassId) {
  Pass pass;
  pass.set_pass_id("pass-id");

  UpsertPublicPassCallback callback;
  UpsertPublicPassRequest request(pass, callback.GetCallback());

  std::string request_body = request.GetRequestContent();
  api::UpsertPassRequest request_proto;
  ASSERT_TRUE(request_proto.ParseFromString(request_body));

  ASSERT_TRUE(request_proto.has_pass());
  EXPECT_EQ(request_proto.pass().pass_id(), "pass-id");
}

// Tests that OnResponse handles a successful HTTP response.
TEST_F(UpsertPublicPassRequestTest, OnResponse_Success) {
  UpsertPublicPassCallback callback;
  UpsertPublicPassRequest request(Pass(), callback.GetCallback());

  api::UpsertPassResponse response;
  response.set_pass_id("pass-id");
  std::move(request).OnResponse(response.SerializeAsString());

  ASSERT_TRUE(callback.Wait());
  EXPECT_TRUE(callback.Get().has_value());
  EXPECT_EQ(callback.Get().value(), "pass-id");
}

// Tests that OnResponse handles an error HTTP response.
TEST_F(UpsertPublicPassRequestTest, OnResponse_Error) {
  UpsertPublicPassCallback callback;
  UpsertPublicPassRequest request(Pass(), callback.GetCallback());

  std::move(request).OnResponse(base::unexpected(
      WalletHttpClient::WalletRequestError::kAccessTokenFetchFailed));

  ASSERT_TRUE(callback.Wait());
  ASSERT_FALSE(callback.Get().has_value());
  EXPECT_EQ(callback.Get().error(),
            WalletHttpClient::WalletRequestError::kAccessTokenFetchFailed);
}

}  // namespace
}  // namespace wallet
