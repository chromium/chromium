// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/consent_kit/consent_kit_result_parser.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace {

constexpr identity_consent::ConsentFlowId kFlowId =
    identity_consent::ConsentFlowId::CHOICEFLOW_PCONTEXT_WORKSPACE_AIM;

identity_consent::PrivacyFlowResult BuildResult(
    identity_consent::ConsentSettingId setting_id,
    identity_consent::Decision decision) {
  identity_consent::PrivacyFlowResult result;
  result.set_flow_id(kFlowId);
  auto* consent_decision = result.add_decision();
  consent_decision->set_ftc_consent_setting_id(setting_id);
  consent_decision->set_decision(decision);
  return result;
}

identity_consent::PrivacyFlowResult BuildResult(
    identity_consent::Decision decision) {
  return BuildResult(identity_consent::ConsentSettingId::
                         PERSONAL_CONTEXT_SEARCH_USING_WORKSPACE,
                     decision);
}

TEST(ConsentKitResultParserTest, GrantsOnConsent) {
  EXPECT_TRUE(HasGrantedDriveConsent(
      BuildResult(identity_consent::Decision::DECISION_CONSENT)));
}

TEST(ConsentKitResultParserTest, GrantsOnKeepConsent) {
  EXPECT_TRUE(HasGrantedDriveConsent(
      BuildResult(identity_consent::Decision::DECISION_KEEP_CONSENT)));
}

TEST(ConsentKitResultParserTest, RejectsDoNotConsent) {
  EXPECT_FALSE(HasGrantedDriveConsent(
      BuildResult(identity_consent::Decision::DECISION_DO_NOT_CONSENT)));
}

TEST(ConsentKitResultParserTest, RejectsUnrelatedSetting) {
  EXPECT_FALSE(HasGrantedDriveConsent(
      BuildResult(identity_consent::ConsentSettingId::SETTING_UNSPECIFIED,
                  identity_consent::Decision::DECISION_CONSENT)));
}

TEST(ConsentKitResultParserTest, MatchesExpectedFlowId) {
  identity_consent::PrivacyFlowResult result =
      BuildResult(identity_consent::Decision::DECISION_CONSENT);

  EXPECT_TRUE(IsExpectedConsentFlow(result, kFlowId));
  EXPECT_FALSE(IsExpectedConsentFlow(result, kFlowId + 1));
}

TEST(ConsentKitResultParserTest, RejectsMissingFlowId) {
  EXPECT_FALSE(
      IsExpectedConsentFlow(identity_consent::PrivacyFlowResult(), kFlowId));
}

}  // namespace
}  // namespace drive
