// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/chrome_cleaner/engines/broker/interface_log_service.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

constexpr char kDummyBuildVersion[] = "DUMMY_BUILD_VERSION";

}  // namespace

class InterfaceLogServiceTest : public testing::Test {
 public:
  void SetUp() override {
    log_service_ = InterfaceLogService::Create(
        kLogFileName, base::UTF8ToUTF16(kDummyBuildVersion));
    expected_file_size_ = 0LL;
  }

  void InvalidFunction() {
    // Try to log nothing.
    log_service_->ObserveCall({});
  }

  void CheckFileSizeIncreased() {
    int64_t file_size;
    GetFileSize(&file_size);
    EXPECT_GT(file_size, expected_file_size_)
        << log_service_->GetLogFilePath()
        << " did not grow between calls to CheckFileSizeIncreased";

    // Next time this is called the size should be greater than it currently is.
    expected_file_size_ = file_size;
  }

  void CheckFileSizeUnchanged() {
    int64_t file_size;
    GetFileSize(&file_size);
    EXPECT_EQ(file_size, expected_file_size_)
        << log_service_->GetLogFilePath()
        << " grew unexpectedly after last call to CheckFileSizeIncreased";
  }

  void GetFileSize(int64_t* size) {
    // This function uses ASSERT to return early on error. This will only abort
    // the current function, not the whole test, but will still record a test
    // failure. If this happens treat the file size as 0.
    *size = 0LL;

    base::FilePath log_file_path = log_service_->GetLogFilePath();
    base::File file(log_file_path,
                    base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(file.IsValid())
        << log_file_path << " couldn't be opened: "
        << base::File::ErrorToString(file.GetLastFileError());

    base::File::Info info;
    ASSERT_TRUE(file.GetInfo(&info))
        << log_file_path << " couldn't be sized: "
        << base::File::ErrorToString(file.GetLastFileError());

    *size = info.size;
  }

  int64_t expected_file_size_;
  std::unique_ptr<InterfaceLogService> log_service_;
  const base::string16 kLogFileName = L"interface_log_service_test";
  const std::string kFileName = __FILE__;
};

class TestClass1 {
 public:
  explicit TestClass1(InterfaceLogService* log_service)
      : log_service_(log_service) {}
  // Auxiliary functions, they do nothing but add a new entry to the
  // call history.
  void function1(std::string parameter1, int32_t parameter2) {
    std::map<std::string, std::string> params;
    params["parameter1"] = parameter1;
    std::string s_parameter2 = base::NumberToString(parameter2);
    params["parameter2"] = s_parameter2;
    log_service_->ObserveCall(CURRENT_FILE_AND_METHOD, params);
  }

  void function3() { log_service_->ObserveCall(CURRENT_FILE_AND_METHOD); }

 private:
  InterfaceLogService* log_service_;
};

class TestClass2 {
 public:
  explicit TestClass2(InterfaceLogService* log_service)
      : log_service_(log_service) {}

  void function2(std::map<std::string, int32_t> map_parameter) {
    // There is no currently supported way to log the value of an object
    // into the InterfaceLogService, so the params can go empty
    log_service_->ObserveCall(CURRENT_FILE_AND_METHOD);
  }

