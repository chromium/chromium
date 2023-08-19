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

using ChromiumPrintMetadata =
    enterprise_connectors::ContentMetaData::PrintMetadata;
using SdkPrintMetadata = content_analysis::sdk::ContentMetaData::PrintMetadata;

TEST(EnterpriseConnectorsProtoTest, PrintTypeEnum) {
  EXPECT_EQ(ChromiumPrintMetadata::PrinterType_ARRAYSIZE, 3);
  EXPECT_EQ(ChromiumPrintMetadata::PrinterType_ARRAYSIZE,
            SdkPrintMetadata::PrinterType_ARRAYSIZE);

  EXPECT_EQ((int)ChromiumPrintMetadata::UNKNOWN,
            (int)SdkPrintMetadata::UNKNOWN);
  EXPECT_EQ((int)ChromiumPrintMetadata::CLOUD, (int)SdkPrintMetadata::CLOUD);
  EXPECT_EQ((int)ChromiumPrintMetadata::LOCAL, (int)SdkPrintMetadata::LOCAL);
}

using ChromiumReason = enterprise_connectors::ContentAnalysisRequest;
using SdkReason = content_analysis::sdk::ContentAnalysisRequest;

TEST(EnterpriseConnectorsProtoTest, ReasonEnum) {
  EXPECT_EQ(ChromiumReason::Reason_ARRAYSIZE, 8);
  EXPECT_EQ(ChromiumReason::Reason_ARRAYSIZE, SdkReason::Reason_ARRAYSIZE);

  EXPECT_EQ((int)ChromiumReason::UNKNOWN, (int)SdkReason::UNKNOWN);
  EXPECT_EQ((int)ChromiumReason::CLIPBOARD_PASTE,
            (int)SdkReason::CLIPBOARD_PASTE);
  EXPECT_EQ((int)ChromiumReason::DRAG_AND_DROP, (int)SdkReason::DRAG_AND_DROP);
  EXPECT_EQ((int)ChromiumReason::FILE_PICKER_DIALOG,
            (int)SdkReason::FILE_PICKER_DIALOG);
  EXPECT_EQ((int)ChromiumReason::PRINT_PREVIEW_PRINT,
            (int)SdkReason::PRINT_PREVIEW_PRINT);
  EXPECT_EQ((int)ChromiumReason::SYSTEM_DIALOG_PRINT,
            (int)SdkReason::SYSTEM_DIALOG_PRINT);
  EXPECT_EQ((int)ChromiumReason::NORMAL_DOWNLOAD,
            (int)SdkReason::NORMAL_DOWNLOAD);
  EXPECT_EQ((int)ChromiumReason::SAVE_AS_DOWNLOAD,
            (int)SdkReason::SAVE_AS_DOWNLOAD);
}

}  // namespace enterprise_connectors
