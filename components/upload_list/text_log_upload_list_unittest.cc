// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/upload_list/text_log_upload_list.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestUploadTime[] = "1234567890";
constexpr char kTestUploadId[] = "0123456789abcdef";
constexpr char kTestLocalID[] = "fedcba9876543210";
constexpr char kTestCaptureTime[] = "2345678901";
constexpr char kTestSource[] = "test_source";
constexpr char kTestPathHash[] = "1a2b3c4d5e6f";
// Explicitly partly taken from `base::kWhitespaceASCII` so our test doesn't
// depend on the change of the behavior of the base library.
constexpr char kTestWhitespaces[] = {' ', '\f', '\r', '\t'};

class TextLogUploadListTest : public testing::Test {
 public:
  TextLogUploadListTest() = default;

  TextLogUploadListTest(const TextLogUploadListTest&) = delete;
  TextLogUploadListTest& operator=(const TextLogUploadListTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

 protected:
  void WriteUploadLog(const std::string& log_data) {
    ASSERT_TRUE(base::WriteFile(log_path(), log_data));
  }

  base::FilePath log_path() {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("uploads.log"));
  }

 private:
  base::ScopedTempDir temp_dir_;

 protected:
  base::test::TaskEnvironment task_environment_;
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

  const std::vector<const UploadList::UploadInfo*> uploads =
      upload_list->GetUploads(999);

  EXPECT_EQ(1u, uploads.size());
  double time_double = uploads[0]->upload_time.InSecondsFSinceUnixEpoch();
  EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
  EXPECT_STREQ(kTestUploadId, uploads[0]->upload_id.c_str());
  EXPECT_STREQ("", uploads[0]->local_id.c_str());
  time_double = uploads[0]->capture_time.InSecondsFSinceUnixEpoch();
  EXPECT_STREQ("0", base::NumberToString(time_double).c_str());
}

TEST_F(TextLogUploadListTest, ParseUploadTimeUploadId_JSON) {
  std::stringstream stream;
  stream << "{";
  stream << "\"upload_time\":\"" << kTestUploadTime << "\",";
  stream << "\"upload_id\":\"" << kTestUploadId << "\"";
  stream << "}" << std::endl;
  WriteUploadLog(stream.str());

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();

  const std::vector<const UploadList::UploadInfo*> uploads =
      upload_list->GetUploads(999);

  EXPECT_EQ(1u, uploads.size());
  double time_double = uploads[0]->upload_time.InSecondsFSinceUnixEpoch();
  EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
  EXPECT_STREQ(kTestUploadId, uploads[0]->upload_id.c_str());
  EXPECT_STREQ("", uploads[0]->local_id.c_str());
  time_double = uploads[0]->capture_time.InSecondsFSinceUnixEpoch();
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

  const std::vector<const UploadList::UploadInfo*> uploads =
      upload_list->GetUploads(999);

  EXPECT_EQ(1u, uploads.size());
  double time_double = uploads[0]->upload_time.InSecondsFSinceUnixEpoch();
  EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
  EXPECT_STREQ(kTestUploadId, uploads[0]->upload_id.c_str());
  EXPECT_STREQ(kTestLocalID, uploads[0]->local_id.c_str());
  time_double = uploads[0]->capture_time.InSecondsFSinceUnixEpoch();
  EXPECT_STREQ("0", base::NumberToString(time_double).c_str());
}

TEST_F(TextLogUploadListTest, ParseUploadTimeUploadIdLocalId_JSON) {
  std::stringstream stream;
  stream << "{";
  stream << "\"upload_time\":\"" << kTestUploadTime << "\",";
  stream << "\"upload_id\":\"" << kTestUploadId << "\",";
  stream << "\"local_id\":\"" << kTestLocalID << "\"";
  stream << "}" << std::endl;
  WriteUploadLog(stream.str());

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();

  const std::vector<const UploadList::UploadInfo*> uploads =
      upload_list->GetUploads(999);

  EXPECT_EQ(1u, uploads.size());
  double time_double = uploads[0]->upload_time.InSecondsFSinceUnixEpoch();
  EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
  EXPECT_STREQ(kTestUploadId, uploads[0]->upload_id.c_str());
  EXPECT_STREQ(kTestLocalID, uploads[0]->local_id.c_str());
  time_double = uploads[0]->capture_time.InSecondsFSinceUnixEpoch();
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

  const std::vector<const UploadList::UploadInfo*> uploads =
      upload_list->GetUploads(999);

  EXPECT_EQ(1u, uploads.size());
  double time_double = uploads[0]->upload_time.InSecondsFSinceUnixEpoch();
  EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
  EXPECT_STREQ(kTestUploadId, uploads[0]->upload_id.c_str());
  EXPECT_STREQ("", uploads[0]->local_id.c_str());
  time_double = uploads[0]->capture_time.InSecondsFSinceUnixEpoch();
  EXPECT_STREQ(kTestCaptureTime, base::NumberToString(time_double).c_str());
}

