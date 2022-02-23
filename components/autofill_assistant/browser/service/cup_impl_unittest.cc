// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cup_impl.h"

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/switches.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

TEST(CUPImplTest, PacksAndSignsGetActionsRequest) {
  autofill_assistant::cup::CUPImpl cup_{
      autofill_assistant::cup::CUPImpl::CreateQuerySigner(),
      autofill_assistant::RpcType::GET_ACTIONS};
  autofill_assistant::ScriptActionRequestProto user_request;
  user_request.mutable_client_context()->set_experiment_ids("test");
  std::string user_request_str;
  user_request.SerializeToString(&user_request_str);

  auto packed_request_str = cup_.PackAndSignRequest(user_request_str);

  autofill_assistant::ScriptActionRequestProto packed_request;
  EXPECT_TRUE(packed_request.ParseFromString(packed_request_str));
  EXPECT_TRUE(packed_request.client_context().experiment_ids().empty());
  EXPECT_FALSE(packed_request.cup_data().request().empty());
  EXPECT_FALSE(packed_request.cup_data().query_cup2key().empty());
  EXPECT_FALSE(packed_request.cup_data().hash_hex().empty());

  autofill_assistant::ScriptActionRequestProto actual_user_request;
  EXPECT_TRUE(
      actual_user_request.ParseFromString(packed_request.cup_data().request()));
  EXPECT_EQ(actual_user_request.client_context().experiment_ids(), "test");
  EXPECT_FALSE(actual_user_request.has_cup_data());
}

TEST(CUPImplTest, IgnoresNonGetActionsRequest) {
  autofill_assistant::cup::CUPImpl cup_{
      autofill_assistant::cup::CUPImpl::CreateQuerySigner(),
      autofill_assistant::RpcType::GET_TRIGGER_SCRIPTS};

  EXPECT_EQ(cup_.PackAndSignRequest("a request"), "a request");
}

TEST(CUPImplTest, UnpacksTrustedGetActionsResponse) {
  // TODO(b/203031699): Write test for the successful case.
}

TEST(CUPImplTest, FailsToUnpackNonTrustedGetActionsResponse) {
  autofill_assistant::cup::CUPImpl cup_{
      autofill_assistant::cup::CUPImpl::CreateQuerySigner(),
      autofill_assistant::RpcType::GET_ACTIONS};
  autofill_assistant::ScriptActionRequestProto user_request;
  user_request.mutable_client_context()->set_experiment_ids("123");
  std::string user_request_str;
  user_request.SerializeToString(&user_request_str);

  cup_.PackAndSignRequest(user_request_str);
  cup_.GetQuerySigner().OverrideNonceForTesting(8, 12345);

  autofill_assistant::ActionsResponseProto user_response;
  user_response.set_global_payload("adsf");
  std::string user_response_str;
  user_response.SerializeToString(&user_response_str);
  autofill_assistant::ActionsResponseProto packed_response;
  packed_response.mutable_cup_data()->set_response(user_response_str);
  packed_response.mutable_cup_data()->set_ecdsa_signature("not a signature");

  EXPECT_EQ(cup_.UnpackResponse(user_response_str), absl::nullopt);
}

TEST(CUPImplTest, IgnoresNonGetActionsResponse) {
  autofill_assistant::cup::CUPImpl cup_{
      autofill_assistant::cup::CUPImpl::CreateQuerySigner(),
      autofill_assistant::RpcType::GET_TRIGGER_SCRIPTS};

  absl::optional<std::string> unpacked_response =
      cup_.UnpackResponse("a response");
  EXPECT_EQ(*unpacked_response, "a response");
}

TEST(CUPImplTest, OverridesEcdsaPublicKeyWithCLIValue) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      autofill_assistant::switches::kAutofillAssistantCupPublicKeyBase64,
      "SGVsbG8=");
  EXPECT_EQ(autofill_assistant::cup::CUPImpl::GetPublicKey(), "Hello");
}

TEST(CUPImplTest, HasValidEcdsaPublicKeyWithNotValidCLIValue) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      autofill_assistant::switches::kAutofillAssistantCupPublicKeyBase64,
      "Not valid base64");
  EXPECT_FALSE(autofill_assistant::cup::CUPImpl::GetPublicKey().empty());
}

TEST(CUPImplTest, OverridesEcdsaKeyVersionithCLIValue) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      autofill_assistant::switches::kAutofillAssistantCupKeyVersion, "15");
  EXPECT_EQ(autofill_assistant::cup::CUPImpl::GetKeyVersion(), 15);
}

TEST(CUPImplTest, HasValidEcdsaKeyVersionWithNotValidCLIValue) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      autofill_assistant::switches::kAutofillAssistantCupKeyVersion,
      "Not a number");
  EXPECT_GT(autofill_assistant::cup::CUPImpl::GetKeyVersion(), -1);
}

}  // namespace
