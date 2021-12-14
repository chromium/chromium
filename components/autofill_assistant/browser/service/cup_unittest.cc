// Copyright 2021 The Chromium Authors. All rights reserved.
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
  CUPTest() = default;
  ~CUPTest() override = default;

 protected:
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

  EXPECT_TRUE(autofill_assistant::cup::ShouldSignRequests(
      autofill_assistant::RpcType::GET_ACTIONS));
}

TEST_F(CUPTest, ShouldNotSignGetActionsRequestWhenFeatureNotActivated) {
  InitCupFeatures(false, false);

  EXPECT_FALSE(autofill_assistant::cup::ShouldSignRequests(
      autofill_assistant::RpcType::GET_ACTIONS));
}

TEST_F(CUPTest, ShouldNotSignNotGetActionsRequest) {
  InitCupFeatures(true, false);

  EXPECT_FALSE(autofill_assistant::cup::ShouldSignRequests(
      autofill_assistant::RpcType::GET_TRIGGER_SCRIPTS));
}

TEST_F(CUPTest, ShouldVerifyGetActionsResponseWhenFeatureActivated) {
  InitCupFeatures(true, true);

  EXPECT_TRUE(autofill_assistant::cup::ShouldVerifyResponses(
      autofill_assistant::RpcType::GET_ACTIONS));
}

TEST_F(CUPTest, ShouldNotVerifyGetActionsResponseWhenFeatureNotActivated) {
  InitCupFeatures(true, false);

  EXPECT_FALSE(autofill_assistant::cup::ShouldVerifyResponses(
      autofill_assistant::RpcType::GET_ACTIONS));
}

TEST_F(CUPTest, ShouldNotVerifyGetActionsResponseWhenSigningNotActivated) {
  InitCupFeatures(false, true);

  EXPECT_FALSE(autofill_assistant::cup::ShouldVerifyResponses(
      autofill_assistant::RpcType::GET_ACTIONS));
}

TEST_F(CUPTest, ShouldNotVerifyNotGetActionsResponse) {
  InitCupFeatures(true, true);

  EXPECT_FALSE(autofill_assistant::cup::ShouldVerifyResponses(
      autofill_assistant::RpcType::GET_TRIGGER_SCRIPTS));
}

}  // namespace