TEST_F(TextLogUploadListTest, ParseUploadTimeUploadIdCaptureTime_JSON) {
  std::stringstream stream;
  stream << "{";
  stream << "\"upload_time\":\"" << kTestUploadTime << "\",";
  stream << "\"upload_id\":\"" << kTestUploadId << "\",";
  stream << "\"capture_time\":\"" << kTestCaptureTime << "\"";
  stream << "}" << std::endl;
  WriteUploadLog(stream.str());

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();

  const std::vector<const UploadList::UploadInfo*> uploads =
      upload_list->GetUploads(999);

  EXPECT_EQ(1u, uploads.size());
  double time_double = uploads[0]->upload_time.InSecondsFSinceUnixEpoch();
  EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
  EXPECT_STREQ(kTestUploadId, uploads[0]->upload_id.c_str());
  EXPECT_STREQ("", uploads[0]->local_id.c_str());
  time_double = uploads[0]->capture_time.InSecondsFSinceUnixEpoch();
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

  const std::vector<const UploadList::UploadInfo*> uploads =
      upload_list->GetUploads(999);

  EXPECT_EQ(1u, uploads.size());
  double time_double = uploads[0]->upload_time.InSecondsFSinceUnixEpoch();
  EXPECT_STREQ("0", base::NumberToString(time_double).c_str());
  EXPECT_STREQ("", uploads[0]->upload_id.c_str());
  EXPECT_STREQ(kTestLocalID, uploads[0]->local_id.c_str());
  time_double = uploads[0]->capture_time.InSecondsFSinceUnixEpoch();
  EXPECT_STREQ(kTestCaptureTime, base::NumberToString(time_double).c_str());
}

TEST_F(TextLogUploadListTest, ParseLocalIdCaptureTime_JSON) {
  std::stringstream stream;
  stream << "{";
  stream << "\"local_id\":\"" << kTestLocalID << "\",";
  stream << "\"capture_time\":\"" << kTestCaptureTime << "\"";
  stream << "}" << std::endl;
  WriteUploadLog(stream.str());

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();

  const std::vector<const UploadList::UploadInfo*> uploads =
      upload_list->GetUploads(999);

  EXPECT_EQ(1u, uploads.size());
  double time_double = uploads[0]->upload_time.InSecondsFSinceUnixEpoch();
  EXPECT_STREQ("0", base::NumberToString(time_double).c_str());
  EXPECT_STREQ("", uploads[0]->upload_id.c_str());
  EXPECT_STREQ(kTestLocalID, uploads[0]->local_id.c_str());
  time_double = uploads[0]->capture_time.InSecondsFSinceUnixEpoch();
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

  const std::vector<const UploadList::UploadInfo*> uploads =
      upload_list->GetUploads(999);

  EXPECT_EQ(1u, uploads.size());
  double time_double = uploads[0]->upload_time.InSecondsFSinceUnixEpoch();
  EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
  EXPECT_STREQ(kTestUploadId, uploads[0]->upload_id.c_str());
  EXPECT_STREQ(kTestLocalID, uploads[0]->local_id.c_str());
  time_double = uploads[0]->capture_time.InSecondsFSinceUnixEpoch();
  EXPECT_STREQ(kTestCaptureTime, base::NumberToString(time_double).c_str());
}

