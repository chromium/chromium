// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/upload_list/text_log_upload_list.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestUploadTime[] = "1234567890";
const char kTestUploadId[] = "0123456789abcdef";
const char kTestLocalID[] = "fedcba9876543210";
const char kTestCaptureTime[] = "2345678901";

class TextLogUploadListTest : public testing::Test {
 public:
  TextLogUploadListTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

 protected:
  void WriteUploadLog(const std::string& log_data) {
    ASSERT_GT(base::WriteFile(log_path(), log_data.c_str(),
                              static_cast<int>(log_data.size())),
              0);
  }

  base::FilePath log_path() {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("uploads.log"));
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(TextLogUploadListTest);
};

// These tests test that UploadList can parse a vector of log entry strings of
// various formats to a vector of UploadInfo objects. See the UploadList
// declaration for a description of the log entry string formats.

// Test log entry string with upload time and upload ID.
// This is the format that crash reports are stored in.
TEST_F(TextLogUploadListTest, ParseUploadTimeUploadId) {
  std::string test_entry = kTestUploadTime;
  test_entry += ",";
  test_entry.append(kTestUploadId);
  WriteUploadLog(test_entry);

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();

  std::vector<UploadList::UploadInfo> uploads;
  upload_list->GetUploads(999, &uploads);

  EXPECT_EQ(1u, uploads.size());
  double time_double = uploads[0].upload_time.ToDoubleT();
  EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
  EXPECT_STREQ(kTestUploadId, uploads[0].upload_id.c_str());
  EXPECT_STREQ("", uploads[0].local_id.c_str());
  time_double = uploads[0].capture_time.ToDoubleT();
  EXPECT_STREQ("0", base::NumberToString(time_double).c_str());
}

// Test log entry string with upload time, upload ID and local ID.
// This is the old format that WebRTC logs were stored in.
TEST_F(TextLogUploadListTest, ParseUploadTimeUploadIdLocalId) {
  std::string test_entry = kTestUploadTime;
  test_entry += ",";
  test_entry.append(kTestUploadId);
  test_entry += ",";
  test_entry.append(kTestLocalID);
  WriteUploadLog(test_entry);

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();

  std::vector<UploadList::UploadInfo> uploads;
  upload_list->GetUploads(999, &uploads);

  EXPECT_EQ(1u, uploads.size());
  double time_double = uploads[0].upload_time.ToDoubleT();
  EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
  EXPECT_STREQ(kTestUploadId, uploads[0].upload_id.c_str());
  EXPECT_STREQ(kTestLocalID, uploads[0].local_id.c_str());
  time_double = uploads[0].capture_time.ToDoubleT();
  EXPECT_STREQ("0", base::NumberToString(time_double).c_str());
}

// Test log entry string with upload time, upload ID and capture time.
// This is the format that WebRTC logs that only have been uploaded only are
// stored in.
TEST_F(TextLogUploadListTest, ParseUploadTimeUploadIdCaptureTime) {
  std::string test_entry = kTestUploadTime;
  test_entry += ",";
  test_entry.append(kTestUploadId);
  test_entry += ",,";
  test_entry.append(kTestCaptureTime);
  WriteUploadLog(test_entry);

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();

  std::vector<UploadList::UploadInfo> uploads;
  upload_list->GetUploads(999, &uploads);

  EXPECT_EQ(1u, uploads.size());
  double time_double = uploads[0].upload_time.ToDoubleT();
  EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
  EXPECT_STREQ(kTestUploadId, uploads[0].upload_id.c_str());
  EXPECT_STREQ("", uploads[0].local_id.c_str());
  time_double = uploads[0].capture_time.ToDoubleT();
  EXPECT_STREQ(kTestCaptureTime, base::NumberToString(time_double).c_str());
}

