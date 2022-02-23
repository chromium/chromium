// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cup.h"

#include "base/test/scoped_feature_list.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

namespace cup {

class CUPTest : public testing::Test {
 public:
  CUPTest() = default;
  ~CUPTest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  void InitCupFeatures(bool enableSigning, bool enableVerifying) {
    std::vector<base::Feature> enabled_features;
    std::vector<base::Feature> disabled_features;

    if (enableSigning) {
      enabled_features.push_back(
          features::kAutofillAssistantSignGetActionsRequests);
    } else {
      disabled_features.push_back(
          features::kAutofillAssistantSignGetActionsRequests);
    }

    if (enableVerifying) {
      enabled_features.push_back(
          features::kAutofillAssistantVerifyGetActionsResponses);
    } else {
      disabled_features.push_back(
          features::kAutofillAssistantVerifyGetActionsResponses);
    }

    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
};

TEST_F(CUPTest, ShouldSignAndVerify) {
  struct TestCase {
    bool sign_requests;
    bool verify_responses;
    RpcType rpc_type;
    bool expected_should_sign;
    bool expected_should_verify;
  };
  std::vector<TestCase> test_cases = {
      {true, true, RpcType::GET_ACTIONS, true, true},
      {true, false, RpcType::GET_ACTIONS, true, false},
      {false, true, RpcType::GET_ACTIONS, false, false},
      {false, false, RpcType::GET_ACTIONS, false, false},
      {false, false, RpcType::GET_TRIGGER_SCRIPTS, false, false},
      {true, true, RpcType::GET_TRIGGER_SCRIPTS, false, false},
  };

  RpcType unsupported_rpc_types[] = {
      RpcType::UNKNOWN,
      RpcType::GET_TRIGGER_SCRIPTS,
      RpcType::SUPPORTS_SCRIPT,
  };
  for (const auto& unsupported_type : unsupported_rpc_types) {
    test_cases.push_back({true, true, unsupported_type, false, false});
    test_cases.push_back({true, false, unsupported_type, false, false});
    test_cases.push_back({false, true, unsupported_type, false, false});
    test_cases.push_back({false, false, unsupported_type, false, false});
  }

  for (const auto& test_case : test_cases) {
    InitCupFeatures(test_case.sign_requests, test_case.verify_responses);
    EXPECT_EQ(ShouldSignRequests(test_case.rpc_type),
              test_case.expected_should_sign);
    EXPECT_EQ(ShouldVerifyResponses(test_case.rpc_type),
              test_case.expected_should_verify);
  }
}

}  // namespace cup

}  // namespace autofill_assistant