TEST_F(TextLogUploadListTest, ParseUploadTimeUploadIdLocalIdCaptureTime_JSON) {
  std::stringstream stream;
  stream << "{";
  stream << "\"upload_time\":\"" << kTestUploadTime << "\",";
  stream << "\"upload_id\":\"" << kTestUploadId << "\",";
  stream << "\"local_id\":\"" << kTestLocalID << "\",";
  stream << "\"capture_time\":\"" << kTestCaptureTime << "\"";
  stream << "}" << std::endl;
  WriteUploadLog(stream.str());

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();

  const std::vector<const UploadList::UploadInfo*> uploads =
      upload_list->GetUploads(999);

  EXPECT_EQ(1u, uploads.size());
  double time_double = uploads[0]->upload_time.InSecondsFSinceUnixEpoch();
  EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
  EXPECT_STREQ(kTestUploadId, uploads[0]->upload_id.c_str());
  EXPECT_STREQ(kTestLocalID, uploads[0]->local_id.c_str());
  time_double = uploads[0]->capture_time.InSecondsFSinceUnixEpoch();
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

  const std::vector<const UploadList::UploadInfo*> uploads =
      upload_list->GetUploads(999);

  EXPECT_EQ(4u, uploads.size());
  // The entries order should be reversed during the parsing.
  for (size_t i = 0; i < uploads.size(); ++i) {
    double time_double = uploads[i]->upload_time.InSecondsFSinceUnixEpoch();
    EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
    EXPECT_STREQ(kTestUploadId, uploads[i]->upload_id.c_str());
    EXPECT_EQ(base::NumberToString(uploads.size() - i), uploads[i]->local_id);
    time_double = uploads[i]->capture_time.InSecondsFSinceUnixEpoch();
    EXPECT_STREQ(kTestCaptureTime, base::NumberToString(time_double).c_str());
  }
}

TEST_F(TextLogUploadListTest, ParseMultipleEntries_JSON) {
  std::stringstream stream;
  for (int i = 1; i <= 4; ++i) {
    stream << "{";
    stream << "\"upload_time\":\"" << kTestUploadTime << "\",";
    stream << "\"upload_id\":\"" << kTestUploadId << "\",";
    stream << "\"local_id\":\"" << i << "\",";
    stream << "\"capture_time\":\"" << kTestCaptureTime << "\"";
    stream << "}" << std::endl;
  }
  WriteUploadLog(stream.str());

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();

  const std::vector<const UploadList::UploadInfo*> uploads =
      upload_list->GetUploads(999);

  EXPECT_EQ(4u, uploads.size());
  // The entries order should be reversed during the parsing.
  for (size_t i = 0; i < uploads.size(); ++i) {
    double time_double = uploads[i]->upload_time.InSecondsFSinceUnixEpoch();
    EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
    EXPECT_STREQ(kTestUploadId, uploads[i]->upload_id.c_str());
    EXPECT_EQ(base::NumberToString(uploads.size() - i), uploads[i]->local_id);
    time_double = uploads[i]->capture_time.InSecondsFSinceUnixEpoch();
    EXPECT_STREQ(kTestCaptureTime, base::NumberToString(time_double).c_str());
  }
}

TEST_F(TextLogUploadListTest, ParseWithMultipleDelimiters) {
  std::ostringstream stream;
  for (const auto delimiter : kTestWhitespaces) {
    stream << kTestUploadTime << ',';
    stream << kTestUploadId << ',';
    stream << kTestLocalID << ',';
    stream << kTestCaptureTime << delimiter;
  }
  WriteUploadLog(stream.str());

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();

  const std::vector<const UploadList::UploadInfo*> uploads =
      upload_list->GetUploads(999);

  EXPECT_EQ(std::size(kTestWhitespaces), uploads.size());
  for (const auto* upload : uploads) {
    double time_double = upload->upload_time.InSecondsFSinceUnixEpoch();
    EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
    EXPECT_STREQ(kTestUploadId, upload->upload_id.c_str());
    EXPECT_STREQ(kTestLocalID, upload->local_id.c_str());
    time_double = upload->capture_time.InSecondsFSinceUnixEpoch();
    EXPECT_STREQ(kTestCaptureTime, base::NumberToString(time_double).c_str());
  }
}

