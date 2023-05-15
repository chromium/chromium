// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_util.h"

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/rand_util.h"
#include "base/test/values_test_util.h"
#include "components/feedback/feedback_report.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feedback_util {

// Note: This file is excluded from win build.
// See https://crbug.com/1119560.
class FeedbackUtilTest : public ::testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::ScopedTempDir temp_dir_;
};

TEST_F(FeedbackUtilTest, ReadEndOfFileEmpty) {
  std::string read_data("should be erased");

  base::FilePath file_path = temp_dir_.GetPath().Append("test_empty.txt");

  WriteFile(file_path, "", 0);

  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 10, &read_data));
  EXPECT_EQ(0u, read_data.length());
}

TEST_F(FeedbackUtilTest, ReadEndOfFileSmall) {
  const char kTestData[] = "0123456789";  // Length of 10
  std::string read_data;

  base::FilePath file_path = temp_dir_.GetPath().Append("test_small.txt");

  WriteFile(file_path, kTestData, strlen(kTestData));

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 15, &read_data));
  EXPECT_EQ(kTestData, read_data);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 10, &read_data));
  EXPECT_EQ(kTestData, read_data);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 2, &read_data));
  EXPECT_EQ("89", read_data);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 3, &read_data));
  EXPECT_EQ("789", read_data);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 5, &read_data));
  EXPECT_EQ("56789", read_data);
}

TEST_F(FeedbackUtilTest, ReadEndOfFileWithZeros) {
  const size_t test_size = 10;
  std::string test_data("abcd\0\0\0\0hi", test_size);
  std::string read_data;

  base::FilePath file_path = temp_dir_.GetPath().Append("test_zero.txt");

  WriteFile(file_path, test_data.data(), test_size);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 15, &read_data));
  EXPECT_EQ(test_data, read_data);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 10, &read_data));
  EXPECT_EQ(test_data, read_data);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 2, &read_data));
  EXPECT_EQ(test_data.substr(test_size - 2, 2), read_data);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 3, &read_data));
  EXPECT_EQ(test_data.substr(test_size - 3, 3), read_data);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 5, &read_data));
  EXPECT_EQ(test_data.substr(test_size - 5, 5), read_data);
}

TEST_F(FeedbackUtilTest, ReadEndOfFileMedium) {
  std::string test_data = base::RandBytesAsString(10000);  // 10KB data
  std::string read_data;

  const size_t test_size = test_data.length();

  base::FilePath file_path = temp_dir_.GetPath().Append("test_med.txt");

  WriteFile(file_path, test_data.data(), test_size);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 15000, &read_data));
  EXPECT_EQ(test_data, read_data);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 10000, &read_data));
  EXPECT_EQ(test_data, read_data);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 1000, &read_data));
  EXPECT_EQ(test_data.substr(test_size - 1000, 1000), read_data);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 300, &read_data));
  EXPECT_EQ(test_data.substr(test_size - 300, 300), read_data);

  read_data.clear();
  EXPECT_TRUE(feedback_util::ReadEndOfFile(file_path, 175, &read_data));
  EXPECT_EQ(test_data.substr(test_size - 175, 175), read_data);
}

TEST_F(FeedbackUtilTest, LogsToStringShouldSkipFeedbackUserCtlConsentKey) {
  FeedbackCommon::SystemLogsMap sys_info;
  sys_info[feedback::FeedbackReport::kFeedbackUserCtlConsentKey] = "true";

  std::string logs = feedback_util::LogsToString(sys_info);
  EXPECT_TRUE(logs.empty());

  // Now add a fake key expected to be in system_logs.
  sys_info["fake_key"] = "fake_value";
  logs = feedback_util::LogsToString(sys_info);
  EXPECT_EQ("fake_key=fake_value\n", logs);
}

TEST_F(FeedbackUtilTest, RemoveUrlsFromAutofillData) {
  base::Value::Dict autofill_data = base::test::ParseJsonDict(
      R"({
        "formStructures": [
          {
            "formSignature": "123",
            "sourceUrl": "https://www.example.com",
            "mainFrameUrl": "https://www.example.com"
          },
          {
            "formSignature": "456",
            "sourceUrl": "https://www.another-example.com",
            "mainFrameUrl": "https://www.another-example.com"
          }
        ]})");
  std::string autofill_data_str;
  base::JSONWriter::Write(autofill_data, &autofill_data_str);

  base::Value::List* form_structures = autofill_data.FindList("formStructures");
  ASSERT_TRUE(form_structures);
  for (base::Value& item : *form_structures) {
    auto& dict = item.GetDict();
    dict.Remove("sourceUrl");
    dict.Remove("mainFrameUrl");
  }

  std::string expected_autofill_data_str;
  base::JSONWriter::Write(autofill_data, &expected_autofill_data_str);

  feedback_util::RemoveUrlsFromAutofillData(autofill_data_str);
  EXPECT_EQ(autofill_data_str, expected_autofill_data_str);
}

}  // namespace feedback_util
