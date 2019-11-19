// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/logging/reporter_logging_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "chrome/chrome_cleaner/http/http_agent_factory.h"
#include "chrome/chrome_cleaner/http/mock_http_agent_factory.h"
#include "chrome/chrome_cleaner/logging/proto/reporter_logs.pb.h"
#include "chrome/chrome_cleaner/logging/safe_browsing_reporter.h"
#include "chrome/chrome_cleaner/logging/test_utils.h"
#include "chrome/chrome_cleaner/pup_data/test_uws.h"
#include "chrome/chrome_cleaner/settings/settings.h"
#include "chrome/chrome_cleaner/test/test_settings_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

using ::testing::Return;
using ::testing::StrictMock;

void NoSleep(base::TimeDelta) {}

PUPData::PUP CreateSimpleDetectedPUP() {
  // Use a static signature object so that it will outlive any PUP object
  // pointing to it.
  static PUPData::UwSSignature signature{kGoogleTestAUwSID,
                                         PUPData::FLAGS_STATE_CONFIRMED_UWS,
                                         /*name=*/"This is nasty"};
  return PUPData::PUP(&signature);
}

}  // namespace

class ReporterLoggingServiceTest : public testing::Test {
 public:
  ReporterLoggingServiceTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  // LoggingServiceAPI::UploadResultCallback implementation.
  void LoggingServiceDone(base::OnceClosure run_loop_quit, bool success) {
    done_callback_called_ = true;
    upload_success_ = success;

    // A RunLoop will be waiting for this callback to run before proceeding
    // with the test. Now that the callback has run, we can quit the RunLoop.
    std::move(run_loop_quit).Run();
  }

  void SetUp() override {
    registry_logger_.reset(new RegistryLogger(RegistryLogger::Mode::REPORTER));

    reporter_logging_service_ = ReporterLoggingService::GetInstance();
    reporter_logging_service_->Initialize(registry_logger_.get());

    done_callback_called_ = false;
    upload_success_ = false;

    // Use a mock HttpAgent instead of making real network requests.
    SafeBrowsingReporter::SetHttpAgentFactoryForTesting(
        http_agent_factory_.get());
    SafeBrowsingReporter::SetSleepCallbackForTesting(
        base::BindRepeating(&NoSleep));
    SafeBrowsingReporter::SetNetworkCheckerForTesting(&network_checker_);
  }

  void TearDown() override { reporter_logging_service_->Terminate(); }

  // Sends logs to Safe Browsing, and waits for LoggingServiceDone to be called
  // before returning.
  void DoSendLogsToSafeBrowsing() {
    base::RunLoop run_loop;
    reporter_logging_service_->SendLogsToSafeBrowsing(
        base::BindRepeating(&ReporterLoggingServiceTest::LoggingServiceDone,
                            base::Unretained(this), run_loop.QuitClosure()),
        registry_logger_.get());
    run_loop.Run();
  }

  LoggingServiceAPI* reporter_logging_service_;

  // Needed for the current task runner to be available.
  base::test::TaskEnvironment task_environment_;

  // |done_callback_called_| is set to true in |LoggingServiceDone| to confirm
  // it was called appropriately.
  bool done_callback_called_;

  // Set with the value given to the done callback.
  bool upload_success_;

  std::unique_ptr<RegistryLogger> registry_logger_;

  MockHttpAgentConfig http_agent_config_;
  std::unique_ptr<HttpAgentFactory> http_agent_factory_{
      std::make_unique<MockHttpAgentFactory>(&http_agent_config_)};
  MockNetworkChecker network_checker_;
};

TEST_F(ReporterLoggingServiceTest, Disabled) {
  reporter_logging_service_->EnableUploads(false, registry_logger_.get());

  DoSendLogsToSafeBrowsing();

  // No URLRequest should have been made when not enabled.
  EXPECT_EQ(0UL, http_agent_config_.num_request_data());

  // But the done callback should have been called.
  EXPECT_TRUE(done_callback_called_);
  // With a failure.
  EXPECT_FALSE(upload_success_);
}

TEST_F(ReporterLoggingServiceTest, Empty) {
  reporter_logging_service_->EnableUploads(true, registry_logger_.get());

  DoSendLogsToSafeBrowsing();

  // No URLRequest should have been made when there is nothing to report.
  EXPECT_EQ(0UL, http_agent_config_.num_request_data());

  // But the done callback should have been called.
  EXPECT_TRUE(done_callback_called_);
  // With a failure.
  EXPECT_FALSE(upload_success_);
}

TEST_F(ReporterLoggingServiceTest, ExitCodeNotSet) {
  reporter_logging_service_->EnableUploads(true, registry_logger_.get());

  // Adding UwS data, but exit code is not set.
  PUPData::PUP pup = CreateSimpleDetectedPUP();

  reporter_logging_service_->AddDetectedUwS(&pup, kUwSDetectedFlagsNone);

  DoSendLogsToSafeBrowsing();

  // No URLRequest should have been made when the exit code has not been set.
  EXPECT_EQ(0UL, http_agent_config_.num_request_data());

  // But the done callback should have been called.
  EXPECT_TRUE(done_callback_called_);
  // With a failure.
  EXPECT_FALSE(upload_success_);
}

TEST_F(ReporterLoggingServiceTest, NoUwSFound) {
  reporter_logging_service_->EnableUploads(true, registry_logger_.get());

  reporter_logging_service_->SetExitCode(RESULT_CODE_NO_PUPS_FOUND);

  MockHttpAgentConfig::Calls calls(HttpStatus::kOk);
  ChromeFoilResponse response;
  response.SerializeToString(&calls.read_data_result);
  http_agent_config_.AddCalls(calls);

  DoSendLogsToSafeBrowsing();

  // No URLRequest should have been made when no UwS was found.
  EXPECT_EQ(0UL, http_agent_config_.num_request_data());

  // But the done callback should have been called.
  EXPECT_TRUE(done_callback_called_);
  // With a failure.
  EXPECT_FALSE(upload_success_);
}

