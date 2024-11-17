// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_common.h"

#include "base/functional/bind.h"
#include "build/chromeos_buildflags.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/proto/common.pb.h"
#include "components/feedback/proto/dom.pb.h"
#include "components/feedback/proto/extension.pb.h"
#include "components/feedback/proto/math.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kOne[] = "one";
constexpr char kTwo[] = "two";
constexpr char kThree[] = "three";
constexpr char kFour[] = "four";
#define TEN_LINES "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n"
constexpr char kLongLog[] = TEN_LINES TEN_LINES TEN_LINES TEN_LINES TEN_LINES;
constexpr char kLogsAttachmentName[] = "system_logs.zip";
constexpr int kTestProductId = 3490;

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr int kDefaultProductId = 208;  // ChromeOS default product ID.
#else
constexpr int kDefaultProductId = 237;  // Chrome default product ID.
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}  // namespace

class FeedbackCommonTest : public testing::Test {
 protected:
  FeedbackCommonTest() : feedback_(new FeedbackCommon()) {}

  ~FeedbackCommonTest() override = default;

  void CompressLogs() { feedback_->CompressLogs(); }

  bool FeedbackHasProductId() const { return feedback_->HasProductId(); }

  scoped_refptr<FeedbackCommon> feedback_;
  userfeedback::ExtensionSubmit report_;
};

TEST_F(FeedbackCommonTest, TestBasicData) {
  // Test that basic data can be set and propagates to the request.
  feedback_->set_category_tag(kOne);
  feedback_->set_description(kTwo);
  feedback_->set_page_url(kThree);
  feedback_->set_user_email(kFour);
  EXPECT_FALSE(FeedbackHasProductId());
  feedback_->set_product_id(kTestProductId);
  EXPECT_TRUE(FeedbackHasProductId());
  feedback_->PrepareReport(&report_);

  EXPECT_EQ(kOne, report_.bucket());
  EXPECT_EQ(kTwo, report_.common_data().description());
  EXPECT_EQ(kThree, report_.web_data().url());
  EXPECT_EQ(kFour, report_.common_data().user_email());
  EXPECT_EQ(kTestProductId, report_.product_id());
}

// If an feedback requester doesn't set the product ID, the report will be sent
// with the default product ID for Chrome/ChromeOS depending on the platform.
TEST_F(FeedbackCommonTest, TestDefaultProductId) {
  EXPECT_FALSE(FeedbackHasProductId());
  feedback_->PrepareReport(&report_);
  EXPECT_EQ(kDefaultProductId, report_.product_id());
}

TEST_F(FeedbackCommonTest, TestAddLogs) {
  feedback_->AddLog(kOne, kTwo);
  feedback_->AddLog(kThree, kFour);

  EXPECT_EQ(2U, feedback_->sys_info()->size());
}

TEST_F(FeedbackCommonTest, TestCompressionThreshold) {
  // Add a large and small log, verify that only the small log gets
  // included in the report.
  feedback_->AddLog(kOne, kTwo);
  feedback_->AddLog(kThree, kLongLog);
  feedback_->PrepareReport(&report_);

  EXPECT_EQ(1, report_.web_data().product_specific_data_size());
  EXPECT_EQ(kOne, report_.web_data().product_specific_data(0).key());
}

TEST_F(FeedbackCommonTest, TestCompression) {
  // Add a large and small log, verify that an attachment has been
  // added with the right name.
  feedback_->AddLog(kOne, kTwo);
  feedback_->AddLog(kThree, kLongLog);
  CompressLogs();
  feedback_->PrepareReport(&report_);

  EXPECT_EQ(1, report_.product_specific_binary_data_size());
  EXPECT_EQ(kLogsAttachmentName,
            report_.product_specific_binary_data(0).name());
}

TEST_F(FeedbackCommonTest, TestAllCrashIdsRemoval) {
  feedback_->AddLog(feedback::FeedbackReport::kAllCrashReportIdsKey, kOne);
  feedback_->set_user_email("nobody@example.com");
  feedback_->PrepareReport(&report_);

  EXPECT_EQ(0, report_.web_data().product_specific_data_size());
}

TEST_F(FeedbackCommonTest, TestAllCrashIdsRetention) {
  feedback_->AddLog(feedback::FeedbackReport::kAllCrashReportIdsKey, kOne);
  feedback_->set_user_email("nobody@google.com");
  feedback_->PrepareReport(&report_);

  EXPECT_EQ(1, report_.web_data().product_specific_data_size());
}

TEST_F(FeedbackCommonTest, IncludeInSystemLogs) {
  bool google_email = true;
  EXPECT_TRUE(FeedbackCommon::IncludeInSystemLogs(kOne, google_email));
  EXPECT_TRUE(FeedbackCommon::IncludeInSystemLogs(
      feedback::FeedbackReport::kAllCrashReportIdsKey, google_email));

  google_email = false;
  EXPECT_TRUE(FeedbackCommon::IncludeInSystemLogs(kOne, google_email));
  EXPECT_FALSE(FeedbackCommon::IncludeInSystemLogs(
      feedback::FeedbackReport::kAllCrashReportIdsKey, google_email));
}

TEST_F(FeedbackCommonTest, IsOffensiveOrUnsafe) {
  feedback_->set_is_offensive_or_unsafe(true);
  feedback_->PrepareReport(&report_);

  EXPECT_EQ(1, report_.web_data().product_specific_data_size());
  EXPECT_EQ("is_offensive_or_unsafe",
            report_.web_data().product_specific_data(0).key());
  EXPECT_EQ("true", report_.web_data().product_specific_data(0).value());
}

TEST_F(FeedbackCommonTest, AiMetadata) {
  feedback_->set_ai_metadata("{\"log_id\":\"TEST_ID\"}");
  feedback_->PrepareReport(&report_);

  EXPECT_EQ(1, report_.web_data().product_specific_data_size());
  EXPECT_EQ("log_id", report_.web_data().product_specific_data(0).key());
  EXPECT_EQ("TEST_ID", report_.web_data().product_specific_data(0).value());
}
