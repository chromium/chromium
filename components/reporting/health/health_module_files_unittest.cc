// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/health/health_module_files.h"

#include <string>

#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::IsEmpty;
using ::testing::StrEq;

namespace reporting {
namespace {

constexpr char kBaseFileOne[] = "base_file_name";
constexpr char kUnmatchedBaseFile[] = "no_files_match_this";
constexpr int kInitialFiles = 3;
constexpr int kIntialRecordsPerFile = 5;
constexpr uint32_t kSimpleEnqueueRecordCallLineSize = 21u;
constexpr uint32_t kSmallMaxStorageSize = 70u;
constexpr uint32_t kLargeMaxStorageSize = 1000u;
constexpr uint32_t kFullMaxStorageSize = 20000u;

class HealthModuleFilesTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(directory_.CreateUniqueTempDir());

    for (int i = 0; i < kInitialFiles; i++) {
      const std::string file_name =
          base::StrCat({kBaseFileOne, base::NumberToString(i)});
      for (int k = 0; k < kIntialRecordsPerFile; k++) {
        auto call = AddEnqueueRecordCall();
        *initial_health_data_.add_history() = call;
        ASSERT_TRUE(AppendLine(directory_.GetPath().AppendASCII(file_name),
                               base::HexEncode(base::as_bytes(
                                   base::make_span(call.SerializeAsString()))))
                        .ok());
      }
    }
  }

  HealthDataHistory AddEnqueueRecordCall() {
    HealthDataHistory history;
    EnqueueRecordCall call;
    call.set_priority(
        static_cast<Priority>(priority_counter_++ % Priority_MAX + 1));
    *history.mutable_enqueue_record_call() = call;
    history.set_timestamp_seconds(base::Time::Now().ToTimeT());
    return history;
  }

  base::ScopedTempDir directory_;

  ERPHealthData initial_health_data_;

 private:
  int priority_counter_ = Priority_MIN;
};

TEST_F(HealthModuleFilesTest, TestCreation) {
  ERPHealthData history;
  std::unique_ptr<HealthModuleFiles> files = HealthModuleFiles::Create(
      directory_.GetPath(), kBaseFileOne, kLargeMaxStorageSize);
  ASSERT_TRUE(files != nullptr);
  files->PopulateHistory(&history);
  EXPECT_THAT(history.SerializeAsString(),
              StrEq(initial_health_data_.SerializeAsString()));

  ERPHealthData empty_history;
  std::unique_ptr<HealthModuleFiles> no_files = HealthModuleFiles::Create(
      directory_.GetPath(), kUnmatchedBaseFile, kLargeMaxStorageSize);
  ASSERT_TRUE(no_files != nullptr);
  no_files->PopulateHistory(&empty_history);
  EXPECT_THAT(empty_history.history(), IsEmpty());

  ERPHealthData small_history;
  const uint32_t total_records_stored =
      kSmallMaxStorageSize / kSimpleEnqueueRecordCallLineSize;
  std::unique_ptr<HealthModuleFiles> small_files = HealthModuleFiles::Create(
      directory_.GetPath(), kBaseFileOne, kSmallMaxStorageSize);
  ASSERT_TRUE(small_files != nullptr);
  small_files->PopulateHistory(&small_history);
  int initial_size = initial_health_data_.history_size();
  initial_health_data_.mutable_history()->DeleteSubrange(
      0, initial_size - total_records_stored);
  EXPECT_THAT(small_history.SerializeAsString(),
              StrEq(initial_health_data_.SerializeAsString()));

  std::unique_ptr<HealthModuleFiles> null_files = HealthModuleFiles::Create(
      directory_.GetPath(), kBaseFileOne, /*max_storage_space=*/0);
  ASSERT_TRUE(null_files == nullptr);
}

TEST_F(HealthModuleFilesTest, TestFullStorage) {
  ERPHealthData history;
  std::unique_ptr<HealthModuleFiles> files = HealthModuleFiles::Create(
      directory_.GetPath(), kBaseFileOne, kFullMaxStorageSize);
  static constexpr uint32_t total_records_stored =
      kFullMaxStorageSize / kSimpleEnqueueRecordCallLineSize;
  ASSERT_TRUE(files != nullptr);
  for (int i = 0; i < 1000; i++) {
    auto call = AddEnqueueRecordCall();
    ASSERT_OK(files->Write(base::HexEncode(
        base::as_bytes(base::make_span(call.SerializeAsString())))));
    if (i + total_records_stored >= 1000) {
      *history.add_history() = call;
    }
  }
  ERPHealthData got;
  files->PopulateHistory(&got);
  EXPECT_THAT(history.SerializeAsString(), StrEq(got.SerializeAsString()));
}

TEST_F(HealthModuleFilesTest, NotEnoughStorage) {
  ERPHealthData history;
  std::unique_ptr<HealthModuleFiles> files = HealthModuleFiles::Create(
      directory_.GetPath(), kBaseFileOne, /*max_storage_space=*/1);
  ASSERT_TRUE(files != nullptr);
  files->PopulateHistory(&history);
  ASSERT_THAT(history.history(), IsEmpty());

  ASSERT_FALSE(files
                   ->Write(base::HexEncode(base::as_bytes(base::make_span(
                       AddEnqueueRecordCall().SerializeAsString()))))
                   .ok());
  files->PopulateHistory(&history);
  ASSERT_THAT(history.history(), IsEmpty());
}

TEST_F(HealthModuleFilesTest, JustEnoughStorage) {
  ERPHealthData history;
  std::unique_ptr<HealthModuleFiles> files = HealthModuleFiles::Create(
      directory_.GetPath(), kBaseFileOne, kSimpleEnqueueRecordCallLineSize);
  ASSERT_TRUE(files != nullptr);
  files->PopulateHistory(&history);
  int initial_size = initial_health_data_.history_size();
  initial_health_data_.mutable_history()->DeleteSubrange(0, initial_size - 1);
  ASSERT_THAT(history.SerializeAsString(),
              StrEq(initial_health_data_.SerializeAsString()));

  history.mutable_history()->Clear();
  auto call = AddEnqueueRecordCall();
  *initial_health_data_.mutable_history(0) = call;
  ASSERT_TRUE(files
                  ->Write(base::HexEncode(base::as_bytes(
                      base::make_span(call.SerializeAsString()))))
                  .ok());
  files->PopulateHistory(&history);
  EXPECT_THAT(history.SerializeAsString(),
              StrEq(initial_health_data_.SerializeAsString()));
}
}  // namespace
}  // namespace reporting