// Test log entry string with local ID and capture time.
// This is the format that WebRTC logs that only are stored locally are stored
// in.
TEST_F(TextLogUploadListTest, ParseLocalIdCaptureTime) {
  std::string test_entry = ",,";
  test_entry.append(kTestLocalID);
  test_entry += ",";
  test_entry.append(kTestCaptureTime);
  WriteUploadLog(test_entry);

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();

  std::vector<UploadList::UploadInfo> uploads;
  upload_list->GetUploads(999, &uploads);

  EXPECT_EQ(1u, uploads.size());
  double time_double = uploads[0].upload_time.ToDoubleT();
  EXPECT_STREQ("0", base::NumberToString(time_double).c_str());
  EXPECT_STREQ("", uploads[0].upload_id.c_str());
  EXPECT_STREQ(kTestLocalID, uploads[0].local_id.c_str());
  time_double = uploads[0].capture_time.ToDoubleT();
  EXPECT_STREQ(kTestCaptureTime, base::NumberToString(time_double).c_str());
}

// Test log entry string with upload time, upload ID, local ID and capture
// time.
// This is the format that WebRTC logs that are stored locally and have been
// uploaded are stored in.
TEST_F(TextLogUploadListTest, ParseUploadTimeUploadIdLocalIdCaptureTime) {
  std::string test_entry = kTestUploadTime;
  test_entry += ",";
  test_entry.append(kTestUploadId);
  test_entry += ",";
  test_entry.append(kTestLocalID);
  test_entry += ",";
  test_entry.append(kTestCaptureTime);
  WriteUploadLog(test_entry);

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();

  std::vector<UploadList::UploadInfo> uploads;
  upload_list->GetUploads(999, &uploads);

  EXPECT_EQ(1u, uploads.size());
  double time_double = uploads[0].upload_time.ToDoubleT();
  EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
  EXPECT_STREQ(kTestUploadId, uploads[0].upload_id.c_str());
  EXPECT_STREQ(kTestLocalID, uploads[0].local_id.c_str());
  time_double = uploads[0].capture_time.ToDoubleT();
  EXPECT_STREQ(kTestCaptureTime, base::NumberToString(time_double).c_str());
}

TEST_F(TextLogUploadListTest, ParseMultipleEntries) {
  std::string test_entry;
  for (int i = 1; i <= 4; ++i) {
    test_entry.append(kTestUploadTime);
    test_entry += ",";
    test_entry.append(kTestUploadId);
    test_entry += ",";
    test_entry.append(base::NumberToString(i));
    test_entry += ",";
    test_entry.append(kTestCaptureTime);
    test_entry += "\n";
  }
  WriteUploadLog(test_entry);

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();

  std::vector<UploadList::UploadInfo> uploads;
  upload_list->GetUploads(999, &uploads);

  EXPECT_EQ(4u, uploads.size());
  // The entries order should be reversed during the parsing.
  for (size_t i = 0; i < uploads.size(); ++i) {
    double time_double = uploads[i].upload_time.ToDoubleT();
    EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
    EXPECT_STREQ(kTestUploadId, uploads[i].upload_id.c_str());
    EXPECT_EQ(base::NumberToString(uploads.size() - i), uploads[i].local_id);
    time_double = uploads[i].capture_time.ToDoubleT();
    EXPECT_STREQ(kTestCaptureTime, base::NumberToString(time_double).c_str());
  }
}

