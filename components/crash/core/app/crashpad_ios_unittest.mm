// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/crashpad.h"

#import <Foundation/Foundation.h>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/sys_string_conversions.h"
#include "components/crash/core/app/crash_reporter_client.h"
#import "components/crash/core/common/reporter_running_ios.h"
#include "testing/platform_test.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"

namespace {

// A class to temporarily set a crash reporter client.
// Setting the global value when crashpad is in use can have unexpected
// consequences, so this class asserts that the values are null before setting
// them and restores null values on destruction.
class ScopedTestCrashDatabaseDir {
 public:
  ScopedTestCrashDatabaseDir() = default;

  void Init() {
    ASSERT_FALSE(crash_reporter::internal::GetCrashReportDatabase());
    ASSERT_TRUE(database_dir_.CreateUniqueTempDir());
    database_dir_path_ = database_dir_.GetPath();
    database_ = crashpad::CrashReportDatabase::Initialize(database_dir_path_);
    crash_reporter::internal::SetCrashReportDatabaseForTesting(
        database_.get(), &database_dir_path_);
  }

  ~ScopedTestCrashDatabaseDir() {
    crash_reporter::internal::SetCrashReportDatabaseForTesting(nullptr,
                                                               nullptr);
  }

  // The temporary path to the database.
  base::FilePath DatabasePath() { return database_dir_path_; }

  // The temporary database.
  crashpad::CrashReportDatabase* Database() { return database_.get(); }

 private:
  base::ScopedTempDir database_dir_;
  base::FilePath database_dir_path_;
  std::unique_ptr<crashpad::CrashReportDatabase> database_;
};

}  // namespace
using CrashpadIOS = PlatformTest;

TEST_F(CrashpadIOS, ProcessExternalDump) {
  ScopedTestCrashDatabaseDir scoped_test_crash_database_dir;
  scoped_test_crash_database_dir.Init();
  base::FilePath database_path = scoped_test_crash_database_dir.DatabasePath();
  crashpad::CrashReportDatabase* database =
      scoped_test_crash_database_dir.Database();

  std::vector<crashpad::CrashReportDatabase::Report> reports;
  EXPECT_EQ(database->GetPendingReports(&reports),
            crashpad::CrashReportDatabase::kNoError);
  ASSERT_EQ(reports.size(), 0u);
  std::string attachment_name = "external-source";
  const char attachment_data[] = "external-dump-data";
  base::span<const uint8_t> data(
      reinterpret_cast<const uint8_t*>(attachment_data),
      sizeof(attachment_data));
  crash_reporter::ProcessExternalDump(attachment_name, data, {});

  reports.clear();
  EXPECT_EQ(database->GetPendingReports(&reports),
            crashpad::CrashReportDatabase::kNoError);
  ASSERT_EQ(reports.size(), 1u);

  std::unique_ptr<const crashpad::CrashReportDatabase::UploadReport>
      upload_report;
  EXPECT_EQ(database->GetReportForUploading(reports[0].uuid, &upload_report),
            crashpad::CrashReportDatabase::kNoError);

  std::map<std::string, crashpad::FileReader*> attachments =
      upload_report->GetAttachments();
  EXPECT_EQ(attachments.size(), 1u);
  ASSERT_NE(attachments.find(attachment_name), attachments.end());
  char result_buffer[sizeof(attachment_data)];
  attachments[attachment_name]->Read(result_buffer, sizeof(result_buffer));
  EXPECT_EQ(memcmp(attachment_data, result_buffer, sizeof(attachment_data)), 0);
}
