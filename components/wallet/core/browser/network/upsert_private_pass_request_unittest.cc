// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/network/upsert_private_pass_request.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/version_info/version_info.h"
#include "components/wallet/core/browser/proto/api_v1.pb.h"
#include "components/wallet/core/browser/proto/private_pass.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace wallet {

namespace {

class UpsertPrivatePassRequestTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

using UpsertPrivatePassCallback = base::test::TestFuture<
    const base::expected<PrivatePass, WalletHttpClient::WalletRequestError>&>;

// Tests that GetRequestUrlPath returns the correct URL path.
TEST_F(UpsertPrivatePassRequestTest, GetRequestUrlPath) {
  UpsertPrivatePassCallback callback;
  PrivatePass pass;
  pass.mutable_passport();
  UpsertPrivatePassRequest request(
      pass, consent_auditor::ConsentAuditor::GenerateSessionId(),
      callback.GetCallback());

  EXPECT_EQ(request.GetRequestUrlPath(), "v1/e/privatePasses:upsert");
}

// Tests that GetRequestHeaders returns the correct headers.
TEST_F(UpsertPrivatePassRequestTest, GetRequestHeaders) {
  UpsertPrivatePassCallback callback;
  {
    PrivatePass pass;
    pass.mutable_driver_license();
    UpsertPrivatePassRequest request(
        pass, consent_auditor::ConsentAuditor::GenerateSessionId(),
        callback.GetCallback());
    net::HttpRequestHeaders headers = request.GetRequestHeaders();
    EXPECT_EQ(headers.GetHeader("EES-S7E-Mode"), "proto");
    EXPECT_EQ(headers.GetHeader("EES-Proto-Tokenization"), "1.2.2;574");
  }

  {
    PrivatePass pass;
    pass.mutable_passport();
    UpsertPrivatePassRequest request(
        pass, consent_auditor::ConsentAuditor::GenerateSessionId(),
        callback.GetCallback());
    net::HttpRequestHeaders headers = request.GetRequestHeaders();
    EXPECT_EQ(headers.GetHeader("EES-S7E-Mode"), "proto");
    EXPECT_EQ(headers.GetHeader("EES-Proto-Tokenization"), "1.3.2;574");
  }
}
// Tests that GetRequestContent generates the correct proto request body.
TEST_F(UpsertPrivatePassRequestTest, GetRequestContent) {
  PrivatePass pass;
  pass.set_pass_id("pass-id");
  auto* passport = pass.mutable_passport();
  passport->set_owner_name("owner");
  passport->set_passport_number("number");
  passport->set_country_code("US");

  UpsertPrivatePassCallback callback;
  UpsertPrivatePassRequest request(pass, std::nullopt, callback.GetCallback());

  std::string request_body = request.GetRequestContent();
  api::UpsertPrivatePassRequest request_proto;
  ASSERT_TRUE(request_proto.ParseFromString(request_body));

  EXPECT_EQ(request_proto.private_pass().pass_id(), "pass-id");
  EXPECT_EQ(request_proto.private_pass().passport().owner_name(), "owner");
  EXPECT_EQ(request_proto.private_pass().passport().passport_number(),
            "number");
  EXPECT_EQ(request_proto.private_pass().passport().country_code(), "US");
  EXPECT_EQ(request_proto.client_info().chrome_client_info().version(),
            version_info::GetVersionNumber());
  // No session ID was provided to the constructor and hence no session_id
  // should be set. Since the `pass` has an ID and the request thus corresponds
  // to an update, providing a session ID in this test is unrealistic.
  EXPECT_FALSE(request_proto.has_session_id());
}

// Tests that GetRequestContent sets a session ID, if specified through the
// constructor.
TEST_F(UpsertPrivatePassRequestTest, GetRequestContent_SessionId) {
  PrivatePass pass;
  consent_auditor::ConsentAuditor::SessionId session_id =
      consent_auditor::ConsentAuditor::GenerateSessionId();
  UpsertPrivatePassCallback callback;
  UpsertPrivatePassRequest request(pass, session_id, callback.GetCallback());

  std::string request_body = request.GetRequestContent();
  api::UpsertPrivatePassRequest request_proto;
  ASSERT_TRUE(request_proto.ParseFromString(request_body));

  absl::uint128 session_id_int = session_id.AsInteger();
  EXPECT_EQ(request_proto.session_id().uuid().most_significant_uuid_bits(),
            absl::Uint128High64(session_id_int));
  EXPECT_EQ(request_proto.session_id().uuid().least_significant_uuid_bits(),
            absl::Uint128Low64(session_id_int));
}

// Tests that OnResponse handles a successful HTTP response.
TEST_F(UpsertPrivatePassRequestTest, OnResponse_Success) {
  UpsertPrivatePassCallback callback;
  PrivatePass pass;
  pass.mutable_passport();
  UpsertPrivatePassRequest request(
      pass, consent_auditor::ConsentAuditor::GenerateSessionId(),
      callback.GetCallback());

  api::UpsertPrivatePassResponse response_proto;
  response_proto.mutable_private_pass()->set_pass_id("returned-id");

  std::move(request).OnResponse(response_proto.SerializeAsString());

  ASSERT_TRUE(callback.Wait());
  EXPECT_TRUE(callback.Get().has_value());
  EXPECT_EQ(callback.Get()->pass_id(), "returned-id");
}

// Tests that OnResponse handles an error HTTP response.
TEST_F(UpsertPrivatePassRequestTest, OnResponse_Error) {
  UpsertPrivatePassCallback callback;
  PrivatePass pass;
  pass.mutable_passport();
  UpsertPrivatePassRequest request(
      pass, consent_auditor::ConsentAuditor::GenerateSessionId(),
      callback.GetCallback());

  std::move(request).OnResponse(base::unexpected(
      WalletHttpClient::WalletRequestError::kAccessTokenFetchFailed));

  ASSERT_TRUE(callback.Wait());
  ASSERT_FALSE(callback.Get().has_value());
  EXPECT_EQ(callback.Get().error(),
            WalletHttpClient::WalletRequestError::kAccessTokenFetchFailed);
}

// Tests that OnResponse handles a parse error.
TEST_F(UpsertPrivatePassRequestTest, OnResponse_ParseError) {
  UpsertPrivatePassCallback callback;
  PrivatePass pass;
  pass.mutable_passport();
  UpsertPrivatePassRequest request(
      pass, consent_auditor::ConsentAuditor::GenerateSessionId(),
      callback.GetCallback());

  std::move(request).OnResponse("invalid-proto");

  ASSERT_TRUE(callback.Wait());
  ASSERT_FALSE(callback.Get().has_value());
  EXPECT_EQ(callback.Get().error(),
            WalletHttpClient::WalletRequestError::kParseResponseFailed);
}

}  // namespace
}  // namespace wallet