TEST_F(TextLogUploadListTest, ParseWithState) {
  std::string test_entry;
  for (int i = 1; i <= 4; ++i) {
    test_entry.append(kTestUploadTime);
    test_entry += ",";
    test_entry.append(kTestUploadId);
    test_entry += ",";
    test_entry.append(kTestLocalID);
    test_entry += ",";
    test_entry.append(kTestCaptureTime);
    test_entry += ",";
    test_entry.append(base::NumberToString(
        static_cast<int>(UploadList::UploadInfo::State::Uploaded)));
    test_entry += "\n";
  }
  WriteUploadLog(test_entry);

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();

  std::vector<UploadList::UploadInfo> uploads;
  upload_list->GetUploads(999, &uploads);

  EXPECT_EQ(4u, uploads.size());
  for (size_t i = 0; i < uploads.size(); ++i) {
    double time_double = uploads[i].upload_time.ToDoubleT();
    EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
    EXPECT_STREQ(kTestUploadId, uploads[i].upload_id.c_str());
    EXPECT_STREQ(kTestLocalID, uploads[i].local_id.c_str());
    time_double = uploads[i].capture_time.ToDoubleT();
    EXPECT_STREQ(kTestCaptureTime, base::NumberToString(time_double).c_str());
    EXPECT_EQ(UploadList::UploadInfo::State::Uploaded, uploads[i].state);
  }
}

TEST_F(TextLogUploadListTest, ClearUsingUploadTime) {
  constexpr time_t kTestTime = 1234u;
  constexpr char kOtherEntry[] = "4567,def\n";
  std::string test_entry = base::NumberToString(kTestTime);
  test_entry.append(",abc\n");
  test_entry.append(kOtherEntry);
  WriteUploadLog(test_entry);

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Clear(base::Time::FromTimeT(kTestTime),
                     base::Time::FromTimeT(kTestTime + 1),
                     run_loop.QuitClosure());
  run_loop.Run();

  std::string contents;
  base::ReadFileToString(log_path(), &contents);
  EXPECT_EQ(kOtherEntry, contents);
}

TEST_F(TextLogUploadListTest, ClearUsingCaptureTime) {
  constexpr time_t kTestTime = 1234u;
  constexpr char kOtherEntry[] = "4567,def,def,7890\n";
  std::string test_entry = kOtherEntry;
  test_entry.append("4567,abc,abc,");
  test_entry.append(base::NumberToString(kTestTime));
  test_entry.append("\n");
  WriteUploadLog(test_entry);

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Clear(base::Time::FromTimeT(kTestTime),
                     base::Time::FromTimeT(kTestTime + 1),
                     run_loop.QuitClosure());
  run_loop.Run();

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(log_path(), &contents));
  EXPECT_EQ(kOtherEntry, contents);
}

TEST_F(TextLogUploadListTest, ClearingAllDataDeletesFile) {
  constexpr time_t kTestTime = 1234u;
  std::string test_entry = base::NumberToString(kTestTime);
  test_entry.append(",abc\n");
  WriteUploadLog(test_entry);

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Clear(base::Time::FromTimeT(kTestTime),
                     base::Time::FromTimeT(kTestTime + 1),
                     run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(base::PathExists(log_path()));
}

// https://crbug.com/597384
TEST_F(TextLogUploadListTest, SimultaneousAccess) {
  std::string test_entry = kTestUploadTime;
  test_entry += ",";
  test_entry.append(kTestUploadId);
  test_entry += ",";
  test_entry.append(kTestLocalID);
  test_entry += ",";
  test_entry.append(kTestCaptureTime);
  WriteUploadLog(test_entry);

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  // Queue up a bunch of loads, waiting only for the first one to complete.
  base::RunLoop run_loop;
  upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();

  for (int i = 1; i <= 20; ++i) {
    upload_list->Load(base::OnceClosure());
  }

  // Read the list a few times to try and race one of the loads above.
  for (int i = 1; i <= 4; ++i) {
    std::vector<UploadList::UploadInfo> uploads;
    upload_list->GetUploads(999, &uploads);

    EXPECT_EQ(1u, uploads.size());
    double time_double = uploads[0].upload_time.ToDoubleT();
    EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
    EXPECT_STREQ(kTestUploadId, uploads[0].upload_id.c_str());
    EXPECT_STREQ(kTestLocalID, uploads[0].local_id.c_str());
    time_double = uploads[0].capture_time.ToDoubleT();
    EXPECT_STREQ(kTestCaptureTime, base::NumberToString(time_double).c_str());
  }
}

}  // namespace
