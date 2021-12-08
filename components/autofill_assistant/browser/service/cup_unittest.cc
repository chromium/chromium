// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cup.h"

#include "base/test/scoped_feature_list.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

class CUPTest : public testing::Test {
 public:
  CUPTest() : cup_{autofill_assistant::CUP::CreateQuerySigner()} {}
  ~CUPTest() override = default;

 protected:
  autofill_assistant::CUP cup_;
  base::test::ScopedFeatureList scoped_feature_list_;

  void InitCupFeatures(bool enableSigning, bool enableVerifying) {
    std::vector<base::Feature> enabled_features;
    std::vector<base::Feature> disabled_features;

    if (enableSigning) {
      enabled_features.push_back(autofill_assistant::features::
                                     kAutofillAssistantSignGetActionsRequests);
    } else {
      disabled_features.push_back(autofill_assistant::features::
                                      kAutofillAssistantSignGetActionsRequests);
    }

    if (enableVerifying) {
      enabled_features.push_back(
          autofill_assistant::features::
              kAutofillAssistantVerifyGetActionsResponses);
    } else {
      disabled_features.push_back(
          autofill_assistant::features::
              kAutofillAssistantVerifyGetActionsResponses);
    }

    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
};

TEST_F(CUPTest, ShouldSignGetActionsRequestWhenFeatureActivated) {
  InitCupFeatures(true, false);

  EXPECT_TRUE(autofill_assistant::CUP::ShouldSignRequests(
      autofill_assistant::RpcType::GET_ACTIONS));
}

TEST_F(CUPTest, ShouldNotSignGetActionsRequestWhenFeatureNotActivated) {
  InitCupFeatures(false, false);

  EXPECT_FALSE(autofill_assistant::CUP::ShouldSignRequests(
      autofill_assistant::RpcType::GET_ACTIONS));
}

TEST_F(CUPTest, ShouldNotSignNotGetActionsRequest) {
  InitCupFeatures(true, false);

  EXPECT_FALSE(autofill_assistant::CUP::ShouldSignRequests(
      autofill_assistant::RpcType::GET_TRIGGER_SCRIPTS));
}

TEST_F(CUPTest, ShouldVerifyGetActionsResponseWhenFeatureActivated) {
  InitCupFeatures(true, true);

  EXPECT_TRUE(autofill_assistant::CUP::ShouldVerifyResponses(
      autofill_assistant::RpcType::GET_ACTIONS));
}

TEST_F(CUPTest, ShouldNotVerifyGetActionsResponseWhenFeatureNotActivated) {
  InitCupFeatures(true, false);

  EXPECT_FALSE(autofill_assistant::CUP::ShouldVerifyResponses(
      autofill_assistant::RpcType::GET_ACTIONS));
}

TEST_F(CUPTest, ShouldNotVerifyGetActionsResponseWhenSigningNotActivated) {
  InitCupFeatures(false, true);

  EXPECT_FALSE(autofill_assistant::CUP::ShouldVerifyResponses(
      autofill_assistant::RpcType::GET_ACTIONS));
}

TEST_F(CUPTest, ShouldNotVerifyNotGetActionsResponse) {
  InitCupFeatures(true, true);

  EXPECT_FALSE(autofill_assistant::CUP::ShouldVerifyResponses(
      autofill_assistant::RpcType::GET_TRIGGER_SCRIPTS));
}

TEST_F(CUPTest, PacksAndSignsGetActionsRequest) {
  InitCupFeatures(true, false);

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

TEST_F(CUPTest, UnpacksTrustedGetActionsResponse) {
  // TODO(b/203031699): Write test for the successful case.
}

TEST_F(CUPTest, FailsToUnpackNonTrustedGetActionsResponse) {
  InitCupFeatures(true, true);

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

}  // namespace
