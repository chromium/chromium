// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #include "third_party/content_analysis_sdk/src/proto/content_analysis/sdk/

#include "components/enterprise/common/proto/connectors.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/content_analysis_sdk/src/proto/content_analysis/sdk/analysis.pb.h"

namespace enterprise_connectors {

TEST(EnterpriseConnectorsProtoTest, AnalysisConnectorEnum) {
  EXPECT_EQ(enterprise_connectors::AnalysisConnector_ARRAYSIZE, 6);
  EXPECT_EQ(enterprise_connectors::AnalysisConnector_ARRAYSIZE,
            content_analysis::sdk::AnalysisConnector_ARRAYSIZE);

  EXPECT_EQ((int)enterprise_connectors::ANALYSIS_CONNECTOR_UNSPECIFIED,
            (int)content_analysis::sdk::ANALYSIS_CONNECTOR_UNSPECIFIED);
  EXPECT_EQ((int)enterprise_connectors::FILE_DOWNLOADED,
            (int)content_analysis::sdk::FILE_DOWNLOADED);
  EXPECT_EQ((int)enterprise_connectors::FILE_ATTACHED,
            (int)content_analysis::sdk::FILE_ATTACHED);
  EXPECT_EQ((int)enterprise_connectors::BULK_DATA_ENTRY,
            (int)content_analysis::sdk::BULK_DATA_ENTRY);
  EXPECT_EQ((int)enterprise_connectors::PRINT,
            (int)content_analysis::sdk::PRINT);
  EXPECT_EQ((int)enterprise_connectors::FILE_TRANSFER,
            (int)content_analysis::sdk::FILE_TRANSFER);
}

using ChromiumResult = enterprise_connectors::ContentAnalysisResponse::Result;
using SdkResult = content_analysis::sdk::ContentAnalysisResponse::Result;

TEST(EnterpriseConnectorsProtoTest, StatusEnum) {
  EXPECT_EQ(ChromiumResult::Status_ARRAYSIZE, 3);
  EXPECT_EQ(ChromiumResult::Status_ARRAYSIZE, SdkResult::Status_ARRAYSIZE);

  EXPECT_EQ((int)ChromiumResult::STATUS_UNKNOWN,
            (int)SdkResult::STATUS_UNKNOWN);
  EXPECT_EQ((int)ChromiumResult::SUCCESS, (int)SdkResult::SUCCESS);
  EXPECT_EQ((int)ChromiumResult::FAILURE, (int)SdkResult::FAILURE);
}

using ChromiumRule = ChromiumResult::TriggeredRule;
using SdkRule = SdkResult::TriggeredRule;

TEST(EnterpriseConnectorsProtoTest, TriggeredRuleActionEnum) {
  EXPECT_EQ(ChromiumRule::Action_ARRAYSIZE, 4);
  EXPECT_EQ(ChromiumRule::Action_ARRAYSIZE, SdkRule::Action_ARRAYSIZE);

  EXPECT_EQ((int)ChromiumRule::ACTION_UNSPECIFIED,
            (int)SdkRule::ACTION_UNSPECIFIED);
  EXPECT_EQ((int)ChromiumRule::REPORT_ONLY, (int)SdkRule::REPORT_ONLY);
  EXPECT_EQ((int)ChromiumRule::WARN, (int)SdkRule::WARN);
  EXPECT_EQ((int)ChromiumRule::BLOCK, (int)SdkRule::BLOCK);
}

}  // namespace enterprise_connectors
