// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/logging/pending_logs_service.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/http/http_agent_factory.h"
#include "chrome/chrome_cleaner/http/mock_http_agent_factory.h"
#include "chrome/chrome_cleaner/logging/cleaner_logging_service.h"
#include "chrome/chrome_cleaner/logging/proto/chrome_cleaner_report.pb.h"
#include "chrome/chrome_cleaner/logging/registry_logger.h"
#include "chrome/chrome_cleaner/logging/safe_browsing_reporter.h"
#include "chrome/chrome_cleaner/logging/test_utils.h"
#include "chrome/chrome_cleaner/os/task_scheduler.h"
#include "chrome/chrome_cleaner/test/test_branding.h"
#include "chrome/chrome_cleaner/test/test_settings_util.h"
#include "chrome/chrome_cleaner/test/test_task_scheduler.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

class PendingLogsServiceTest : public testing::Test {
 public:
  PendingLogsServiceTest()
      : logging_service_(nullptr),
        task_environment_(base::test::TaskEnvironment::MainThreadType::UI),
        done_callback_called_(false),
        upload_success_(false),
        cleanup_execution_mode_settings_(ExecutionMode::kCleanup) {}

  void SetUp() override {
    // This test creates an instance of CleanerLoggingService, which requires
    // ExecutionMode::kCleanup to log properly.
    Settings::SetInstanceForTesting(&cleanup_execution_mode_settings_);

    registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER);
    // The registry logger must be created after calling OverrideRegistry.
    registry_logger_.reset(new RegistryLogger(RegistryLogger::Mode::REMOVER));

    // Make sure to clear any previous tests content, e.g., log lines.
    // Individual tests will enable it appropriately.
    logging_service_ = CleanerLoggingService::GetInstance();
    LoggingServiceAPI::SetInstanceForTesting(logging_service_);
    logging_service_->Initialize(registry_logger_.get());

    // Use a mock HttpAgent instead of making real network requests.
    SafeBrowsingReporter::SetHttpAgentFactoryForTesting(
        http_agent_factory_.get());
    SafeBrowsingReporter::SetNetworkCheckerForTesting(&network_checker_);
  }

  void TearDown() override {
    SafeBrowsingReporter::SetNetworkCheckerForTesting(nullptr);
    SafeBrowsingReporter::SetHttpAgentFactoryForTesting(nullptr);

    registry_logger_.reset(nullptr);

    logging_service_->Terminate();
    LoggingServiceAPI::SetInstanceForTesting(nullptr);

    Settings::SetInstanceForTesting(nullptr);
  }

  // RetryNextPendingLogsUpload callback implementation.
  void LogsUploadCallback(base::OnceClosure run_loop_quit, bool success) {
    done_callback_called_ = true;
    upload_success_ = success;

    // A RunLoop will be waiting for this callback to run before proceeding
    // with the test. Now that the callback has run, we can quit the RunLoop.
    std::move(run_loop_quit).Run();
  }

 protected:
  // Helper method to successfully upload logs for a pending log file failure.
  void ValidatePendingLogsUploadFailure(const base::FilePath& log_file) {
    MockHttpAgentConfig::Calls calls(HttpStatus::kOk);
    const size_t calls_index = http_agent_config_.AddCalls(calls);

    // Set the failing file as the next pending log.
    ASSERT_TRUE(registry_logger_->AppendLogFilePath(log_file));
    // And now try to upload it, it should fail, but a failure logs should
    // successfully be uploaded.
    base::RunLoop run_loop;
    PendingLogsService pending_logs_service;
    pending_logs_service.RetryNextPendingLogsUpload(
        TEST_PRODUCT_SHORTNAME_STRING,
        base::BindRepeating(&PendingLogsServiceTest::LogsUploadCallback,
                            base::Unretained(this), run_loop.QuitClosure()),
        registry_logger_.get());
    run_loop.Run();

    std::string upload_data(http_agent_config_.request_data(calls_index).body);
    ChromeCleanerReport uploaded_report;
    ASSERT_TRUE(uploaded_report.ParseFromString(upload_data));

    // Et voila!
    EXPECT_EQ(
        RESULT_CODE_FAILED_TO_READ_UPLOAD_LOGS_FILE,
        static_cast<chrome_cleaner::ResultCode>(uploaded_report.exit_code()));

    // All succeeded, even though the log file could not be read.
    EXPECT_TRUE(done_callback_called_);
    EXPECT_TRUE(upload_success_);

    // And make sure the pending log that couldn't be read has been cleaned up.
    base::FilePath empty_log_file;
    registry_logger_->GetNextLogFilePath(&empty_log_file);
    EXPECT_TRUE(empty_log_file.empty());

    // And that there are no more task scheduled to try again.
    task_scheduler_.ExpectDeleteTaskCalled(true);
  }

  LoggingServiceAPI* logging_service_;

  registry_util::RegistryOverrideManager registry_override_manager_;
  std::unique_ptr<RegistryLogger> registry_logger_;

  // Needed for the current task runner to be available.
  base::test::TaskEnvironment task_environment_;
  // |done_callback_called_| is set to true in |LogsUploadCallback| to confirm
  // it was called appropriately.
  bool done_callback_called_;
  // Set with the value given to the done callback. Default to false.
  bool upload_success_;

  // Mocked TaskScheduler.
  TestTaskScheduler task_scheduler_;

  // Overridden settings with ExecutionMode::kCleanup.
  SettingsWithExecutionModeOverride cleanup_execution_mode_settings_;

  MockHttpAgentConfig http_agent_config_;
  std::unique_ptr<HttpAgentFactory> http_agent_factory_{
      std::make_unique<MockHttpAgentFactory>(&http_agent_config_)};
  MockNetworkChecker network_checker_;
};

