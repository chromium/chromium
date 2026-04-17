// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/deep_scanning_utils.h"

#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/test/scoped_feature_list.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/analysis_test_utils.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/deep_scanning_utils.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/test_connectors_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "ui/gfx/range/range.h"

namespace enterprise_connectors {

namespace {

struct CustomMessageTestCase {
  TriggeredRule::Action action;
  std::string message;
};

constexpr char kTestUrl[] = "http://example.com/";
constexpr char kTestInvalidUrl[] = "example.com";
constexpr char kTestMessage[] = "test";
constexpr char16_t kU16TestMessage[] = u"test";
constexpr char kTestMessage2[] = "test2";
constexpr char kGoogleServiceProvider[] = R"(
{
  "service_provider": "google",
  "enable": [
    {
      "url_list": ["*"],
      "tags": ["dlp"]
    }
  ],
  "block_large_files": 1
})";
constexpr char kTestEscapedHtmlMessage[] = "&lt;&gt;&amp;&quot;&#39;";
constexpr char16_t kTestUnescapedHtmlMessage[] = u"<>&\"'";
// Offset to first placeholder index for
// IDS_DEEP_SCANNING_DIALOG_CUSTOM_MESSAGE.
constexpr size_t kRuleMessageOffset = 26;
constexpr char kTestLinkedMessage[] = "Learn More";
constexpr char16_t kU16TestLinkedMessage[] = u"Learn More";
constexpr char kProfileDMToken[] = "profile_dm_token";
constexpr char kMachineDMToken[] = "machine_dm_token";

ContentAnalysisResponse CreateContentAnalysisResponse(
    const std::vector<CustomMessageTestCase>& triggered_rules,
    const std::string& url) {
  ContentAnalysisResponse response;
  auto* result = response.add_results();
  result->set_tag("dlp");
  result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);

  for (const auto& triggered_rule : triggered_rules) {
    auto* rule = result->add_triggered_rules();
    rule->set_action(triggered_rule.action);
    if (!triggered_rule.message.empty()) {
      ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage
          custom_message;
      auto* custom_segment = custom_message.add_message_segments();
      custom_segment->set_text(triggered_rule.message);
      auto* custom_linked_segment = custom_message.add_message_segments();
      custom_linked_segment->set_text(kTestLinkedMessage);
      custom_linked_segment->set_link(url);
      *rule->mutable_custom_rule_message() = custom_message;
    }
  }
  return response;
}

class BaseTest : public testing::Test {
 public:
  BaseTest() {
    // Settings can't be returned if no DM token exists.
    connectors_service_ = std::make_unique<TestConnectorsService>(&prefs_);
    connectors_service_->set_connectors_enabled(true);
    connectors_service_->set_profile_dm_token(kProfileDMToken);
    connectors_service_->set_machine_dm_token(kMachineDMToken);
  }

  void TearDown() override { connectors_service_ = nullptr; }

  void EnableFeatures() { scoped_feature_list_.Reset(); }

  AnalysisSettings settings(TestConnectorsService* service) {
    std::optional<AnalysisSettings> settings =
        service->GetAnalysisSettings(GURL(kTestUrl), FILE_DOWNLOADED);
    EXPECT_TRUE(settings.has_value());
    return std::move(settings.value());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<TestConnectorsService> connectors_service_;
};

}  // namespace

class EnterpriseConnectorsResultShouldAllowDataUseTest
    : public BaseTest,
      public testing::WithParamInterface<bool> {
 public:
  EnterpriseConnectorsResultShouldAllowDataUseTest() = default;

  void SetUp() override {
    BaseTest::SetUp();
    EnableFeatures();
  }

  bool allowed() const { return !GetParam(); }
  std::string bool_setting() const { return base::ToString(GetParam()); }
  const char* default_action_setting() const {
    return GetParam() ? "block" : "allow";
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         EnterpriseConnectorsResultShouldAllowDataUseTest,
                         testing::Bool());

// Tests request result should not be allowed a data use if file is too large.
TEST_P(EnterpriseConnectorsResultShouldAllowDataUseTest, BlockLargeFile) {
  auto pref = base::StringPrintf(R"(
    {
      "service_provider": "google",
      "enable": [{"url_list": ["*"], "tags": ["dlp"]}],
      "block_large_files": %s
    })",
                                 bool_setting());
  test::SetAnalysisConnectorsPrefs(connectors_service_->GetPrefs(),
                                   FILE_DOWNLOADED, {pref}, true);

  EXPECT_EQ(allowed(),
            ResultShouldAllowDataUse(settings(connectors_service_.get()),
                                     ScanRequestUploadResult::kFileTooLarge));
}