TEST_F(ReporterLoggingServiceTest, BothExitCodeSetAndUwSDetected) {
  reporter_logging_service_->EnableUploads(true, registry_logger_.get());

  // Set the exit code and add detected UwS.
  reporter_logging_service_->SetExitCode(RESULT_CODE_EXAMINED_FOR_REMOVAL_ONLY);
  PUPData::PUP pup = CreateSimpleDetectedPUP();
  reporter_logging_service_->AddDetectedUwS(&pup, kUwSDetectedFlagsNone);

  MockHttpAgentConfig::Calls calls(HttpStatus::kOk);
  ChromeFoilResponse response;
  response.SerializeToString(&calls.read_data_result);
  http_agent_config_.AddCalls(calls);

  DoSendLogsToSafeBrowsing();

  // One URLRequest should have been made.
  EXPECT_EQ(1UL, http_agent_config_.num_request_data());

  // The done callback should have been called.
  EXPECT_TRUE(done_callback_called_);
  // With success.
  EXPECT_TRUE(upload_success_);

  std::string upload_data(http_agent_config_.request_data(0).body);
  FoilReporterLogs reporter_logs;
  ASSERT_TRUE(reporter_logs.ParseFromString(upload_data));
  EXPECT_EQ(RESULT_CODE_EXAMINED_FOR_REMOVAL_ONLY,
            static_cast<chrome_cleaner::ResultCode>(reporter_logs.exit_code()));

  ASSERT_EQ(1, reporter_logs.detected_uws_size());
  const UwS& uws = reporter_logs.detected_uws(0);
  EXPECT_EQ(pup.signature().id, uws.id());
  EXPECT_EQ(pup.signature().name, uws.name());
  EXPECT_EQ(UwS::REPORT_ONLY, uws.state());
  EXPECT_FALSE(uws.detail_level().only_one_footprint());

  // TODO Add tests for matched files, folders, registry entries,
  // and scheduled tasks, once the corresponding utility functions have been
  // moved from logging_service_unittest.cc to a shared lib.
}

TEST_F(ReporterLoggingServiceTest, AddDetectedUwS) {
  UwS uws;
  uws.set_id(kGoogleTestAUwSID);
  reporter_logging_service_->AddDetectedUwS(uws);

  FoilReporterLogs report;
  ASSERT_TRUE(
      report.ParseFromString(reporter_logging_service_->RawReportContent()));
  ASSERT_EQ(1, report.detected_uws_size());
  ASSERT_EQ(kGoogleTestAUwSID, report.detected_uws(0).id());
}

TEST_F(ReporterLoggingServiceTest, LogProcessInformation) {
  base::IoCounters io_counters;
  io_counters.ReadOperationCount = 1;
  io_counters.WriteOperationCount = 2;
  io_counters.OtherOperationCount = 3;
  io_counters.ReadTransferCount = 4;
  io_counters.WriteTransferCount = 5;
  io_counters.OtherTransferCount = 6;
  SystemResourceUsage usage = {io_counters, base::TimeDelta::FromSeconds(10),
                               base::TimeDelta::FromSeconds(20), 123456};

  StrictMock<MockSettings> mock_settings;
  EXPECT_CALL(mock_settings, engine()).WillOnce(Return(Engine::TEST_ONLY));
  Settings::SetInstanceForTesting(&mock_settings);
  base::ScopedClosureRunner restore_settings(
      base::BindOnce(&Settings::SetInstanceForTesting, nullptr));

  reporter_logging_service_->LogProcessInformation(SandboxType::kEngine, usage);

  FoilReporterLogs report;
  ASSERT_TRUE(
      report.ParseFromString(reporter_logging_service_->RawReportContent()));
  ASSERT_EQ(1, report.process_information_size());
  EXPECT_EQ(ProcessInformation::TEST_SANDBOX,
            report.process_information(0).process());

  ProcessInformation::SystemResourceUsage usage_msg =
      report.process_information(0).resource_usage();
  EXPECT_TRUE(IoCountersEqual(io_counters, usage_msg));
  EXPECT_EQ(10U, usage_msg.user_time());
  EXPECT_EQ(20U, usage_msg.kernel_time());
  EXPECT_EQ(123456U, usage_msg.peak_working_set_size());
}

TEST_F(ReporterLoggingServiceTest, SetFoundModifiedChromeShortcuts) {
  reporter_logging_service_->SetFoundModifiedChromeShortcuts(true);
  FoilReporterLogs report;
  ASSERT_TRUE(
      report.ParseFromString(reporter_logging_service_->RawReportContent()));
  EXPECT_TRUE(report.found_modified_chrome_shortcuts());
  reporter_logging_service_->SetFoundModifiedChromeShortcuts(false);
  ASSERT_TRUE(
      report.ParseFromString(reporter_logging_service_->RawReportContent()));
  EXPECT_FALSE(report.found_modified_chrome_shortcuts());
}

TEST_F(ReporterLoggingServiceTest, SetScannedLocations) {
  const std::vector<UwS::TraceLocation> locations = {
      UwS::FOUND_IN_STARTUP, UwS::FOUND_IN_SHELL, UwS::FOUND_IN_PROGRAMFILES};
  reporter_logging_service_->SetScannedLocations(locations);

  FoilReporterLogs report;
  ASSERT_TRUE(
      report.ParseFromString(reporter_logging_service_->RawReportContent()));
  EXPECT_THAT(report.scanned_locations(), testing::ElementsAreArray(locations));
}

}  // namespace chrome_cleaner