TEST_F(PendingLogsServiceTest, SuccessfulRegistration) {
  ChromeCleanerReport chrome_cleaner_report;
  chrome_cleaner_report.set_exit_code(RESULT_CODE_NO_PUPS_FOUND);

  base::FilePath temp_log_file;
  PendingLogsService::ScheduleLogsUploadTask(
      TEST_PRODUCT_SHORTNAME_STRING, chrome_cleaner_report, &temp_log_file,
      registry_logger_.get());

  // The pending log should have been registered and a task scheduled to use it.
  base::FilePath log_file;
  registry_logger_->GetNextLogFilePath(&log_file);
  EXPECT_FALSE(log_file.empty());
  EXPECT_EQ(temp_log_file, log_file);

  task_scheduler_.ExpectRegisterTaskCalled(true);

  // Cleanup.
  EXPECT_FALSE(registry_logger_->RemoveLogFilePath(log_file));
  EXPECT_TRUE(base::DeleteFile(log_file, false));
}

TEST_F(PendingLogsServiceTest, FailToRegisterScheduledTask) {
  ChromeCleanerReport chrome_cleaner_report;
  chrome_cleaner_report.set_exit_code(RESULT_CODE_NO_PUPS_FOUND);

  // Make the scheduled task registration fail.
  task_scheduler_.SetRegisterTaskReturnValue(false);

  base::FilePath temp_log_file;
  PendingLogsService::ScheduleLogsUploadTask(
      TEST_PRODUCT_SHORTNAME_STRING, chrome_cleaner_report, &temp_log_file,
      registry_logger_.get());
  EXPECT_TRUE(temp_log_file.empty());

  // The temporary file should have been deleted (as validated by the temp file
  // watcher run from run_all_tests.bat) and not registered by the logger.
  base::FilePath log_file;
  registry_logger_->GetNextLogFilePath(&log_file);
  EXPECT_TRUE(log_file.empty());
}

TEST_F(PendingLogsServiceTest, UploadPendingLogs) {
  logging_service_->EnableUploads(true, registry_logger_.get());

  // Setup a simple non-empty report to send.
  ChromeCleanerReport raw_report;
  raw_report.set_exit_code(RESULT_CODE_NO_PUPS_FOUND);

  std::string raw_report_string;
  ASSERT_TRUE(raw_report.SerializeToString(&raw_report_string));

  // Save it on disk.
  base::FilePath log_file;
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(scoped_temp_dir.GetPath(), &log_file));
  ASSERT_TRUE(base::PathExists(log_file));
  ASSERT_GT(base::WriteFile(log_file, raw_report_string.c_str(),
                            raw_report_string.size()),
            0);

  // Set it up as a pending log.
  ASSERT_TRUE(registry_logger_->AppendLogFilePath(log_file));

  MockHttpAgentConfig::Calls calls(HttpStatus::kOk);
  const size_t calls_index = http_agent_config_.AddCalls(calls);

  // And now upload it.
  PendingLogsService pending_logs_service;
  base::RunLoop run_loop;
  pending_logs_service.RetryNextPendingLogsUpload(
      TEST_PRODUCT_SHORTNAME_STRING,
      base::BindRepeating(&PendingLogsServiceTest::LogsUploadCallback,
                          base::Unretained(this), run_loop.QuitClosure()),
      registry_logger_.get());
  run_loop.Run();

  ChromeCleanerReport uploaded_report;
  ASSERT_TRUE(uploaded_report.ParseFromString(
      http_agent_config_.request_data(calls_index).body));
  // There should have been one raw log line added to specify a retry.
  EXPECT_EQ(1, uploaded_report.raw_log_line_size());
  // We need to clear it for the reports to be the same.
  uploaded_report.clear_raw_log_line();
  std::string uploaded_report_string;
  ASSERT_TRUE(uploaded_report.SerializeToString(&uploaded_report_string));
  EXPECT_EQ(raw_report_string, uploaded_report_string);

  EXPECT_TRUE(done_callback_called_);
  EXPECT_TRUE(upload_success_);

  // And make sure the pending log has been cleaned up.
  EXPECT_FALSE(base::PathExists(log_file));
  base::FilePath empty_log_file;
  registry_logger_->GetNextLogFilePath(&empty_log_file);
  EXPECT_TRUE(empty_log_file.empty());
}

TEST_F(PendingLogsServiceTest, UploadPendingLogsFromNonexistentFile) {
  logging_service_->EnableUploads(true, registry_logger_.get());

  // Create a nonexistent file path.
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  // Only the folder was actually created, so Bla is always nonexistent.
  base::FilePath nonexistent_file(scoped_temp_dir.GetPath().Append(L"Bla"));
  ASSERT_FALSE(base::PathExists(nonexistent_file));

  ValidatePendingLogsUploadFailure(nonexistent_file);
}

TEST_F(PendingLogsServiceTest, UploadPendingLogsFromEmptyFile) {
  logging_service_->EnableUploads(true, registry_logger_.get());

  // Create an empty file.
  base::FilePath empty_file;
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(scoped_temp_dir.GetPath(), &empty_file));
  ASSERT_TRUE(base::PathExists(empty_file));

  ValidatePendingLogsUploadFailure(empty_file);
}

TEST_F(PendingLogsServiceTest, UploadPendingLogsFromInvalidFile) {
  logging_service_->EnableUploads(true, registry_logger_.get());

  // Create an invalid file.
  base::FilePath invalid_file;
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(scoped_temp_dir.GetPath(), &invalid_file));
  ASSERT_TRUE(base::PathExists(invalid_file));
  ASSERT_GT(base::WriteFile(invalid_file, "wat", 3), 0);

  ValidatePendingLogsUploadFailure(invalid_file);
}

}  // namespace chrome_cleaner