// Tests request result should not be allowed a data use if file is password
// protected.
TEST_P(EnterpriseConnectorsResultShouldAllowDataUseTest,
       BlockPasswordProtected) {
  auto pref = base::StringPrintf(R"(
    {
      "service_provider": "google",
      "enable": [{"url_list": ["*"], "tags": ["dlp"]}],
      "block_password_protected": %s
    })",
                                 bool_setting());
  test::SetAnalysisConnectorsPrefs(connectors_service_->GetPrefs(),
                                   FILE_DOWNLOADED, {pref}, true);

  EXPECT_EQ(allowed(),
            ResultShouldAllowDataUse(settings(connectors_service_.get()),
                                     ScanRequestUploadResult::kFileEncrypted));
}

// Tests request result should not be allowed a data use if file failed to
// upload.
TEST_P(EnterpriseConnectorsResultShouldAllowDataUseTest, BlockUploadFailure) {
  auto pref = base::StringPrintf(R"(
    {
      "service_provider": "google",
      "enable": [{"url_list": ["*"], "tags": ["dlp"]}],
      "default_action": "%s"
    })",
                                 default_action_setting());
  test::SetAnalysisConnectorsPrefs(connectors_service_->GetPrefs(),
                                   FILE_DOWNLOADED, {pref}, true);

  EXPECT_EQ(allowed(),
            ResultShouldAllowDataUse(settings(connectors_service_.get()),
                                     ScanRequestUploadResult::kUploadFailure));
}

// Tests request result should not be allowed a data use if user cancelled.
TEST_P(EnterpriseConnectorsResultShouldAllowDataUseTest, BlockUserCancelled) {
  EXPECT_FALSE(ResultIsFailClosed(ScanRequestUploadResult::kUserCancelled));
  EXPECT_EQ("UserCancelled",
            BinaryUploadServiceResultToString(
                ScanRequestUploadResult::kUserCancelled, false));
  auto pref = base::StringPrintf(R"(
    {
      "service_provider": "google",
      "enable": [{"url_list": ["*"], "tags": ["dlp"]}],
      "default_action": "%s"
    })",
                                 default_action_setting());
  test::SetAnalysisConnectorsPrefs(connectors_service_->GetPrefs(),
                                   FILE_DOWNLOADED, {pref}, true);

  EXPECT_FALSE(
      ResultShouldAllowDataUse(settings(connectors_service_.get()),
                               ScanRequestUploadResult::kUserCancelled));
}

class ContentAnalysisResponseCustomMessageTest
    : public BaseTest,
      public testing::WithParamInterface<
          std::tuple<std::vector<CustomMessageTestCase>, std::u16string>> {
 public:
  ContentAnalysisResponseCustomMessageTest() = default;

  void SetUp() override {
    BaseTest::SetUp();
    EnableFeatures();
  }

  std::vector<CustomMessageTestCase> triggered_rules() const {
    return std::get<0>(GetParam());
  }
  std::u16string expected_message() const { return std::get<1>(GetParam()); }
};

// Tests that the result message is correct when the url is valid.
TEST_P(ContentAnalysisResponseCustomMessageTest, ValidUrlCustomMessage) {
  test::SetAnalysisConnectorsPrefs(connectors_service_->GetPrefs(),
                                   FILE_DOWNLOADED, {kGoogleServiceProvider},
                                   true);

  ContentAnalysisResponse response =
      CreateContentAnalysisResponse(triggered_rules(), kTestUrl);
  RequestHandlerResult result = CalculateRequestHandlerResult(
      settings(connectors_service_.get()), ScanRequestUploadResult::kSuccess,
      response);
  std::u16string custom_message =
      GetCustomRuleString(result.custom_rule_message);
  std::vector<std::pair<gfx::Range, GURL>> custom_ranges =
      GetCustomRuleStyles(result.custom_rule_message, kRuleMessageOffset);

  EXPECT_EQ(custom_message,
            custom_message.empty()
                ? std::u16string{}
                : base::StrCat({expected_message(), kU16TestLinkedMessage}));

  if (custom_message.empty()) {
    EXPECT_TRUE(custom_ranges.empty());
  } else {
    EXPECT_EQ(1u, custom_ranges.size());
    EXPECT_EQ(strlen(kTestLinkedMessage),
              custom_ranges.begin()->first.length());
    EXPECT_EQ(custom_ranges.begin()->first.start(),
              expected_message().length() + kRuleMessageOffset);
  }
}

