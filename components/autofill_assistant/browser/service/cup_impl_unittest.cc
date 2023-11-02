// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cup_impl.h"
#include "base/base64.h"
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

// kExampleRequestBase64 is the base64 encoded version of `
//   ScriptActionRequestProto {
//     InitialRequest {
//       StructuredQuery {
//         url: "https://www.example.com/123",
//       }
//     }
//   }`
const base::StringPiece kExampleRequestBase64 =
    "Ih8aHRIbaHR0cHM6Ly93d3cuZXhhbXBsZS5jb20vMTIz";
// kExampleResponseBase64 is the base64 encoded version of `
//   ActionsResponseProto {
//    warnings: "foo",
//   }`
const base::StringPiece kExampleResponseBase64 = "SgNmb28=";
// Dev autofill_assistant public key.
const base::StringPiece kPublicKeyBase64 =
    "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAERgn63oKhDjiDbmgYC/pyOlKjiIpFlQH/"
    "CsevP5NynMB4HLF95EPFA4eDobcBDSlRl6GIXoNlKa7GhXs6FkfQbg==";
const base::StringPiece kPublicKeyVersion = "8";
const int kPublicKeyVersionInt = 8;
const uint32_t kExampleNonce = 12345;

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

TEST(CUPImplTest, PacksAndSignsGetNoRoundTripByHashRequest) {
  cup::CUPImpl cup{cup::CUPImpl::CreateQuerySigner(),
                   RpcType::GET_NO_ROUNDTRIP_SCRIPTS_BY_HASH_PREFIX};
  GetNoRoundTripScriptsByHashPrefixRequestProto user_request;
  user_request.mutable_client_context()->set_experiment_ids("test");
  std::string user_request_str;
  user_request.SerializeToString(&user_request_str);

  auto packed_request_str = cup.PackAndSignRequest(user_request_str);

  GetNoRoundTripScriptsByHashPrefixRequestProto packed_request;
  EXPECT_TRUE(packed_request.ParseFromString(packed_request_str));
  EXPECT_TRUE(packed_request.client_context().experiment_ids().empty());
  EXPECT_FALSE(packed_request.cup_data().request().empty());
  EXPECT_FALSE(packed_request.cup_data().query_cup2key().empty());
  EXPECT_FALSE(packed_request.cup_data().hash_hex().empty());

  GetNoRoundTripScriptsByHashPrefixRequestProto actual_user_request;
  EXPECT_TRUE(
      actual_user_request.ParseFromString(packed_request.cup_data().request()));
  EXPECT_EQ(actual_user_request.client_context().experiment_ids(), "test");
  EXPECT_FALSE(actual_user_request.has_cup_data());
}

TEST(CUPImplTest, VerifiesTrustedGetActionsResponse) {
  base::HistogramTester histogram_tester;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAutofillAssistantCupPublicKeyBase64, kPublicKeyBase64);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAutofillAssistantCupKeyVersion, kPublicKeyVersion);

  // Valid server etag generated from the server for the autofill_assistant key
  // for the example request, response and nonce in this test.
  const base::StringPiece kServerEtag =
      "3045"
      "02200789b2dfa79204405f20d54cf35ed0aef948acb6eb2f91e1d90a6c229ed42dcf"
      "022100f17de1439313fab4eeb0f1a99127278edcc0aae50bbc1e248dc28f056f018dfc"
      ":27a8e08b3595e2b17d22bd9a50cd63dea36c8a9bbb91c0ff119bbf1254a9f5c3";

  // Sign the request and override the nonce so that result is deterministic.
  cup::CUPImpl cup(cup::CUPImpl::CreateQuerySigner(), RpcType::GET_ACTIONS);
  std::string request_bytes;
  ASSERT_TRUE(base::Base64Decode(kExampleRequestBase64, &request_bytes));
  cup.PackAndSignRequest(request_bytes);
  cup.GetQuerySigner().OverrideNonceForTesting(kPublicKeyVersionInt,
                                               kExampleNonce);

  // Construct server response as it would have been received by the client.
  autofill_assistant::ActionsResponseProto packed_response;
  packed_response.mutable_cup_data()->set_ecdsa_signature(
      std::string(kServerEtag));
  std::string serialized_response_bytes;
  ASSERT_TRUE(
      base::Base64Decode(kExampleResponseBase64, &serialized_response_bytes));
  packed_response.mutable_cup_data()->set_response(serialized_response_bytes);
  std::string serialized_packed_response;
  packed_response.SerializeToString(&serialized_packed_response);

  // Expect that unpacking gives as a result the cup_data.response field of
  // the packed response proto.
  EXPECT_EQ(cup.UnpackResponse(serialized_packed_response),
            serialized_response_bytes);
  histogram_tester.ExpectBucketCount(
      "Android.AutofillAssistant.CupRpcVerificationEvent",
      Metrics::CupRpcVerificationEvent::VERIFICATION_FAILED, 0);
  histogram_tester.ExpectBucketCount(
      "Android.AutofillAssistant.CupRpcVerificationEvent",
      Metrics::CupRpcVerificationEvent::VERIFICATION_SUCCEEDED, 1);
}

