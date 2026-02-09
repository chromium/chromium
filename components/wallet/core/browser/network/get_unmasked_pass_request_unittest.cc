// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/network/get_unmasked_pass_request.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/version_info/version_info.h"
#include "components/wallet/core/browser/proto/api_v1.pb.h"
#include "components/wallet/core/browser/proto/private_pass.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace wallet {

namespace {

class GetUnmaskedPassRequestTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

using GetUnmaskedPassCallback = base::test::TestFuture<
    const base::expected<PrivatePass, WalletHttpClient::WalletRequestError>&>;

// Tests that GetRequestUrlPath returns the correct URL path.
TEST_F(GetUnmaskedPassRequestTest, GetRequestUrlPath) {
  GetUnmaskedPassCallback callback;
  GetUnmaskedPassRequest request("pass-id", callback.GetCallback());

  EXPECT_EQ(request.GetRequestUrlPath(), "v1/e/privatePasses:batchGet");
}

// Tests that GetRequestContent generates the correct proto request body.
TEST_F(GetUnmaskedPassRequestTest, GetRequestContent) {
  GetUnmaskedPassCallback callback;
  GetUnmaskedPassRequest request("pass-id", callback.GetCallback());

  std::string request_body = request.GetRequestContent();
  api::GetPrivatePassesRequest request_proto;
  ASSERT_TRUE(request_proto.ParseFromString(request_body));

  ASSERT_EQ(request_proto.pass_ids_size(), 1);
  EXPECT_EQ(request_proto.pass_ids(0), "pass-id");
  EXPECT_EQ(request_proto.client_info().chrome_client_info().version(),
            version_info::GetVersionNumber());
}

// Tests that OnResponse handles a successful HTTP response.
TEST_F(GetUnmaskedPassRequestTest, OnResponse_Success) {
  GetUnmaskedPassCallback callback;
  GetUnmaskedPassRequest request("pass-id", callback.GetCallback());

  api::GetPrivatePassesResponse response_proto;
  auto* result = response_proto.add_results();
  result->set_pass_id("pass-id");
  result->mutable_private_pass()->set_pass_id("pass-id");
  result->mutable_private_pass()->mutable_passport()->set_passport_number(
      "secret-number");

  std::move(request).OnResponse(response_proto.SerializeAsString());

  ASSERT_TRUE(callback.Wait());
  EXPECT_TRUE(callback.Get().has_value());
  EXPECT_EQ(callback.Get()->pass_id(), "pass-id");
  EXPECT_EQ(callback.Get()->passport().passport_number(), "secret-number");
}

// Tests that OnResponse handles an error in the GetPrivatePassResult.
TEST_F(GetUnmaskedPassRequestTest, OnResponse_ResultError) {
  GetUnmaskedPassCallback callback;
  GetUnmaskedPassRequest request("pass-id", callback.GetCallback());

  api::GetPrivatePassesResponse response_proto;
  auto* result = response_proto.add_results();
  result->set_pass_id("pass-id");
  result->mutable_error()->set_code(3);  // INVALID_ARGUMENT
  result->mutable_error()->set_message("Invalid ID");

  std::move(request).OnResponse(response_proto.SerializeAsString());

  ASSERT_TRUE(callback.Wait());
  ASSERT_FALSE(callback.Get().has_value());
  EXPECT_EQ(callback.Get().error(),
            WalletHttpClient::WalletRequestError::kGenericError);
}

// Tests that OnResponse handles an empty results list.
TEST_F(GetUnmaskedPassRequestTest, OnResponse_EmptyResults) {
  GetUnmaskedPassCallback callback;
  GetUnmaskedPassRequest request("pass-id", callback.GetCallback());

  api::GetPrivatePassesResponse response_proto;

  std::move(request).OnResponse(response_proto.SerializeAsString());

  ASSERT_TRUE(callback.Wait());
  ASSERT_FALSE(callback.Get().has_value());
  EXPECT_EQ(callback.Get().error(),
            WalletHttpClient::WalletRequestError::kGenericError);
}

// Tests that OnResponse handles an error HTTP response.
TEST_F(GetUnmaskedPassRequestTest, OnResponse_HttpError) {
  GetUnmaskedPassCallback callback;
  GetUnmaskedPassRequest request("pass-id", callback.GetCallback());

  std::move(request).OnResponse(base::unexpected(
      WalletHttpClient::WalletRequestError::kAccessTokenFetchFailed));

  ASSERT_TRUE(callback.Wait());
  ASSERT_FALSE(callback.Get().has_value());
  EXPECT_EQ(callback.Get().error(),
            WalletHttpClient::WalletRequestError::kAccessTokenFetchFailed);
}

// Tests that OnResponse handles a parse error.
TEST_F(GetUnmaskedPassRequestTest, OnResponse_ParseError) {
  GetUnmaskedPassCallback callback;
  GetUnmaskedPassRequest request("pass-id", callback.GetCallback());

  std::move(request).OnResponse("invalid-proto");

  ASSERT_TRUE(callback.Wait());
  ASSERT_FALSE(callback.Get().has_value());
  EXPECT_EQ(callback.Get().error(),
            WalletHttpClient::WalletRequestError::kParseResponseFailed);
}

}  // namespace
}  // namespace wallet