// Tests that the result message is correct when the url is invalid.
TEST_P(ContentAnalysisResponseCustomMessageTest, InvalidUrlCustomMessage) {
  test::SetAnalysisConnectorsPrefs(connectors_service_->GetPrefs(),
                                   FILE_DOWNLOADED, {kGoogleServiceProvider},
                                   true);

  ContentAnalysisResponse response =
      CreateContentAnalysisResponse(triggered_rules(), kTestInvalidUrl);
  RequestHandlerResult result = CalculateRequestHandlerResult(
      settings(connectors_service_.get()), ScanRequestUploadResult::kSuccess,
      response);
  std::u16string custom_message =
      GetCustomRuleString(result.custom_rule_message);
  std::vector<std::pair<gfx::Range, GURL>> custom_ranges =
      GetCustomRuleStyles(result.custom_rule_message, kRuleMessageOffset);

  EXPECT_EQ(custom_message,
            custom_message.empty()
                ? std::u16string{}
                : base::StrCat({expected_message(), kU16TestLinkedMessage}));
  EXPECT_TRUE(custom_ranges.empty());
}

// Tests that the result message is correct when it is a dangerous download.
TEST_P(ContentAnalysisResponseCustomMessageTest, DownloadsItemCustomMessage) {
  ContentAnalysisResponse response =
      CreateContentAnalysisResponse(triggered_rules(), kTestUrl);
  download::DownloadDangerType danger_type;
  TriggeredRule::Action action = GetHighestPrecedenceAction(response, nullptr);
  if (action == TriggeredRule::WARN) {
    danger_type = download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING;
  } else {
    danger_type = download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK;
  }

  // Create download item
  testing::NiceMock<download::MockDownloadItem> item;
  enterprise_connectors::FileMetadata file_metadata(
      "examplename", "12345678", "fake/mimetype", 1234, response);
  auto scan_result = std::make_unique<ScanResult>(std::move(file_metadata));
  item.SetUserData(ScanResult::kKey, std::move(scan_result));

  auto custom_rule_message = GetDownloadsCustomRuleMessage(&item, danger_type);
  if (custom_rule_message.has_value()) {
    EXPECT_EQ(GetCustomRuleString(custom_rule_message.value()),
              base::StrCat({expected_message(), kU16TestLinkedMessage}));
  } else {
    EXPECT_EQ(std::u16string{}, expected_message());
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ContentAnalysisResponseCustomMessageTest,
    testing::Values(
        std::make_tuple(std::vector<CustomMessageTestCase>(),
                        /*expected_message=*/std::u16string{}),
        std::make_tuple(
            std::vector<CustomMessageTestCase>{
                {.action = TriggeredRule::WARN, .message = ""}},
            /*expected_message=*/std::u16string{}),
        std::make_tuple(
            std::vector<CustomMessageTestCase>{
                {.action = TriggeredRule::WARN, .message = kTestMessage}},
            /*expected_message=*/kU16TestMessage),
        std::make_tuple(
            std::vector<CustomMessageTestCase>{
                {.action = TriggeredRule::BLOCK, .message = ""},
                {.action = TriggeredRule::WARN, .message = kTestMessage}},
            /*expected_message=*/std::u16string{}),
        std::make_tuple(
            std::vector<CustomMessageTestCase>{
                {.action = TriggeredRule::BLOCK, .message = ""},
                {.action = TriggeredRule::BLOCK, .message = kTestMessage}},
            /*expected_message=*/kU16TestMessage),
        std::make_tuple(
            std::vector<CustomMessageTestCase>{
                {.action = TriggeredRule::BLOCK, .message = kTestMessage},
                {.action = TriggeredRule::WARN, .message = kTestMessage2}},
            /*expected_message=*/kU16TestMessage),
        std::make_tuple(
            std::vector<CustomMessageTestCase>{
                {.action = TriggeredRule::BLOCK,
                 .message = kTestEscapedHtmlMessage}},
            /*expected_message=*/kTestUnescapedHtmlMessage)));

TEST(DeepScanningUtilsTest, BinaryUploadServiceResultToString) {
  EXPECT_EQ("UserCancelled",
            enterprise_connectors::BinaryUploadServiceResultToString(
                enterprise_connectors::ScanRequestUploadResult::kUserCancelled,
                false));
}

class CalculateRequestHandlerResultTest : public BaseTest {};

TEST_F(CalculateRequestHandlerResultTest, UserCancelled) {
  test::SetAnalysisConnectorsPrefs(connectors_service_->GetPrefs(),
                                   FILE_DOWNLOADED, {kGoogleServiceProvider},
                                   true);

  ContentAnalysisResponse response;
  auto* result = response.add_results();
  result->set_status(ContentAnalysisResponse::Result::SUCCESS);

  RequestHandlerResult handler_result = CalculateRequestHandlerResult(
      settings(connectors_service_.get()),
      ScanRequestUploadResult::kUserCancelled, response);

  EXPECT_FALSE(handler_result.complies);
  EXPECT_EQ(FinalContentAnalysisResult::CANCELLED, handler_result.final_result);
  EXPECT_EQ("Cancelled",
            FinalContentAnalysisResultToString(handler_result.final_result));
}

}  // namespace enterprise_connectors