TEST_F(TextLogUploadListTest, ParseWithMultipleDelimiters_JSON) {
  std::ostringstream stream;
  for (const auto delimiter : kTestWhitespaces) {
    stream << "{";
    stream << "\"upload_time\":\"" << kTestUploadTime << "\",";
    stream << "\"upload_id\":\"" << kTestUploadId << "\",";
    stream << "\"local_id\":\"" << kTestLocalID << "\",";
    stream << "\"capture_time\":\"" << kTestCaptureTime << "\"";
    stream << "}" << delimiter;
  }
  WriteUploadLog(stream.str());

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();

  const std::vector<const UploadList::UploadInfo*> uploads =
      upload_list->GetUploads(999);

  EXPECT_EQ(std::size(kTestWhitespaces), uploads.size());
  for (const UploadList::UploadInfo* upload : uploads) {
    double time_double = upload->upload_time.InSecondsFSinceUnixEpoch();
    EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
    EXPECT_STREQ(kTestUploadId, upload->upload_id.c_str());
    EXPECT_STREQ(kTestLocalID, upload->local_id.c_str());
    time_double = upload->capture_time.InSecondsFSinceUnixEpoch();
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

  const std::vector<const UploadList::UploadInfo*> uploads =
      upload_list->GetUploads(999);

  EXPECT_EQ(4u, uploads.size());
  for (const auto* upload : uploads) {
    double time_double = upload->upload_time.InSecondsFSinceUnixEpoch();
    EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
    EXPECT_STREQ(kTestUploadId, upload->upload_id.c_str());
    EXPECT_STREQ(kTestLocalID, upload->local_id.c_str());
    time_double = upload->capture_time.InSecondsFSinceUnixEpoch();
    EXPECT_STREQ(kTestCaptureTime, base::NumberToString(time_double).c_str());
    EXPECT_EQ(UploadList::UploadInfo::State::Uploaded, upload->state);
  }
}

TEST_F(TextLogUploadListTest, ParseWithState_JSON) {
  std::stringstream stream;
  for (int i = 1; i <= 4; ++i) {
    stream << "{";
    stream << "\"upload_time\":\"" << kTestUploadTime << "\",";
    stream << "\"upload_id\":\"" << kTestUploadId << "\",";
    stream << "\"local_id\":\"" << kTestLocalID << "\",";
    stream << "\"capture_time\":\"" << kTestCaptureTime << "\",";
    stream << "\"state\":"
           << static_cast<int>(UploadList::UploadInfo::State::Uploaded);
    stream << "}" << std::endl;
  }
  WriteUploadLog(stream.str());

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();

  const std::vector<const UploadList::UploadInfo*> uploads =
      upload_list->GetUploads(999);

  EXPECT_EQ(4u, uploads.size());
  for (const UploadList::UploadInfo* upload : uploads) {
    double time_double = upload->upload_time.InSecondsFSinceUnixEpoch();
    EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
    EXPECT_STREQ(kTestUploadId, upload->upload_id.c_str());
    EXPECT_STREQ(kTestLocalID, upload->local_id.c_str());
    time_double = upload->capture_time.InSecondsFSinceUnixEpoch();
    EXPECT_STREQ(kTestCaptureTime, base::NumberToString(time_double).c_str());
    EXPECT_EQ(UploadList::UploadInfo::State::Uploaded, upload->state);
  }
}

TEST_F(TextLogUploadListTest, ParseWithSource_JSON) {
  std::stringstream stream;
  for (int i = 1; i <= 4; ++i) {
    stream << "{";
    stream << "\"upload_time\":\"" << kTestUploadTime << "\",";
    stream << "\"upload_id\":\"" << kTestUploadId << "\",";
    stream << "\"local_id\":\"" << kTestLocalID << "\",";
    stream << "\"capture_time\":\"" << kTestCaptureTime << "\",";
    stream << "\"state\":"
           << static_cast<int>(UploadList::UploadInfo::State::Uploaded) << ",";
    stream << "\"source\":\"" << kTestSource << "\"";
    stream << "}" << std::endl;
  }
  WriteUploadLog(stream.str());

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();

  const std::vector<const UploadList::UploadInfo*> uploads =
      upload_list->GetUploads(999);

  EXPECT_EQ(4u, uploads.size());
  for (const UploadList::UploadInfo* upload : uploads) {
    double time_double = upload->upload_time.InSecondsFSinceUnixEpoch();
    EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
    EXPECT_STREQ(kTestUploadId, upload->upload_id.c_str());
    EXPECT_STREQ(kTestLocalID, upload->local_id.c_str());
    time_double = upload->capture_time.InSecondsFSinceUnixEpoch();
    EXPECT_STREQ(kTestCaptureTime, base::NumberToString(time_double).c_str());
    EXPECT_EQ(UploadList::UploadInfo::State::Uploaded, upload->state);
    EXPECT_STREQ(kTestSource, upload->source.c_str());
  }
}

TEST_F(TextLogUploadListTest, ParseWithPathHash_JSON) {
  std::stringstream stream;
  for (int i = 1; i <= 4; ++i) {
    stream << "{";
    stream << "\"upload_time\":\"" << kTestUploadTime << "\",";
    stream << "\"upload_id\":\"" << kTestUploadId << "\",";
    stream << "\"local_id\":\"" << kTestLocalID << "\",";
    stream << "\"capture_time\":\"" << kTestCaptureTime << "\",";
    stream << "\"state\":"
           << static_cast<int>(UploadList::UploadInfo::State::Uploaded) << ",";
    stream << "\"source\":\"" << kTestSource << "\",";
    stream << "\"path_hash\":\"" << kTestPathHash << "\"";
    stream << "}" << std::endl;
  }
  WriteUploadLog(stream.str());

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();

  const std::vector<const UploadList::UploadInfo*> uploads =
      upload_list->GetUploads(999);

  EXPECT_EQ(4u, uploads.size());
  for (const UploadList::UploadInfo* upload : uploads) {
    double time_double = upload->upload_time.InSecondsFSinceUnixEpoch();
    EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
    EXPECT_STREQ(kTestUploadId, upload->upload_id.c_str());
    EXPECT_STREQ(kTestLocalID, upload->local_id.c_str());
    time_double = upload->capture_time.InSecondsFSinceUnixEpoch();
    EXPECT_STREQ(kTestCaptureTime, base::NumberToString(time_double).c_str());
    EXPECT_EQ(UploadList::UploadInfo::State::Uploaded, upload->state);
    EXPECT_STREQ(kTestSource, upload->source.c_str());
    EXPECT_STREQ(kTestPathHash, upload->path_hash.c_str());
  }
}

TEST_F(TextLogUploadListTest, ParseHybridFormat) {
  std::stringstream stream;
  for (int i = 1; i <= 4; ++i) {
    stream << kTestUploadTime << ",";
    stream << kTestUploadId << ",";
    stream << kTestLocalID << ",";
    stream << kTestCaptureTime << std::endl;
  }

  for (int i = 1; i <= 4; ++i) {
    stream << "{";
    stream << "\"upload_time\":\"" << kTestUploadTime << "\",";
    stream << "\"upload_id\":\"" << kTestUploadId << "\",";
    stream << "\"local_id\":\"" << kTestLocalID << "\",";
    stream << "\"capture_time\":\"" << kTestCaptureTime << "\"";
    stream << "}" << std::endl;
  }
  WriteUploadLog(stream.str());
  std::cout << stream.str() << std::endl;

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();

  const std::vector<const UploadList::UploadInfo*> uploads =
      upload_list->GetUploads(999);

  EXPECT_EQ(8u, uploads.size());
  for (const UploadList::UploadInfo* upload : uploads) {
    double time_double = upload->upload_time.InSecondsFSinceUnixEpoch();
    EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
    EXPECT_STREQ(kTestUploadId, upload->upload_id.c_str());
    EXPECT_STREQ(kTestLocalID, upload->local_id.c_str());
    time_double = upload->capture_time.InSecondsFSinceUnixEpoch();
    EXPECT_STREQ(kTestCaptureTime, base::NumberToString(time_double).c_str());
  }
}

TEST_F(TextLogUploadListTest, SkipInvalidEntry_JSON) {
  std::stringstream stream;
  // the first JSON entry contains the invalid |upload_id|.
  stream << "{";
  stream << "\"upload_time\":\"" << kTestCaptureTime << "\",";
  stream << "\"upload_id\":0.1234";
  stream << "}" << std::endl;
  // the second JSON entry is valid.
  stream << "{";
  stream << "\"upload_time\":\"" << kTestCaptureTime << "\",";
  stream << "\"upload_id\":\"" << kTestLocalID << "\"";
  stream << "}" << std::endl;
  WriteUploadLog(stream.str());

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();

  const std::vector<const UploadList::UploadInfo*> uploads =
      upload_list->GetUploads(999);

  // The invalid JSON entry should be skipped.
  EXPECT_EQ(1u, uploads.size());
}

// Test log entry string with only single column.
// Such kind of lines are considered as invalid CSV entry. They should be
// skipped in parsing the log file.
TEST_F(TextLogUploadListTest, SkipBlankOrCorruptedEntry) {
  std::string test_entry;

  // Add an empty line.
  test_entry += "\n";

  // Add a line with only single column.
  test_entry.append(kTestUploadTime);
  test_entry += "\n";

  WriteUploadLog(test_entry);

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();

  const std::vector<const UploadList::UploadInfo*> uploads =
      upload_list->GetUploads(999);

  EXPECT_EQ(0u, uploads.size());
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

TEST_F(TextLogUploadListTest, ClearUsingUploadTime_JSON) {
  constexpr time_t kTestTime = 1234u;

  std::stringstream stream_other_entry;
  stream_other_entry << "{";
  stream_other_entry << "\"upload_time\":\"4567\",";
  stream_other_entry << "\"upload_id\":\"def\"";
  stream_other_entry << "}" << std::endl;
  std::string other_entry = stream_other_entry.str();

  std::stringstream stream;
  stream << "{";
  stream << "\"upload_time\":\"" << kTestTime << "\",";
  stream << "\"upload_id\":\"abc\"";
  stream << "}" << std::endl;
  stream << other_entry;
  WriteUploadLog(stream.str());

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Clear(base::Time::FromTimeT(kTestTime),
                     base::Time::FromTimeT(kTestTime + 1),
                     run_loop.QuitClosure());
  run_loop.Run();

  std::string contents;
  base::ReadFileToString(log_path(), &contents);
  EXPECT_EQ(other_entry, contents);
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

TEST_F(TextLogUploadListTest, ClearUsingCaptureTime_JSON) {
  constexpr time_t kTestTime = 1234u;

  std::stringstream stream_other_entry;
  stream_other_entry << "{";
  stream_other_entry << "\"upload_time\":\"4567\",";
  stream_other_entry << "\"upload_id\":\"def\",";
  stream_other_entry << "\"local_id\":\"def\",";
  stream_other_entry << "\"capture_time\":\"7890\"";
  stream_other_entry << "}" << std::endl;
  std::string other_entry = stream_other_entry.str();

  std::stringstream stream;
  stream << "{";
  stream << "\"upload_time\":\"4567\",";
  stream << "\"upload_id\":\"abc\",";
  stream << "\"local_id\":\"abc\",";
  stream << "\"capture_time\":\"" << kTestTime << "\"";
  stream << "}" << std::endl;
  stream << other_entry;
  WriteUploadLog(stream.str());

  scoped_refptr<TextLogUploadList> upload_list =
      new TextLogUploadList(log_path());

  base::RunLoop run_loop;
  upload_list->Clear(base::Time::FromTimeT(kTestTime),
                     base::Time::FromTimeT(kTestTime + 1),
                     run_loop.QuitClosure());
  run_loop.Run();

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(log_path(), &contents));
  EXPECT_EQ(other_entry, contents);
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

TEST_F(TextLogUploadListTest, ClearingAllDataDeletesFile_JSON) {
  constexpr time_t kTestTime = 1234u;

  std::stringstream stream;
  stream << "{";
  stream << "\"upload_time\":\"" << kTestTime << "\",";
  stream << "\"upload_id\":\"abc\"";
  stream << "}" << std::endl;
  WriteUploadLog(stream.str());

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
    const std::vector<const UploadList::UploadInfo*> uploads =
        upload_list->GetUploads(999);

    EXPECT_EQ(1u, uploads.size());
    double time_double = uploads[0]->upload_time.InSecondsFSinceUnixEpoch();
    EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
    EXPECT_STREQ(kTestUploadId, uploads[0]->upload_id.c_str());
    EXPECT_STREQ(kTestLocalID, uploads[0]->local_id.c_str());
    time_double = uploads[0]->capture_time.InSecondsFSinceUnixEpoch();
    EXPECT_STREQ(kTestCaptureTime, base::NumberToString(time_double).c_str());
  }

  // Allow the remaining loads to complete.
  task_environment_.RunUntilIdle();
}

TEST_F(TextLogUploadListTest, SimultaneousAccess_JSON) {
  std::stringstream stream;
  stream << "{";
  stream << "\"upload_time\":\"" << kTestUploadTime << "\",";
  stream << "\"upload_id\":\"" << kTestUploadId << "\",";
  stream << "\"local_id\":\"" << kTestLocalID << "\",";
  stream << "\"capture_time\":\"" << kTestCaptureTime << "\"";
  stream << "}" << std::endl;
  WriteUploadLog(stream.str());

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
    const std::vector<const UploadList::UploadInfo*> uploads =
        upload_list->GetUploads(999);

    EXPECT_EQ(1u, uploads.size());
    double time_double = uploads[0]->upload_time.InSecondsFSinceUnixEpoch();
    EXPECT_STREQ(kTestUploadTime, base::NumberToString(time_double).c_str());
    EXPECT_STREQ(kTestUploadId, uploads[0]->upload_id.c_str());
    EXPECT_STREQ(kTestLocalID, uploads[0]->local_id.c_str());
    time_double = uploads[0]->capture_time.InSecondsFSinceUnixEpoch();
    EXPECT_STREQ(kTestCaptureTime, base::NumberToString(time_double).c_str());
  }

  // Allow the remaining loads to complete.
  task_environment_.RunUntilIdle();
}

}  // namespace