 private:
  InterfaceLogService* log_service_;
};

TEST_F(InterfaceLogServiceTest, LogAndRecoverTest) {
  const std::string kString1 = "hola amigo";
  const std::string kString2 = "second call";
  const int kInt1 = 45;
  const int kInt2 = 43;
  TestClass1 test_object1(log_service_.get());
  TestClass2 test_object2(log_service_.get());

  // Some headers are written at the top of the log file, so its size should
  // already be >0.
  CheckFileSizeIncreased();

  // First make some calls and check that the log file grows after each.
  test_object1.function1(kString1, kInt1);
  CheckFileSizeIncreased();

  test_object1.function3();
  CheckFileSizeIncreased();

  test_object1.function1(kString2, kInt2);
  CheckFileSizeIncreased();

  std::map<std::string, int32_t> unused_map;
  test_object2.function2(unused_map);
  CheckFileSizeIncreased();

  test_object1.function3();
  CheckFileSizeIncreased();

  // This function should be ignored on the log service.
  InvalidFunction();
  CheckFileSizeUnchanged();

  // Read back the log file and check that the lines contain the expected calls.
  std::string file_contents;
  ASSERT_TRUE(
      base::ReadFileToString(log_service_->GetLogFilePath(), &file_contents));
  std::vector<std::string> lines = base::SplitString(
      file_contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  enum {
    kReadingVersion,
    kReadingHeader,
    kReadingCalls,
  } state = kReadingVersion;

  CallHistory call_history_from_file;

  for (const auto& line : lines) {
    std::vector<std::string> tokens = base::SplitString(
        line, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    switch (state) {
      case kReadingVersion:
        // Check that we have the same build version.
        ASSERT_EQ(tokens.size(), 2ULL);
        EXPECT_EQ(tokens[0], "buildVersion");
        call_history_from_file.set_build_version(tokens[1]);
        state = kReadingHeader;
        break;
      case kReadingHeader:
        EXPECT_THAT(tokens, ::testing::ElementsAre("timeCalledTicks",
                                                   "fileName", "functionName",
                                                   "functionArguments"));
        state = kReadingCalls;
        break;
      case kReadingCalls:
        ASSERT_EQ(tokens.size(), 4ULL);
        {
          int64_t microseconds_since_start;
          ASSERT_TRUE(
              base::StringToInt64(tokens[0], &microseconds_since_start));

          APICall* new_call = call_history_from_file.add_api_calls();
          new_call->set_microseconds_since_start(microseconds_since_start);
          new_call->set_file_name(tokens[1]);
          new_call->set_function_name(tokens[2]);

          std::vector<std::pair<std::string, std::string>> params;
          EXPECT_TRUE(
              base::SplitStringIntoKeyValuePairs(tokens[3], '=', ';', &params));
          for (const auto& name_value : params) {
            (*new_call->mutable_parameters())[name_value.first] =
                name_value.second;
          }
        }
        break;
      default:
        FAIL() << "Invalid state " << state;
        break;
    }
  }
  EXPECT_EQ(state, kReadingCalls);

  // Make sure the file contents and the data held in log_service_ are
  // equal.
  EXPECT_EQ(log_service_->GetBuildVersion(), kDummyBuildVersion);
  EXPECT_EQ(call_history_from_file.build_version(), kDummyBuildVersion);

  std::vector<APICall> call_record = log_service_->GetCallHistory();
  EXPECT_EQ(call_record.size(), 5UL);
  ASSERT_EQ(static_cast<size_t>(call_history_from_file.api_calls_size()),
            call_record.size());
  for (size_t i = 0; i < call_record.size(); ++i) {
    const APICall& call_from_file = call_history_from_file.api_calls(i);
    EXPECT_EQ(call_record[i].function_name(), call_from_file.function_name());
    EXPECT_EQ(call_record[i].file_name(), call_from_file.file_name());
    EXPECT_EQ(call_record[i].microseconds_since_start(),
              call_from_file.microseconds_since_start());
  }

  // Make sure that the call record that was retrieved from log_service_
  // contains the expected data. (This means the file will contain the expected
  // data too, since we already checked that they're equal.)

  // Check that the order is correct
  EXPECT_EQ(call_record[0].function_name(), "function1");
  EXPECT_EQ(call_record[0].file_name(), kFileName);
  EXPECT_EQ(2U, call_record[0].parameters().size());
  EXPECT_EQ(kString1, call_record[0].parameters().at("parameter1"));
  EXPECT_EQ(base::NumberToString(kInt1),
            call_record[0].parameters().at("parameter2"));

  EXPECT_EQ(call_record[1].function_name(), "function3");
  EXPECT_EQ(call_record[1].file_name(), kFileName);

  EXPECT_EQ(call_record[2].function_name(), "function1");
  EXPECT_EQ(call_record[2].file_name(), kFileName);
  EXPECT_EQ(kString2, call_record[2].parameters().at("parameter1"));
  EXPECT_EQ(base::NumberToString(kInt2),
            call_record[2].parameters().at("parameter2"));

  EXPECT_EQ(call_record[3].function_name(), "function2");
  EXPECT_EQ(call_record[3].file_name(), kFileName);
  EXPECT_EQ(call_record[4].function_name(), "function3");
  EXPECT_EQ(call_record[4].file_name(), kFileName);

  // Check that the times are increasing.
  int64_t time_call1 = call_record[0].microseconds_since_start();
  int64_t time_call2 = call_record[1].microseconds_since_start();
  int64_t time_call3 = call_record[2].microseconds_since_start();
  int64_t time_call4 = call_record[3].microseconds_since_start();
  int64_t time_call5 = call_record[4].microseconds_since_start();

  EXPECT_LE(time_call1, time_call2);
  EXPECT_LE(time_call2, time_call3);
  EXPECT_LE(time_call3, time_call4);
  EXPECT_LE(time_call4, time_call5);

  // Finally check that for the functions that have parameters they are
  // untouched.
  std::map<std::string, std::string> parameters1;
  parameters1.insert(call_record[0].parameters().begin(),
                     call_record[0].parameters().end());
  EXPECT_EQ(parameters1["parameter1"], kString1);
  EXPECT_EQ(parameters1["parameter2"], base::NumberToString(kInt1));

  std::map<std::string, std::string> parameters2;
  parameters2.insert(call_record[2].parameters().begin(),
                     call_record[2].parameters().end());
  EXPECT_EQ(parameters2["parameter1"], kString2);
  EXPECT_EQ(parameters2["parameter2"], base::NumberToString(kInt2));
}

TEST_F(InterfaceLogServiceTest, EmptyLogFileTest) {
  EXPECT_FALSE(
      InterfaceLogService::Create(L"", base::UTF8ToUTF16(kDummyBuildVersion)));
}

}  // namespace chrome_cleaner