TEST(CUPImplTest, VerifiesTrustedGetNoRoundTripScriptsByHashResponse) {
  base::HistogramTester histogram_tester;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAutofillAssistantCupPublicKeyBase64, kPublicKeyBase64);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAutofillAssistantCupKeyVersion, kPublicKeyVersion);

  // Valid server etag generated from the server for the autofill_assistant key
  // for the example request, response and nonce in this test.
  const base::StringPiece server_etag =
      "3044"
      "0220409f3dcc50f9e5a1708c845dd062fe657c636cb63f14f0b68385413f0cbc"
      "4208022046586b13463d8171e87fb1c6f7fbf26ad0eb7ab7fe9139063523e48476203c27"
      ":976b91e914e68d5c8907b1e9752b814ff6c1986276cec6e0d0c92d90eb02838f";
  // Sign the request and override the nonce so that result is deterministic.
  cup::CUPImpl cup(cup::CUPImpl::CreateQuerySigner(),
                   RpcType::GET_NO_ROUNDTRIP_SCRIPTS_BY_HASH_PREFIX);

  // Base64 encoded version of
  //   GetNoRoundTripByHashResponseProto {
  //    hash_prefix: 64,
  //    hash_prefix_length: 15
  //   }`
  const std::string request_base64 = "EEAIDw==";
  std::string request_bytes;
  ASSERT_TRUE(base::Base64Decode(request_base64, &request_bytes));
  cup.PackAndSignRequest(request_bytes);
  cup.GetQuerySigner().OverrideNonceForTesting(kPublicKeyVersionInt,
                                               kExampleNonce);

  // Construct server response as it would have been received by the client.
  GetNoRoundTripScriptsByHashPrefixResponseProto packed_response;
  packed_response.mutable_cup_data()->set_ecdsa_signature(
      std::string(server_etag));

  // Base64 encoded version of
  //   GetNoRoundTripByHashResponseProto {
  //    warnings: "foo",
  //   }`
  const std::string response_base64 = "GgNmb28=";
  std::string serialized_response_bytes;
  ASSERT_TRUE(base::Base64Decode(response_base64, &serialized_response_bytes));
  packed_response.mutable_cup_data()->set_response(serialized_response_bytes);
  std::string serialized_packed_response;
  packed_response.SerializeToString(&serialized_packed_response);

  // Expect that unpacking gives as a result the cup_data.response field of
  // the packed response proto.
  EXPECT_EQ(cup.UnpackResponse(serialized_packed_response),
            serialized_response_bytes);
  histogram_tester.ExpectBucketCount(
      "Android.AutofillAssistant.CupRpcVerificationEvent",
      Metrics::CupRpcVerificationEvent::VERIFICATION_FAILED, 0);
  histogram_tester.ExpectBucketCount(
      "Android.AutofillAssistant.CupRpcVerificationEvent",
      Metrics::CupRpcVerificationEvent::VERIFICATION_SUCCEEDED, 1);
}

