// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cup_impl.h"
#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/switches.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

namespace {

TEST(CUPImplTest, PacksAndSignsGetActionsRequest) {
  cup::CUPImpl cup{cup::CUPImpl::CreateQuerySigner(), RpcType::GET_ACTIONS};
  ScriptActionRequestProto user_request;
  user_request.mutable_client_context()->set_experiment_ids("test");
  std::string user_request_str;
  user_request.SerializeToString(&user_request_str);

  auto packed_request_str = cup.PackAndSignRequest(user_request_str);

  ScriptActionRequestProto packed_request;
  EXPECT_TRUE(packed_request.ParseFromString(packed_request_str));
  EXPECT_TRUE(packed_request.client_context().experiment_ids().empty());
  EXPECT_FALSE(packed_request.cup_data().request().empty());
  EXPECT_FALSE(packed_request.cup_data().query_cup2key().empty());
  EXPECT_FALSE(packed_request.cup_data().hash_hex().empty());

  ScriptActionRequestProto actual_user_request;
  EXPECT_TRUE(
      actual_user_request.ParseFromString(packed_request.cup_data().request()));
  EXPECT_EQ(actual_user_request.client_context().experiment_ids(), "test");
  EXPECT_FALSE(actual_user_request.has_cup_data());
}

TEST(CUPImplTest, IgnoresNonGetActionsRequest) {
  cup::CUPImpl cup{cup::CUPImpl::CreateQuerySigner(),
                   RpcType::GET_TRIGGER_SCRIPTS};

  EXPECT_EQ(cup.PackAndSignRequest("a request"), "a request");
}

TEST(CUPImplTest, FailsToVerifyNonTrustedGetActionsResponse) {
  base::HistogramTester histogram_tester;
  cup::CUPImpl cup{cup::CUPImpl::CreateQuerySigner(), RpcType::GET_ACTIONS};
  ScriptActionRequestProto user_request;
  std::string user_request_str;
  user_request.SerializeToString(&user_request_str);

  cup.PackAndSignRequest(user_request_str);
  cup.GetQuerySigner().OverrideNonceForTesting(8, 12345);

  autofill_assistant::ActionsResponseProto packed_response;
  packed_response.mutable_cup_data()->set_ecdsa_signature("not a signature");

  EXPECT_EQ(cup.UnpackResponse(""), absl::nullopt);
  histogram_tester.ExpectUniqueSample(
      "Android.AutofillAssistant.CupRpcVerificationEvent",
      Metrics::CupRpcVerificationEvent::VERIFICATION_FAILED, 1);
}

TEST(CUPImplTest, FailsToParseInvalidProtoResponse) {
  base::HistogramTester histogram_tester;
  cup::CUPImpl cup{cup::CUPImpl::CreateQuerySigner(), RpcType::GET_ACTIONS};
  ScriptActionRequestProto user_request;
  std::string user_request_str;
  user_request.SerializeToString(&user_request_str);

  cup.PackAndSignRequest(user_request_str);
  cup.GetQuerySigner().OverrideNonceForTesting(8, 12345);

  EXPECT_EQ(cup.UnpackResponse("invalid proto"), absl::nullopt);
  histogram_tester.ExpectUniqueSample(
      "Android.AutofillAssistant.CupRpcVerificationEvent",
      Metrics::CupRpcVerificationEvent::PARSING_FAILED, 1);
}

TEST(CUPImplTest, IgnoresNonGetActionsResponse) {
  cup::CUPImpl cup{cup::CUPImpl::CreateQuerySigner(),
                   RpcType::GET_TRIGGER_SCRIPTS};

  absl::optional<std::string> unpacked_response =
      cup.UnpackResponse("a response");
  EXPECT_EQ(*unpacked_response, "a response");
}

TEST(CUPImplTest, OverridesEcdsaPublicKeyWithCLIValue) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAutofillAssistantCupPublicKeyBase64, "SGVsbG8=");
  EXPECT_EQ(cup::CUPImpl::GetPublicKey(), "Hello");
}

TEST(CUPImplTest, HasValidEcdsaPublicKeyWithNotValidCLIValue) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAutofillAssistantCupPublicKeyBase64, "Not valid base64");
  EXPECT_FALSE(cup::CUPImpl::GetPublicKey().empty());
}

TEST(CUPImplTest, OverridesEcdsaKeyVersionithCLIValue) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAutofillAssistantCupKeyVersion, "15");
  EXPECT_EQ(cup::CUPImpl::GetKeyVersion(), 15);
}

TEST(CUPImplTest, HasValidEcdsaKeyVersionWithNotValidCLIValue) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAutofillAssistantCupKeyVersion, "Not a number");
  EXPECT_GT(cup::CUPImpl::GetKeyVersion(), -1);
}

}  // namespace

}  // namespace autofill_assistant
