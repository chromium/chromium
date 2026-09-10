// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/network/get_details_for_upsert_pass_request.h"

#include <utility>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/version_info/version_info.h"
#include "components/wallet/core/browser/proto/api_v1.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace wallet {
namespace {

class GetDetailsForUpsertPassRequestTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

using GetDetailsForUpsertPassCallback = base::test::TestFuture<
    base::expected<WalletHttpClient::PassUpsertDetails,
                   WalletHttpClient::WalletRequestError>>;

// Tests that GetRequestUrlPath returns the correct URL path.
TEST_F(GetDetailsForUpsertPassRequestTest, GetRequestUrlPath) {
  GetDetailsForUpsertPassCallback callback;
  GetDetailsForUpsertPassRequest request(
      WalletHttpClient::PassType::kVehicleRegistration, callback.GetCallback());

  EXPECT_EQ(request.GetRequestUrlPath(), "v1/passes:getDetailsForUpsert");
}

// Tests that GetRequestContent generates the correct proto request body.
TEST_F(GetDetailsForUpsertPassRequestTest, GetRequestContent) {
  GetDetailsForUpsertPassCallback callback;
  GetDetailsForUpsertPassRequest request(
      WalletHttpClient::PassType::kVehicleRegistration, callback.GetCallback());

  std::string request_body = request.GetRequestContent();
  api::GetDetailsForUpsertPassRequest request_proto;
  ASSERT_TRUE(request_proto.ParseFromString(request_body));

  EXPECT_EQ(request_proto.client_info().chrome_client_info().version(),
            version_info::GetVersionNumber());
  EXPECT_EQ(
      request_proto.pass_type(),
      api::GetDetailsForUpsertPassRequest::PASS_TYPE_VEHICLE_REGISTRATION);
}

// Tests that OnResponse handles a successful HTTP response.
TEST_F(GetDetailsForUpsertPassRequestTest, OnResponse_Success) {
  GetDetailsForUpsertPassCallback callback;
  GetDetailsForUpsertPassRequest request(
      WalletHttpClient::PassType::kVehicleRegistration, callback.GetCallback());

  api::GetDetailsForUpsertPassResponse response_proto;
  response_proto.set_context_token("test_context_token");
  LegalMessage* legal_message = response_proto.mutable_legal_message();
  legal_message->set_token("test_legal_message_token");
  LegalMessage::Line* line = legal_message->add_line();
  line->set_template_("By continuing, you agree to {0}.");
  LegalMessage::Link* param = line->add_template_parameter();
  param->set_display_text("Terms");
  param->set_url("https://example.com/terms");

  std::move(request).OnResponse(response_proto.SerializeAsString());

  ASSERT_TRUE(callback.Wait());
  ASSERT_TRUE(callback.Get().has_value());
  EXPECT_EQ(callback.Get()->context_token, "test_context_token");
  ASSERT_TRUE(callback.Get()->legal_message.has_value());
  EXPECT_EQ(callback.Get()->legal_message->SerializeAsString(),
            response_proto.legal_message().SerializeAsString());
}

// Tests that OnResponse handles an error HTTP response.
TEST_F(GetDetailsForUpsertPassRequestTest, OnResponse_HttpError) {
  GetDetailsForUpsertPassCallback callback;
  GetDetailsForUpsertPassRequest request(
      WalletHttpClient::PassType::kVehicleRegistration, callback.GetCallback());

  std::move(request).OnResponse(base::unexpected(
      WalletHttpClient::WalletRequestError::kAccessTokenFetchFailed));

  ASSERT_TRUE(callback.Wait());
  ASSERT_FALSE(callback.Get().has_value());
  EXPECT_EQ(callback.Get().error(),
            WalletHttpClient::WalletRequestError::kAccessTokenFetchFailed);
}

// Tests that OnResponse handles a parse error.
TEST_F(GetDetailsForUpsertPassRequestTest, OnResponse_ParseError) {
  GetDetailsForUpsertPassCallback callback;
  GetDetailsForUpsertPassRequest request(
      WalletHttpClient::PassType::kVehicleRegistration, callback.GetCallback());

  std::move(request).OnResponse("invalid-proto");

  ASSERT_TRUE(callback.Wait());
  ASSERT_FALSE(callback.Get().has_value());
  EXPECT_EQ(callback.Get().error(),
            WalletHttpClient::WalletRequestError::kParseResponseFailed);
}

}  // namespace
}  // namespace wallet