TEST(CUPImplTest, FailsToVerifySignatureFromDifferentKey) {
  base::HistogramTester histogram_tester;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAutofillAssistantCupPublicKeyBase64, kPublicKeyBase64);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAutofillAssistantCupKeyVersion, kPublicKeyVersion);

  // This is a valid server etag generated from a different private key for the
  // same request, response and nonce provided for this test; following the
  // procedure:
  //   openssl ecparam -genkey -name prime256v1 -out ecpriv.pem
  //   echo -n Ih8aHRIbaHR0cHM6Ly93d3cuZXhhbXBsZS5jb20vMTIz | base64 -d | \
  //     sha256sum -b | cut -d " " -f 1 > h
  //
  //   echo -n SgNmb28= | base64 -d | sha256sum -b | cut -d " " -f 1 >> h
  //   cat h | xxd -r -p > hbin
  //   echo -n 8:12345 >> hbin
  //   sha256sum hbin | cut -d " " -f 1 | xxd -r -p > hbin2
  //   openssl dgst -hex -sha256 -sign ecpriv.pem hbin2 | cut -d " " -f 2 > sig
  //   echo -n : >> sig
  //   echo -n Ih8aHRIbaHR0cHM6Ly93d3cuZXhhbXBsZS5jb20vMTIz | base64 -d | \
  //     sha256sum -b | cut -d " " -f 1 >> sig
  //   cat sig
  const base::StringPiece kServerEtag =
      "3044"
      "022009d5e42b3eacd0a859d182a158c9feece557d31a0276ecae01a693c88d7a71a9"
      "0220084d131af48010fe56b6399e6d6f83d88ca30236100fc34c959baed79ddb8862"
      ":27a8e08b3595e2b17d22bd9a50cd63dea36c8a9bbb91c0ff119bbf1254a9f5c3";

  // Sign the request and override the nonce so that result is deterministic.
  cup::CUPImpl cup(cup::CUPImpl::CreateQuerySigner(), RpcType::GET_ACTIONS);
  std::string request_bytes;
  ASSERT_TRUE(base::Base64Decode(kExampleRequestBase64, &request_bytes));
  cup.PackAndSignRequest(request_bytes);
  cup.GetQuerySigner().OverrideNonceForTesting(kPublicKeyVersionInt,
                                               kExampleNonce);

  // Construct server response as it would have been received by the client.
  autofill_assistant::ActionsResponseProto packed_response;
  packed_response.mutable_cup_data()->set_ecdsa_signature(
      std::string(kServerEtag));
  std::string serialized_response_bytes;
  ASSERT_TRUE(
      base::Base64Decode(kExampleResponseBase64, &serialized_response_bytes));
  packed_response.mutable_cup_data()->set_response(serialized_response_bytes);
  std::string serialized_packed_response;
  packed_response.SerializeToString(&serialized_packed_response);

  // Expect to receive an empty optional as verification fails.
  EXPECT_EQ(cup.UnpackResponse(serialized_packed_response), absl::nullopt);
  histogram_tester.ExpectBucketCount(
      "Android.AutofillAssistant.CupRpcVerificationEvent",
      Metrics::CupRpcVerificationEvent::VERIFICATION_FAILED, 1);
  histogram_tester.ExpectBucketCount(
      "Android.AutofillAssistant.CupRpcVerificationEvent",
      Metrics::CupRpcVerificationEvent::VERIFICATION_SUCCEEDED, 0);
}

TEST(CUPImplTest, FailsToVerifyWithNonValidSignature) {
  base::HistogramTester histogram_tester;
  cup::CUPImpl cup{cup::CUPImpl::CreateQuerySigner(), RpcType::GET_ACTIONS};
  ScriptActionRequestProto user_request;
  std::string user_request_str;
  user_request.SerializeToString(&user_request_str);

  cup.PackAndSignRequest(user_request_str);
  cup.GetQuerySigner().OverrideNonceForTesting(kPublicKeyVersionInt,
                                               kExampleNonce);

  autofill_assistant::ActionsResponseProto packed_response;
  packed_response.mutable_cup_data()->set_response("a response");
  packed_response.mutable_cup_data()->set_ecdsa_signature("not a signature");
  std::string packed_response_str;
  packed_response.SerializeToString(&packed_response_str);

  EXPECT_EQ(cup.UnpackResponse(packed_response_str), absl::nullopt);
  histogram_tester.ExpectUniqueSample(
      "Android.AutofillAssistant.CupRpcVerificationEvent",
      Metrics::CupRpcVerificationEvent::VERIFICATION_FAILED, 1);
}

TEST(CUPImplTest, FailsToVerifyWithEmptySignature) {
  base::HistogramTester histogram_tester;
  cup::CUPImpl cup{cup::CUPImpl::CreateQuerySigner(), RpcType::GET_ACTIONS};
  ScriptActionRequestProto user_request;
  std::string user_request_str;
  user_request.SerializeToString(&user_request_str);

  cup.PackAndSignRequest(user_request_str);
  cup.GetQuerySigner().OverrideNonceForTesting(kPublicKeyVersionInt,
                                               kExampleNonce);

  autofill_assistant::ActionsResponseProto packed_response;
  packed_response.mutable_cup_data()->set_response("a response");
  std::string packed_response_str;
  packed_response.SerializeToString(&packed_response_str);

  EXPECT_EQ(cup.UnpackResponse(packed_response_str), absl::nullopt);
  histogram_tester.ExpectUniqueSample(
      "Android.AutofillAssistant.CupRpcVerificationEvent",
      Metrics::CupRpcVerificationEvent::EMPTY_SIGNATURE, 1);
}

TEST(CUPImplTest, FailsToParseInvalidProtoResponse) {
  base::HistogramTester histogram_tester;
  cup::CUPImpl cup{cup::CUPImpl::CreateQuerySigner(), RpcType::GET_ACTIONS};
  ScriptActionRequestProto user_request;
  std::string user_request_str;
  user_request.SerializeToString(&user_request_str);

  cup.PackAndSignRequest(user_request_str);
  cup.GetQuerySigner().OverrideNonceForTesting(kPublicKeyVersionInt,
                                               kExampleNonce);

  EXPECT_EQ(cup.UnpackResponse("invalid proto"), absl::nullopt);
  histogram_tester.ExpectUniqueSample(
      "Android.AutofillAssistant.CupRpcVerificationEvent",
      Metrics::CupRpcVerificationEvent::PARSING_FAILED, 1);
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
