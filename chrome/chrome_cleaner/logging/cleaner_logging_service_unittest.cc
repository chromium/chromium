// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/logging/cleaner_logging_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "chrome/chrome_cleaner/http/http_agent_factory.h"
#include "chrome/chrome_cleaner/http/mock_http_agent_factory.h"
#include "chrome/chrome_cleaner/logging/pending_logs_service.h"
#include "chrome/chrome_cleaner/logging/proto/chrome_cleaner_report.pb.h"
#include "chrome/chrome_cleaner/logging/proto/removal_status.pb.h"
#include "chrome/chrome_cleaner/logging/registry_logger.h"
#include "chrome/chrome_cleaner/logging/safe_browsing_reporter.h"
#include "chrome/chrome_cleaner/logging/test_utils.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/file_removal_status_updater.h"
#include "chrome/chrome_cleaner/os/registry_util.h"
#include "chrome/chrome_cleaner/os/task_scheduler.h"
#include "chrome/chrome_cleaner/proto/shared_pup_enums.pb.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "chrome/chrome_cleaner/pup_data/test_uws.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_settings_util.h"
#include "chrome/chrome_cleaner/test/test_task_scheduler.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "components/chrome_cleaner/test/test_name_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace chrome_cleaner {

namespace {

using testing::_;
using testing::Return;

const internal::FileInformation kFileInformation1(L"some/path/something.tmp",
                                                  "3/1/2016",
                                                  "3/3/2016",
                                                  "somedigest1234",
                                                  9876,
                                                  L"Company Name",
                                                  L"CNShort",
                                                  L"Product Name",
                                                  L"PNShort",
                                                  L"Internal Name",
                                                  L"Something_Original.tmp",
                                                  L"Very descriptive",
                                                  L"42.1.2");

const internal::FileInformation kFileInformation2(
    L"some/path/somethingelse.tmp",
    "2/1/2016",
    "2/3/2016",
    "someotherdigest1234",
    6543,
    L"Company Name 2",
    L"CN2Short",
    L"Product Name2",
    L"PN2Short",
    L"Internal Name2",
    L"Something_Original2.tmp",
    L"Very descriptive2",
    L"43.2.3");

const wchar_t kMatchedFileToStringExpectedString[] =
    L"path = 'some/path/something.tmp', file_creation_date = "
    L"'3/1/2016', file_last_modified_date = '3/3/2016', digest = "
    L"'somedigest1234', size = '9876', company_name = 'Company Name', "
    L"company_short_name = 'CNShort', product_name = 'Product Name', "
    L"product_short_name = 'PNShort', internal_name = 'Internal Name', "
    L"original_filename = 'Something_Original.tmp', file_description = 'Very "
    L"descriptive', file_version = '42.1.2', active_file = '0', removal_status "
    L"= 0, quarantine_status = 0";

const wchar_t kMatchedRegistryEntryKey[] = L"123";
const wchar_t kMatchedRegistryEntryValueName[] = L"Value Name";
const wchar_t kMatchedRegistryEntryValueSubstring[] = L"Value Substring";
const wchar_t kMatchedRegistryEntryToStringExpectedString[] =
    L"123\\Value Name Value Substring";
const internal::RegistryValue kRegistryValue1 = {
    L"HKCU\\something\\that\\exists", L"ValueName", L"Some content"};
const internal::RegistryValue kRegistryValue2 = {
    L"HKCU\\something\\else", L"ValueName2", L"Some other content"};
const wchar_t kSystemProxySettingsConfig[] = L"http://someconfigurl.com/hello";
const wchar_t kSystemProxySettingsBypass[] = L"http://somebypassurl.com/hello";

const char kFileContent1[] = "This is the file content.";

constexpr PUPData::UwSSignature kMatchedUwSSignature{
    kGoogleTestAUwSID, PUPData::FLAGS_NONE, "Observed/matched_uws"};

constexpr PUPData::UwSSignature kRemovedUwSSignature{
    kGoogleTestBUwSID, PUPData::FLAGS_ACTION_REMOVE, "Removed/removed_uws"};

constexpr PUPData::UwSSignature kMatchedUwSSlowSignature{
    kGoogleTestCUwSID, PUPData::FLAGS_NONE, "Observed/matched_uws_slow_"};

void CompareRegistryEntries(
    const std::vector<PUPData::RegistryFootprint>& expanded_registry_footprints,
    const ::google::protobuf::RepeatedPtrField<MatchedRegistryEntry>&
        registry_entries) {
  ASSERT_EQ(expanded_registry_footprints.size(),
            static_cast<size_t>(registry_entries.size()));
  for (unsigned int i = 0; i < expanded_registry_footprints.size(); ++i) {
    EXPECT_EQ(
        base::WideToUTF8(expanded_registry_footprints[i].key_path.FullPath()),
        registry_entries.Get(i).key_path());
    EXPECT_EQ(base::WideToUTF8(expanded_registry_footprints[i].value_name),
              registry_entries.Get(i).value_name());
    EXPECT_EQ(base::WideToUTF8(expanded_registry_footprints[i].value_substring),
              registry_entries.Get(i).value_substring());
  }
}

void CompareUwSData(const PUPData::PUP& uws, const UwS& log_uws) {
  EXPECT_EQ(uws.signature().name, log_uws.name());
  EXPECT_EQ(uws.signature().id, log_uws.id());

  ASSERT_EQ(uws.expanded_disk_footprints.size(),
            static_cast<size_t>(log_uws.files_size() + log_uws.folders_size()));
  int files_index = 0;
  int folders_index = 0;
  for (const auto& file_path : uws.expanded_disk_footprints.file_paths()) {
    // The path will be sanitized in the logs.
    std::string sanitized_path = base::UTF16ToUTF8(SanitizePath(file_path));
    if (base::DirectoryExists(file_path)) {
      ASSERT_TRUE(folders_index < log_uws.folders_size());
      EXPECT_EQ(sanitized_path,
                log_uws.folders(folders_index).folder_information().path());
      ++folders_index;
    } else {
      ASSERT_TRUE(files_index < log_uws.files_size());
      EXPECT_EQ(sanitized_path,
                log_uws.files(files_index).file_information().path());
      ++files_index;
    }
  }

  EXPECT_EQ(log_uws.folders_size(), folders_index);
  EXPECT_EQ(log_uws.files_size(), files_index);

  CompareRegistryEntries(uws.expanded_registry_footprints,
                         log_uws.registry_entries());

  ASSERT_EQ(uws.expanded_scheduled_tasks.size(),
            static_cast<size_t>(log_uws.scheduled_tasks_size()));
  for (unsigned int i = 0; i < uws.expanded_scheduled_tasks.size(); ++i) {
    EXPECT_EQ(base::WideToUTF8(uws.expanded_scheduled_tasks[i]),
              log_uws.scheduled_tasks(i).scheduled_task().name());
  }
}

void ExpectFileInformationEqualToProtoObj(
    const internal::FileInformation& file_information,
    const FileInformation& proto_file_information) {
  EXPECT_EQ(proto_file_information.path(),
            base::UTF16ToUTF8(file_information.path));
  EXPECT_EQ(proto_file_information.creation_date(),
            file_information.creation_date);
  EXPECT_EQ(proto_file_information.last_modified_date(),
            file_information.last_modified_date);
  EXPECT_EQ(proto_file_information.sha256(), file_information.sha256);
  EXPECT_EQ(proto_file_information.size(), file_information.size);
  EXPECT_EQ(proto_file_information.company_name(),
            base::UTF16ToUTF8(file_information.company_name));
  EXPECT_EQ(proto_file_information.company_short_name(),
            base::UTF16ToUTF8(file_information.company_short_name));
  EXPECT_EQ(proto_file_information.product_name(),
            base::UTF16ToUTF8(file_information.product_name));
  EXPECT_EQ(proto_file_information.product_short_name(),
            base::UTF16ToUTF8(file_information.product_short_name));
  EXPECT_EQ(proto_file_information.internal_name(),
            base::UTF16ToUTF8(file_information.internal_name));
  EXPECT_EQ(proto_file_information.original_filename(),
            base::UTF16ToUTF8(file_information.original_filename));
  EXPECT_EQ(proto_file_information.file_description(),
            base::UTF16ToUTF8(file_information.file_description));
  EXPECT_EQ(proto_file_information.file_version(),
            base::UTF16ToUTF8(file_information.file_version));
  EXPECT_EQ(proto_file_information.active_file(), file_information.active_file);
}

void ExpectRegistryValueEqualToProtoObj(
    const internal::RegistryValue& registry_value,
    const RegistryValue& proto_registry_value) {
  EXPECT_EQ(proto_registry_value.key_path(),
            base::UTF16ToUTF8(registry_value.key_path));
  EXPECT_EQ(proto_registry_value.value_name(),
            base::UTF16ToUTF8(registry_value.value_name));
  EXPECT_EQ(proto_registry_value.data(),
            base::UTF16ToUTF8(registry_value.data));
}

void NoSleep(base::TimeDelta) {}

}  // namespace

// Test parametrized with the execution mode.
class CleanerLoggingServiceTest : public testing::TestWithParam<ExecutionMode> {
 public:
  // LoggingServiceAPI::UploadResultCallback implementation.
  void LoggingServiceDone(base::OnceClosure run_loop_quit, bool success) {
    done_callback_called_ = true;
    upload_success_ = success;

    // A RunLoop will be waiting for this callback to run before proceeding
    // with the test. Now that the callback has run, we can quit the RunLoop.
    std::move(run_loop_quit).Run();
  }

  void SetUp() override {
    overriden_settings_ =
        std::make_unique<SettingsWithExecutionModeOverride>(GetParam());
    Settings::SetInstanceForTesting(overriden_settings_.get());

    registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER);
    // The registry logger must be created after calling OverrideRegistry.
    registry_logger_.reset(new RegistryLogger(RegistryLogger::Mode::REMOVER));

    // By default, tests use the NoOpLoggingService, so individual tests that
    // need logging need to enable it.
    logging_service_ = CleanerLoggingService::GetInstance();
    logging_service_->Initialize(registry_logger_.get());

    ASSERT_TRUE(uws_dir_.CreateUniqueTempDir());

    // Add a folder for each UwS.
    base::FilePath matched_uws_folder;
    base::CreateTemporaryDirInDir(uws_dir_.GetPath(), L"matched_uws",
                                  &matched_uws_folder);
    base::FilePath removed_uws_folder;
    base::CreateTemporaryDirInDir(uws_dir_.GetPath(), L"removed_uws",
                                  &removed_uws_folder);
    base::FilePath matched_uws_slow_folder;
    base::CreateTemporaryDirInDir(uws_dir_.GetPath(), L"matched_uws_slow",
                                  &matched_uws_slow_folder);
    matched_uws_.AddDiskFootprint(matched_uws_folder);
    removed_uws_.AddDiskFootprint(removed_uws_folder);
    matched_uws_slow_.AddDiskFootprint(matched_uws_slow_folder);

    // Add a file for each UwS.
    matched_uws_file_ = matched_uws_folder.Append(L"matched_uws.tmp");
    matched_uws_.AddDiskFootprint(matched_uws_file_);
    removed_uws_file_ = removed_uws_folder.Append(L"removed_uws.tmp");
    removed_uws_.AddDiskFootprint(removed_uws_file_);
    matched_uws_slow_file_ =
        matched_uws_slow_folder.Append(L"matched_uws_slow_.tmp");
    matched_uws_slow_.AddDiskFootprint(matched_uws_slow_file_);

    // Add a registry entry for each UwS.
    RegKeyPath reg_path(HKEY_LOCAL_MACHINE, L"software\\test\\uws");
    PUPData::RegistryFootprint reg(reg_path, L"matched_uws", L"1",
                                   REGISTRY_VALUE_MATCH_KEY);
    matched_uws_.expanded_registry_footprints.push_back(reg);

    reg = PUPData::RegistryFootprint(reg_path, L"removed_uws", L"2",
                                     REGISTRY_VALUE_MATCH_KEY);
    removed_uws_.expanded_registry_footprints.push_back(reg);

    reg = PUPData::RegistryFootprint(reg_path, L"matched_uws_slow", L"3",
                                     REGISTRY_VALUE_MATCH_KEY);
    matched_uws_slow_.expanded_registry_footprints.push_back(reg);

    // Add dummy expanded scheduled tasks to one UwS.
    matched_uws_.expanded_scheduled_tasks.push_back(L"ScheduledTask1");
    matched_uws_.expanded_scheduled_tasks.push_back(L"ScheduledTask2");

    // Use a mock HttpAgent instead of making real network requests.
    SafeBrowsingReporter::SetHttpAgentFactoryForTesting(
        http_agent_factory_.get());
    SafeBrowsingReporter::SetSleepCallbackForTesting(
        base::BindRepeating(&NoSleep));
    SafeBrowsingReporter::SetNetworkCheckerForTesting(&network_checker_);
  }

  void TearDown() override {
    // Disable logs uploading to delete any pending logs data.
    logging_service_->EnableUploads(false, registry_logger_.get());

    SafeBrowsingReporter::SetNetworkCheckerForTesting(nullptr);
    SafeBrowsingReporter::SetHttpAgentFactoryForTesting(nullptr);

    logging_service_->Terminate();
    registry_logger_.reset();

    Settings::SetInstanceForTesting(nullptr);
  }

 protected:
  // Sends logs to Safe Browsing, and waits for LoggingServiceDone to be called
  // before returning.
  void DoSendLogsToSafeBrowsing() {
    base::RunLoop run_loop;
    logging_service_->SendLogsToSafeBrowsing(
        base::BindRepeating(&CleanerLoggingServiceTest::LoggingServiceDone,
                            base::Unretained(this), run_loop.QuitClosure()),
        registry_logger_.get());
    run_loop.Run();
  }

  CleanerLoggingServiceTest()
      : logging_service_(nullptr),
        task_environment_(base::test::TaskEnvironment::MainThreadType::UI),
        done_callback_called_(false),
        upload_success_(false),
        matched_uws_(&kMatchedUwSSignature),
        removed_uws_(&kRemovedUwSSignature),
        matched_uws_slow_(&kMatchedUwSSlowSignature) {}

  // This is a pointer to the singleton object which we don't own.
  CleanerLoggingService* logging_service_;

  registry_util::RegistryOverrideManager registry_override_manager_;
  std::unique_ptr<RegistryLogger> registry_logger_;

  // Needed for the current task runner to be available.
  base::test::TaskEnvironment task_environment_;

  // |done_callback_called_| is set to true in |LoggingServiceDone| to confirm
  // it was called appropriately.
  bool done_callback_called_;
  // Set with the value given to the done callback. Default to false.
  bool upload_success_;

  // UwS data for various UwS that should be logged.
  PUPData::PUP matched_uws_;
  PUPData::PUP removed_uws_;
  PUPData::PUP matched_uws_slow_;
  base::FilePath matched_uws_file_;
  base::FilePath removed_uws_file_;
  base::FilePath matched_uws_slow_file_;

  // A temporary dir to store some temp UwS files.
  base::ScopedTempDir uws_dir_;

  // Mocked TaskScheduler.
  TestTaskScheduler task_scheduler_;

  // Overridden settings with the ExecutionMode test param.
  std::unique_ptr<SettingsWithExecutionModeOverride> overriden_settings_;

  MockHttpAgentConfig http_agent_config_;
  std::unique_ptr<HttpAgentFactory> http_agent_factory_{
      std::make_unique<MockHttpAgentFactory>(&http_agent_config_)};
  MockNetworkChecker network_checker_;
};

TEST(LoggingServiceUtilTest, GetCleanerStartupFromCommandLine) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);

  EXPECT_EQ(ChromeCleanerReport::CLEANER_STARTUP_NOT_PROMPTED,
            GetCleanerStartupFromCommandLine(&command_line));

  // Check that cleaner_startup is set to "unknown" when the chrome prompt
  // switch is set, but has no value,
  command_line.AppendSwitch(kChromePromptSwitch);
  EXPECT_EQ(ChromeCleanerReport::CLEANER_STARTUP_UNKNOWN,
            GetCleanerStartupFromCommandLine(&command_line));

  // Check that cleaner_startup is set to "unknown" when the chrome prompt
  // switch has an invalid non-integer value.
  command_line.InitFromArgv(command_line.argv());
  command_line.AppendSwitchASCII(kChromePromptSwitch, "blah");
  EXPECT_EQ(ChromeCleanerReport::CLEANER_STARTUP_UNKNOWN,
            GetCleanerStartupFromCommandLine(&command_line));

  // Check that cleaner_startup is set according to the value of the chrome
  // prompt switch when the integer is a valid CleanerStartup value.
  for (int flag_value = 0;
       flag_value <= ChromeCleanerReport::CleanerStartup_MAX; ++flag_value) {
    command_line.InitFromArgv(command_line.argv());
    command_line.AppendSwitchASCII(kChromePromptSwitch,
                                   base::NumberToString(flag_value));
    if (flag_value == ChromeCleanerReport::CLEANER_STARTUP_UNSPECIFIED ||
        flag_value == ChromeCleanerReport::CLEANER_STARTUP_NOT_PROMPTED) {
      EXPECT_EQ(ChromeCleanerReport::CLEANER_STARTUP_UNKNOWN,
                GetCleanerStartupFromCommandLine(&command_line));
    } else {
      EXPECT_EQ(static_cast<ChromeCleanerReport::CleanerStartup>(flag_value),
                GetCleanerStartupFromCommandLine(&command_line));
    }
  }
}

TEST_P(CleanerLoggingServiceTest, Disabled) {
  logging_service_->EnableUploads(false, registry_logger_.get());

  DoSendLogsToSafeBrowsing();

  // No URLRequest should have been made when not enabled.
  EXPECT_EQ(0UL, http_agent_config_.num_request_data());

  // But the done callback should have been called.
  EXPECT_TRUE(done_callback_called_);
  // With a failure.
  EXPECT_FALSE(upload_success_);
  // That should NOT have registered a retry.
  task_scheduler_.ExpectRegisterTaskCalled(false);
}

TEST_P(CleanerLoggingServiceTest, Empty) {
  logging_service_->EnableUploads(true, registry_logger_.get());

  DoSendLogsToSafeBrowsing();

  // No URLRequest should have been made when there is nothing to report.
  EXPECT_EQ(0UL, http_agent_config_.num_request_data());

  // But the done callback should have been called.
  EXPECT_TRUE(done_callback_called_);
  // With a failure.
  EXPECT_FALSE(upload_success_);
  // That should NOT have registered a retry.
  task_scheduler_.ExpectRegisterTaskCalled(false);
}

TEST_P(CleanerLoggingServiceTest, OnlyUwS) {
  CreateFileWithContent(matched_uws_file_, kFileContent1,
                        sizeof(kFileContent1));

  logging_service_->EnableUploads(true, registry_logger_.get());

  logging_service_->AddFoundUwS(matched_uws_.signature().name);

  logging_service_->AddDetectedUwS(&matched_uws_, kUwSDetectedFlagsNone);

  MockHttpAgentConfig::Calls calls(HttpStatus::kOk);
  http_agent_config_.AddCalls(calls);

  DoSendLogsToSafeBrowsing();

  // A URLRequest should have been made even if there's just UwS to report.
  ASSERT_NE(0UL, http_agent_config_.num_request_data());

  std::string upload_data(http_agent_config_.request_data(0).body);
  ChromeCleanerReport uploaded_report;
  ASSERT_TRUE(uploaded_report.ParseFromString(upload_data));
  ASSERT_TRUE(uploaded_report.intermediate_log());
  ASSERT_EQ(1, uploaded_report.found_uws_size());
  EXPECT_EQ(matched_uws_.signature().name, uploaded_report.found_uws(0));
  ASSERT_EQ(1, uploaded_report.detected_uws_size());
  CompareUwSData(matched_uws_, uploaded_report.detected_uws(0));
  EXPECT_FALSE(
      uploaded_report.detected_uws(0).detail_level().only_one_footprint());

  EXPECT_TRUE(done_callback_called_);
  EXPECT_TRUE(upload_success_);
  // A safety task should have been registered.
  task_scheduler_.ExpectRegisterTaskCalled(true);
  // And then deleted.
  task_scheduler_.ExpectDeleteTaskCalled(true);
  // So none should be left.
  task_scheduler_.ExpectRegisteredTasksSize(0U);
}

TEST_P(CleanerLoggingServiceTest, FullContent) {
  CreateFileWithContent(matched_uws_file_, kFileContent1,
                        sizeof(kFileContent1));
  CreateFileWithContent(removed_uws_file_, kFileContent1,
                        sizeof(kFileContent1));
  CreateFileWithContent(matched_uws_slow_file_, kFileContent1,
                        sizeof(kFileContent1));
  logging_service_->EnableUploads(true, registry_logger_.get());

  std::string log_line1("A log line");
  std::string log_line2("Another log line");
  LOG(INFO) << log_line1;
  LOG(WARNING) << log_line2;

  logging_service_->SetExitCode(RESULT_CODE_NO_PUPS_FOUND);
  logging_service_->AddFoundUwS(matched_uws_.signature().name);
  logging_service_->AddDetectedUwS(&matched_uws_, kUwSDetectedFlagsNone);

  logging_service_->AddFoundUwS(removed_uws_.signature().name);
  logging_service_->AddDetectedUwS(&removed_uws_,
                                   kUwSDetectedFlagsOnlyOneFootprint);

  logging_service_->AddFoundUwS(matched_uws_slow_.signature().name);
  logging_service_->AddDetectedUwS(&matched_uws_slow_,
                                   kUwSDetectedFlagsOnlyOneFootprint);

  MockHttpAgentConfig::Calls calls(HttpStatus::kOk);
  ChromeFoilResponse response;
  response.SerializeToString(&calls.read_data_result);
  http_agent_config_.AddCalls(calls);

  DoSendLogsToSafeBrowsing();

  std::string upload_data(http_agent_config_.request_data(0).body);
  ChromeCleanerReport uploaded_report;
  ASSERT_TRUE(uploaded_report.ParseFromString(upload_data));

  EXPECT_EQ(RESULT_CODE_NO_PUPS_FOUND, static_cast<chrome_cleaner::ResultCode>(
                                           uploaded_report.exit_code()));

  ASSERT_EQ(3, uploaded_report.found_uws_size());
  EXPECT_EQ(matched_uws_.signature().name, uploaded_report.found_uws(0));
  EXPECT_EQ(removed_uws_.signature().name, uploaded_report.found_uws(1));
  EXPECT_EQ(matched_uws_slow_.signature().name, uploaded_report.found_uws(2));

  ASSERT_EQ(3, uploaded_report.detected_uws_size());
  CompareUwSData(matched_uws_, uploaded_report.detected_uws(0));
  EXPECT_FALSE(
      uploaded_report.detected_uws(0).detail_level().only_one_footprint());
  EXPECT_EQ(UwS::REPORT_ONLY, uploaded_report.detected_uws(0).state());
  CompareUwSData(removed_uws_, uploaded_report.detected_uws(1));
  EXPECT_TRUE(
      uploaded_report.detected_uws(1).detail_level().only_one_footprint());
  EXPECT_EQ(UwS::REMOVABLE, uploaded_report.detected_uws(1).state());
  CompareUwSData(matched_uws_slow_, uploaded_report.detected_uws(2));
  EXPECT_TRUE(
      uploaded_report.detected_uws(2).detail_level().only_one_footprint());
  EXPECT_EQ(UwS::REPORT_ONLY, uploaded_report.detected_uws(2).state());

  ASSERT_EQ(2, uploaded_report.raw_log_line_size());
  // Log lines include the \n char.
  log_line1 += "\n";
  log_line2 += "\n";
  EXPECT_TRUE(base::EndsWith(uploaded_report.raw_log_line(0), log_line1,
                             base::CompareCase::SENSITIVE));
  EXPECT_TRUE(base::EndsWith(uploaded_report.raw_log_line(1), log_line2,
                             base::CompareCase::SENSITIVE));

  EXPECT_TRUE(done_callback_called_);
  EXPECT_TRUE(upload_success_);
  // A safety task should have been registered.
  task_scheduler_.ExpectRegisterTaskCalled(true);
  // And then deleted.
  task_scheduler_.ExpectDeleteTaskCalled(true);
  // So none should be left.
  task_scheduler_.ExpectRegisteredTasksSize(0U);
}

TEST_P(CleanerLoggingServiceTest, EnableDisableChanges) {
  // Start enable, and then disable, and then enable again.
  logging_service_->EnableUploads(true, registry_logger_.get());

  std::string log_line1("Visible log line");
  LOG(INFO) << log_line1;

  logging_service_->EnableUploads(false, registry_logger_.get());

  std::string log_line2("Visible log line with uploads off");
  LOG(INFO) << log_line2;

  // Logs shouldn't be sent since uploads are disabled.
  DoSendLogsToSafeBrowsing();
  EXPECT_TRUE(done_callback_called_);
  EXPECT_FALSE(upload_success_);

  logging_service_->EnableUploads(true, registry_logger_.get());

  std::string log_line3("Another visible log line");
  LOG(INFO) << log_line3;

  MockHttpAgentConfig::Calls calls(HttpStatus::kOk);
  ChromeFoilResponse response;
  response.SerializeToString(&calls.read_data_result);
  http_agent_config_.AddCalls(calls);

  // Reset done_callback before attempting to upload again.
  done_callback_called_ = false;
  DoSendLogsToSafeBrowsing();

  std::string upload_data(http_agent_config_.request_data(0).body);
  ChromeCleanerReport uploaded_report;
  ASSERT_TRUE(uploaded_report.ParseFromString(upload_data));

  ASSERT_EQ(3, uploaded_report.raw_log_line_size());
  // Log lines include the \n char.
  log_line1 += "\n";
  log_line2 += "\n";
  log_line3 += "\n";
  EXPECT_TRUE(base::EndsWith(uploaded_report.raw_log_line(0), log_line1,
                             base::CompareCase::SENSITIVE));
  EXPECT_TRUE(base::EndsWith(uploaded_report.raw_log_line(1), log_line2,
                             base::CompareCase::SENSITIVE));
  EXPECT_TRUE(base::EndsWith(uploaded_report.raw_log_line(2), log_line3,
                             base::CompareCase::SENSITIVE));

  EXPECT_TRUE(done_callback_called_);
  EXPECT_TRUE(upload_success_);
  // A safety task should have been registered.
  task_scheduler_.ExpectRegisterTaskCalled(true);
  // And then deleted.
  task_scheduler_.ExpectDeleteTaskCalled(true);
  // So none should be left.
  task_scheduler_.ExpectRegisteredTasksSize(0U);
}

TEST_P(CleanerLoggingServiceTest, FailedAndRetry) {
  logging_service_->EnableUploads(true, registry_logger_.get());

  // First request will fail.
  MockHttpAgentConfig::Calls calls(HttpStatus::kBadRequest);
  http_agent_config_.AddCalls(calls);

  // Second request will succeed.
  calls.get_status_code_result = HttpStatus::kOk;
  http_agent_config_.AddCalls(calls);

  logging_service_->SetExitCode(RESULT_CODE_NO_PUPS_FOUND);
  DoSendLogsToSafeBrowsing();

  // Now the done callback should have been called.
  EXPECT_TRUE(done_callback_called_);
  EXPECT_TRUE(upload_success_);

  // A safety task should have been registered.
  task_scheduler_.ExpectRegisterTaskCalled(true);
  // And then deleted.
  task_scheduler_.ExpectDeleteTaskCalled(true);
  // So none should be left.
  task_scheduler_.ExpectRegisteredTasksSize(0U);

  // And no more retries should be done.
  EXPECT_EQ(2UL, http_agent_config_.num_request_data());
}

TEST_P(CleanerLoggingServiceTest, CompleteFailure) {
  logging_service_->EnableUploads(true, registry_logger_.get());

  MockHttpAgentConfig::Calls calls(HttpStatus::kBadRequest);
  http_agent_config_.AddCalls(calls);
  http_agent_config_.AddCalls(calls);
  http_agent_config_.AddCalls(calls);

  logging_service_->SetExitCode(RESULT_CODE_NO_PUPS_FOUND);
  DoSendLogsToSafeBrowsing();

  // Now the done callback should have been called.
  EXPECT_TRUE(done_callback_called_);
  EXPECT_FALSE(upload_success_);

  // A safety task should have been registered.
  task_scheduler_.ExpectRegisterTaskCalled(true);
  // The safety task should not have been deleted this time.
  task_scheduler_.ExpectDeleteTaskCalled(false);
  // So task should still be registered.
  task_scheduler_.ExpectRegisteredTasksSize(1U);

  // And no more retries should be done.
  EXPECT_EQ(3UL, http_agent_config_.num_request_data());
  base::FilePath log_file;

  // Now forget about the scheduled logs upload retry.
  registry_logger_->GetNextLogFilePath(&log_file);
  ASSERT_FALSE(log_file.empty());
  bool success = base::DeleteFile(log_file, false);
  EXPECT_TRUE(success) << "Failed to delete " << log_file.value();
  bool more_log_files = registry_logger_->RemoveLogFilePath(log_file);
  EXPECT_FALSE(more_log_files);
  task_scheduler_.DeleteTask(L"");
}

TEST_P(CleanerLoggingServiceTest, RawReportContent) {
  ChromeCleanerReport raw_report;
  // Start with the current state of the LoggingService which adds stuff
  // initially like the Environment object.
  ASSERT_TRUE(raw_report.ParseFromString(logging_service_->RawReportContent()));

  logging_service_->SetExitCode(RESULT_CODE_NO_PUPS_FOUND);
  raw_report.set_exit_code(RESULT_CODE_NO_PUPS_FOUND);
  raw_report.set_intermediate_log(false);

  logging_service_->AddFoundUwS(matched_uws_.signature().name);
  raw_report.add_found_uws(matched_uws_.signature().name);
  logging_service_->AddFoundUwS(removed_uws_.signature().name);
  raw_report.add_found_uws(removed_uws_.signature().name);

  ChromeCleanerReport report;
  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  EXPECT_EQ(raw_report.SerializeAsString(), report.SerializeAsString());
}

TEST_P(CleanerLoggingServiceTest, ReadContentFromFile) {
  ChromeCleanerReport raw_report;

  raw_report.set_exit_code(RESULT_CODE_NO_PUPS_FOUND);
  raw_report.add_found_uws("UwS-42");
  raw_report.add_found_uws("UwS-trio");

  std::string raw_report_string;
  ASSERT_TRUE(raw_report.SerializeToString(&raw_report_string));

  base::FilePath test_file;
  base::ScopedTempDir test_dir;
  ASSERT_TRUE(test_dir.CreateUniqueTempDir());
  base::CreateTemporaryFileInDir(test_dir.GetPath(), &test_file);
  ASSERT_TRUE(base::PathExists(test_file));
  ASSERT_GT(base::WriteFile(test_file, raw_report_string.c_str(),
                            raw_report_string.size()),
            0);

  EXPECT_TRUE(logging_service_->ReadContentFromFile(test_file));

  std::string report_string(logging_service_->RawReportContent());
  EXPECT_EQ(raw_report_string, report_string);
}

TEST_P(CleanerLoggingServiceTest, ReadContentFromNonexistentFile) {
  base::ScopedTempDir test_dir;
  ASSERT_TRUE(test_dir.CreateUniqueTempDir());
  // Only the folder was actually created, so Bla is always nonexistent.
  base::FilePath nonexistent_file(test_dir.GetPath().Append(L"Bla"));
  ASSERT_FALSE(base::PathExists(nonexistent_file));

  EXPECT_FALSE(logging_service_->ReadContentFromFile(nonexistent_file));
}

TEST_P(CleanerLoggingServiceTest, ReadContentFromEmptyFile) {
  base::FilePath test_file;
  base::ScopedTempDir test_dir;
  ASSERT_TRUE(test_dir.CreateUniqueTempDir());
  base::CreateTemporaryFileInDir(test_dir.GetPath(), &test_file);
  ASSERT_TRUE(base::PathExists(test_file));

  EXPECT_FALSE(logging_service_->ReadContentFromFile(test_file));
}

TEST_P(CleanerLoggingServiceTest, ReadContentFromInvalidFile) {
  base::FilePath test_file;
  base::ScopedTempDir test_dir;
  ASSERT_TRUE(test_dir.CreateUniqueTempDir());
  base::CreateTemporaryFileInDir(test_dir.GetPath(), &test_file);
  ASSERT_TRUE(base::PathExists(test_file));
  ASSERT_GT(base::WriteFile(test_file, "bla", 3), 0);

  EXPECT_FALSE(logging_service_->ReadContentFromFile(test_file));
}

TEST_P(CleanerLoggingServiceTest, ScheduleFallbackLogsUpload) {
  // Shouldn't schedule a task if uploads are not enabled.
  logging_service_->ScheduleFallbackLogsUpload(registry_logger_.get(),
                                               RESULT_CODE_SUCCESS);

  task_scheduler_.ExpectRegisterTaskCalled(false);

  logging_service_->EnableUploads(true, registry_logger_.get());
  logging_service_->ScheduleFallbackLogsUpload(registry_logger_.get(),
                                               RESULT_CODE_SUCCESS);

  task_scheduler_.ExpectRegisterTaskCalled(true);
  // The safety task should not have been deleted.
  task_scheduler_.ExpectDeleteTaskCalled(false);
  // A single task should still be registered.
  task_scheduler_.ExpectRegisteredTasksSize(1U);

  // Also make sure that opting out of logs upload will clear any pending logs
  // upload.
  logging_service_->EnableUploads(false, registry_logger_.get());
  task_scheduler_.ExpectDeleteTaskCalled(true);
  // No task should be registered anymore.
  task_scheduler_.ExpectRegisteredTasksSize(0U);
}

TEST_P(CleanerLoggingServiceTest, AppendMatchedFile) {
  MatchedFile matched_file;
  FileInformation* proto_file_information =
      matched_file.mutable_file_information();
  proto_file_information->set_path(base::UTF16ToUTF8(kFileInformation1.path));
  proto_file_information->set_creation_date(kFileInformation1.creation_date);
  proto_file_information->set_last_modified_date(
      kFileInformation1.last_modified_date);
  proto_file_information->set_sha256(kFileInformation1.sha256);
  proto_file_information->set_size(kFileInformation1.size);
  proto_file_information->set_company_name(
      base::UTF16ToUTF8(kFileInformation1.company_name));
  proto_file_information->set_company_short_name(
      base::UTF16ToUTF8(kFileInformation1.company_short_name));
  proto_file_information->set_product_name(
      base::UTF16ToUTF8(kFileInformation1.product_name));
  proto_file_information->set_product_short_name(
      base::UTF16ToUTF8(kFileInformation1.product_short_name));
  proto_file_information->set_internal_name(
      base::UTF16ToUTF8(kFileInformation1.internal_name));
  proto_file_information->set_original_filename(
      base::UTF16ToUTF8(kFileInformation1.original_filename));
  proto_file_information->set_file_description(
      base::UTF16ToUTF8(kFileInformation1.file_description));
  proto_file_information->set_file_version(
      base::UTF16ToUTF8(kFileInformation1.file_version));
  proto_file_information->set_active_file(kFileInformation1.active_file);
  MessageBuilder builder;
  AppendMatchedFile(matched_file, &builder);
  EXPECT_EQ(kMatchedFileToStringExpectedString, builder.content());
}

TEST_P(CleanerLoggingServiceTest, AppendFolderInformation) {
  constexpr char kFolderPath[] = "some/path/something";
  constexpr char kCreationDate[] = "3/1/2016";
  constexpr char kLastModifiedDate[] = "3/3/2016";

  constexpr wchar_t kExpectedFolderString[] =
      L"path = 'some/path/something', folder_creation_date = "
      L"'3/1/2016', folder_last_modified_date = '3/3/2016'";

  FolderInformation folder_information;
  MessageBuilder builder;
  AppendFolderInformation(folder_information, &builder);
  EXPECT_TRUE(builder.content().empty());

  folder_information.set_path(kFolderPath);
  folder_information.set_creation_date(kCreationDate);
  folder_information.set_last_modified_date(kLastModifiedDate);
  AppendFolderInformation(folder_information, &builder);

  EXPECT_EQ(kExpectedFolderString, builder.content());
}

TEST_P(CleanerLoggingServiceTest, AppendMatchedRegistryEntry) {
  MatchedRegistryEntry matched_registry_entry;
  matched_registry_entry.set_key_path(
      base::UTF16ToUTF8(kMatchedRegistryEntryKey));
  matched_registry_entry.set_value_name(
      base::UTF16ToUTF8(kMatchedRegistryEntryValueName));
  matched_registry_entry.set_value_substring(
      base::UTF16ToUTF8(kMatchedRegistryEntryValueSubstring));
  MessageBuilder builder;
  AppendMatchedRegistryEntry(matched_registry_entry, &builder);
  EXPECT_EQ(kMatchedRegistryEntryToStringExpectedString, builder.content());
}

TEST_P(CleanerLoggingServiceTest, AddLoadedModule) {
  static const wchar_t kLoadedModuleName1[] = L"SomeLoadedModuleName1";
  static const wchar_t kLoadedModuleName2[] = L"SomeLoadedModuleName2";

  logging_service_->EnableUploads(true, registry_logger_.get());

  ChromeCleanerReport report;
  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  EXPECT_EQ(report.system_report().loaded_modules_size(), 0);

  logging_service_->AddLoadedModule(kLoadedModuleName1, ModuleHost::CHROME,
                                    kFileInformation1);
  logging_service_->AddLoadedModule(
      kLoadedModuleName2, ModuleHost::CHROME_CLEANUP_TOOL, kFileInformation2);

  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  ASSERT_EQ(report.system_report().loaded_modules_size(), 2);

  ChromeCleanerReport_SystemReport_LoadedModule loaded_module_chrome =
      report.system_report().loaded_modules(0);
  ExpectFileInformationEqualToProtoObj(kFileInformation1,
                                       loaded_module_chrome.file_information());
  EXPECT_EQ(base::WideToUTF8(kLoadedModuleName1), loaded_module_chrome.name());
  EXPECT_EQ(ModuleHost::CHROME, loaded_module_chrome.host());

  ChromeCleanerReport_SystemReport_LoadedModule loaded_module_cct =
      report.system_report().loaded_modules(1);
  ExpectFileInformationEqualToProtoObj(kFileInformation2,
                                       loaded_module_cct.file_information());
  EXPECT_EQ(base::WideToUTF8(kLoadedModuleName2), loaded_module_cct.name());
  EXPECT_EQ(ModuleHost::CHROME_CLEANUP_TOOL, loaded_module_cct.host());
}

TEST_P(CleanerLoggingServiceTest, AddService) {
  static const wchar_t kServiceDisplayName[] = L"ServiceDisplayName";
  static const wchar_t kServiceName[] = L"ServiceName";

  logging_service_->EnableUploads(true, registry_logger_.get());

  ChromeCleanerReport report;
  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  ASSERT_EQ(report.system_report().services_size(), 0);

  logging_service_->AddService(kServiceDisplayName, kServiceName,
                               kFileInformation1);

  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  ASSERT_EQ(report.system_report().services_size(), 1);

  ChromeCleanerReport_SystemReport_Service added_service =
      report.system_report().services(0);

  ExpectFileInformationEqualToProtoObj(kFileInformation1,
                                       added_service.file_information());
  EXPECT_EQ(base::WideToUTF8(kServiceDisplayName),
            added_service.display_name());
  EXPECT_EQ(base::WideToUTF8(kServiceName), added_service.service_name());
}

TEST_P(CleanerLoggingServiceTest, AddInstalledProgram) {
  constexpr wchar_t kInstalledProgramName[] = L"some_installed_program";

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath installed_program_path;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(
      temp_dir.GetPath(), kInstalledProgramName, &installed_program_path));

  logging_service_->EnableUploads(true, registry_logger_.get());

  ChromeCleanerReport report;
  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  ASSERT_EQ(report.system_report().installed_programs_size(), 0);

  logging_service_->AddInstalledProgram(installed_program_path);

  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  ASSERT_EQ(report.system_report().installed_programs_size(), 1);

  FolderInformation folder_information =
      report.system_report().installed_programs(0).folder_information();

  const base::string16 sanitized_path = SanitizePath(installed_program_path);
  EXPECT_EQ(base::UTF16ToUTF8(sanitized_path), folder_information.path());
  EXPECT_FALSE(folder_information.creation_date().empty());
  EXPECT_FALSE(folder_information.last_modified_date().empty());
}

TEST_P(CleanerLoggingServiceTest, AddProcess) {
  static const wchar_t kProcessName[] = L"SomeProcessName";

  logging_service_->EnableUploads(true, registry_logger_.get());

  ChromeCleanerReport report;
  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  EXPECT_EQ(report.system_report().processes_size(), 0);

  logging_service_->AddProcess(kProcessName, kFileInformation1);

  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  ASSERT_EQ(report.system_report().processes_size(), 1);

  ChromeCleanerReport_SystemReport_Process added_process =
      report.system_report().processes(0);
  ExpectFileInformationEqualToProtoObj(kFileInformation1,
                                       added_process.file_information());
  EXPECT_EQ(base::WideToUTF8(kProcessName), added_process.name());
}

TEST_P(CleanerLoggingServiceTest, AddRegistryValue) {
  logging_service_->EnableUploads(true, registry_logger_.get());

  ChromeCleanerReport report;
  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  EXPECT_EQ(report.system_report().registry_values_size(), 0);

  // Add a registry value with no FileInformation.
  std::vector<internal::FileInformation> empty_file_informations;
  logging_service_->AddRegistryValue(kRegistryValue1, empty_file_informations);

  // Add a registry value with many FileInformation.
  std::vector<internal::FileInformation> multiple_file_informations;
  multiple_file_informations.push_back(kFileInformation1);
  multiple_file_informations.push_back(kFileInformation2);
  logging_service_->AddRegistryValue(kRegistryValue2,
                                     multiple_file_informations);

  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  ASSERT_EQ(report.system_report().registry_values_size(), 2);

  RegistryValue registry_value1 = report.system_report().registry_values(0);
  RegistryValue registry_value2 = report.system_report().registry_values(1);

  ExpectRegistryValueEqualToProtoObj(kRegistryValue1, registry_value1);
  EXPECT_EQ(registry_value1.file_informations_size(), 0);
  ExpectRegistryValueEqualToProtoObj(kRegistryValue2, registry_value2);
  ASSERT_EQ(registry_value2.file_informations_size(), 2);
  ExpectFileInformationEqualToProtoObj(kFileInformation1,
                                       registry_value2.file_informations(0));
  ExpectFileInformationEqualToProtoObj(kFileInformation2,
                                       registry_value2.file_informations(1));
}

TEST_P(CleanerLoggingServiceTest, AddLayeredServiceProvider) {
  static const wchar_t kGuid1[] = L"Guid1";
  static const wchar_t kGuid2[] = L"Guid2";

  logging_service_->EnableUploads(true, registry_logger_.get());

  ChromeCleanerReport report;
  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  ASSERT_EQ(0, report.system_report().layered_service_providers_size());

  std::vector<base::string16> guids;
  guids.push_back(kGuid1);
  guids.push_back(kGuid2);
  logging_service_->AddLayeredServiceProvider(guids, kFileInformation1);

  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  ASSERT_EQ(1, report.system_report().layered_service_providers_size());

  ChromeCleanerReport_SystemReport_LayeredServiceProvider
      layered_service_provider =
          report.system_report().layered_service_providers(0);

  ASSERT_EQ(2, layered_service_provider.guids_size());

  EXPECT_EQ(base::UTF16ToUTF8(guids.at(0)), layered_service_provider.guids(0));
  EXPECT_EQ(base::UTF16ToUTF8(guids.at(1)), layered_service_provider.guids(1));

  ExpectFileInformationEqualToProtoObj(
      kFileInformation1, layered_service_provider.file_information());
}

TEST_P(CleanerLoggingServiceTest, SetWinInetProxySettings) {
  static const wchar_t kSystemProxySettingsAutoConfigURL[] =
      L"https://somewhere.test/proxy.pac";

  const bool kAutodetect = true;

  logging_service_->EnableUploads(true, registry_logger_.get());

  ChromeCleanerReport report;
  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  ASSERT_FALSE(report.system_report().has_win_inet_proxy_settings());

  logging_service_->SetWinInetProxySettings(
      kSystemProxySettingsConfig, kSystemProxySettingsBypass,
      kSystemProxySettingsAutoConfigURL, kAutodetect);

  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  ASSERT_TRUE(report.system_report().has_win_inet_proxy_settings());

  ChromeCleanerReport_SystemReport_SystemProxySettings win_inet_proxy_settings =
      report.system_report().win_inet_proxy_settings();
  EXPECT_EQ(base::WideToUTF8(kSystemProxySettingsConfig),
            win_inet_proxy_settings.config());
  EXPECT_EQ(base::WideToUTF8(kSystemProxySettingsBypass),
            win_inet_proxy_settings.bypass());
  EXPECT_EQ(base::WideToUTF8(kSystemProxySettingsAutoConfigURL),
            win_inet_proxy_settings.auto_config_url());
  EXPECT_EQ(kAutodetect, win_inet_proxy_settings.autodetect());
}

TEST_P(CleanerLoggingServiceTest, SetWinHttpProxySettings) {
  logging_service_->EnableUploads(true, registry_logger_.get());

  ChromeCleanerReport report;
  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  ASSERT_FALSE(report.system_report().has_win_http_proxy_settings());

  logging_service_->SetWinHttpProxySettings(kSystemProxySettingsConfig,
                                            kSystemProxySettingsBypass);

  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  ASSERT_TRUE(report.system_report().has_win_http_proxy_settings());

  ChromeCleanerReport_SystemReport_SystemProxySettings win_http_proxy_settings =
      report.system_report().win_http_proxy_settings();
  EXPECT_EQ(base::WideToUTF8(kSystemProxySettingsConfig),
            win_http_proxy_settings.config());
  EXPECT_EQ(base::WideToUTF8(kSystemProxySettingsBypass),
            win_http_proxy_settings.bypass());
}

TEST_P(CleanerLoggingServiceTest, AddInstalledExtension) {
  static const wchar_t kExtensionId[] = L"ababababcdcdcdcdefefefefghghghgh";
  static const wchar_t kExtensionId2[] = L"aaaabbbbccccddddeeeeffffgggghhhh";

  logging_service_->EnableUploads(true, registry_logger_.get());
  ChromeCleanerReport report;
  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  ASSERT_EQ(report.system_report().installed_extensions_size(), 0);

  internal::FileInformation file1, file2;
  const base::string16 kFilePath1 = L"path/file1";
  const base::string16 kFilePath2 = L"path/file2";
  file1.path = kFilePath1;
  file2.path = kFilePath2;
  logging_service_->AddInstalledExtension(
      kExtensionId, ExtensionInstallMethod::POLICY_EXTENSION_FORCELIST,
      {file1});
  logging_service_->AddInstalledExtension(
      kExtensionId2, ExtensionInstallMethod::POLICY_MASTER_PREFERENCES,
      {file1, file2});

  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  ASSERT_EQ(report.system_report().installed_extensions_size(), 2);

  ChromeCleanerReport_SystemReport_InstalledExtension installed_extension =
      report.system_report().installed_extensions(0);
  EXPECT_EQ(base::WideToUTF8(kExtensionId), installed_extension.extension_id());
  EXPECT_EQ(ExtensionInstallMethod::POLICY_EXTENSION_FORCELIST,
            installed_extension.install_method());
  ASSERT_EQ(installed_extension.extension_files().size(), 1);
  EXPECT_EQ(installed_extension.extension_files(0).path(),
            base::UTF16ToUTF8(kFilePath1));

  installed_extension = report.system_report().installed_extensions(1);
  EXPECT_EQ(base::WideToUTF8(kExtensionId2),
            installed_extension.extension_id());
  EXPECT_EQ(ExtensionInstallMethod::POLICY_MASTER_PREFERENCES,
            installed_extension.install_method());
  ASSERT_EQ(installed_extension.extension_files().size(), 2);

  std::vector<std::string> reported_files = {
      installed_extension.extension_files(0).path(),
      installed_extension.extension_files(1).path()};
  EXPECT_THAT(reported_files, testing::UnorderedElementsAreArray(
                                  {base::UTF16ToUTF8(kFilePath1),
                                   base::UTF16ToUTF8(kFilePath2)}));
}

TEST_P(CleanerLoggingServiceTest, AddScheduledTask) {
  static const wchar_t kScheduledTaskName[] = L"SomeTaskName";
  static const wchar_t kScheduledTaskDescription[] = L"Description";

  logging_service_->EnableUploads(true, registry_logger_.get());

  ChromeCleanerReport report;
  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  ASSERT_EQ(0, report.system_report().scheduled_tasks_size());

  std::vector<internal::FileInformation> actions;
  actions.push_back(kFileInformation1);
  actions.push_back(kFileInformation2);
  logging_service_->AddScheduledTask(kScheduledTaskName,
                                     kScheduledTaskDescription, actions);

  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  ASSERT_EQ(1, report.system_report().scheduled_tasks_size());

  ScheduledTask added_scheduled_task =
      report.system_report().scheduled_tasks(0);
  ASSERT_EQ(2, added_scheduled_task.actions_size());

  ExpectFileInformationEqualToProtoObj(
      kFileInformation1, added_scheduled_task.actions(0).file_information());
  ExpectFileInformationEqualToProtoObj(
      kFileInformation2, added_scheduled_task.actions(1).file_information());
  EXPECT_EQ(base::WideToUTF8(kScheduledTaskName), added_scheduled_task.name());
  EXPECT_EQ(base::WideToUTF8(kScheduledTaskDescription),
            added_scheduled_task.description());
}

TEST_P(CleanerLoggingServiceTest, LogProcessInformation) {
  base::IoCounters io_counters;
  io_counters.ReadOperationCount = 1;
  io_counters.WriteOperationCount = 2;
  io_counters.OtherOperationCount = 3;
  io_counters.ReadTransferCount = 4;
  io_counters.WriteTransferCount = 5;
  io_counters.OtherTransferCount = 6;
  SystemResourceUsage usage = {io_counters, base::TimeDelta::FromSeconds(10),
                               base::TimeDelta::FromSeconds(20), 123456};

  logging_service_->EnableUploads(true, registry_logger_.get());
  logging_service_->LogProcessInformation(SandboxType::kNonSandboxed, usage);

  ChromeCleanerReport report;
  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  ASSERT_EQ(1, report.process_information_size());
  EXPECT_EQ(ProcessInformation::MAIN, report.process_information(0).process());

  ProcessInformation::SystemResourceUsage usage_msg =
      report.process_information(0).resource_usage();
  EXPECT_TRUE(IoCountersEqual(io_counters, usage_msg));
  EXPECT_EQ(10U, usage_msg.user_time());
  EXPECT_EQ(20U, usage_msg.kernel_time());
  EXPECT_EQ(123456U, usage_msg.peak_working_set_size());
}

namespace {

// Add a matched file entry to |uws| with path given by SanitizePath(|path|)
// and removal status "matched only".
void AddFileToUwS(const base::FilePath& path, UwS* uws) {
  MatchedFile* file = uws->add_files();
  file->mutable_file_information()->set_path(
      base::UTF16ToUTF8(SanitizePath(path)));
  file->mutable_file_information()->set_active_file(
      PathHasActiveExtension(path));
  file->set_removal_status(REMOVAL_STATUS_MATCHED_ONLY);
  file->set_quarantine_status(QUARANTINE_STATUS_UNSPECIFIED);
}

// Add a matched folder entry to |uws| with path given by SanitizePath(|path|)
// and removal status "matched only".
void AddFolderToUwS(const base::FilePath& path, UwS* uws) {
  MatchedFolder* folder = uws->add_folders();
  folder->mutable_folder_information()->set_path(
      base::UTF16ToUTF8(SanitizePath(path)));
  folder->set_removal_status(REMOVAL_STATUS_MATCHED_ONLY);
}

// Checks that all files and folders for all UwS entries in |report| have
// removal status |expected_status|.
void ExpectRemovalStatus(const ChromeCleanerReport& report,
                         RemovalStatus expected_status) {
  for (const UwS& uws : report.detected_uws()) {
    for (const MatchedFile& file : uws.files())
      EXPECT_EQ(expected_status, file.removal_status());
    for (const MatchedFolder& folder : uws.folders())
      EXPECT_EQ(expected_status, folder.removal_status());
  }
}

// Checks that all files for all UwS entries in |report| have quarantine status
// |expected_status|.
void ExpectQuarantineStatus(const ChromeCleanerReport& report,
                            QuarantineStatus expected_status) {
  for (const UwS& uws : report.detected_uws()) {
    for (const MatchedFile& file : uws.files())
      EXPECT_EQ(expected_status, file.quarantine_status());

    // We should not quarantine any folder.
    EXPECT_TRUE(uws.folders().empty());
  }
}

// Checks that the given path appears in the unknown UwS field with the expected
// removal status.
void ExpectUnknownRemovalStatus(const ChromeCleanerReport& report,
                                const base::FilePath& path,
                                RemovalStatus expected_status) {
  bool found = false;
  std::string normalized_path = base::UTF16ToUTF8(SanitizePath(path));
  for (const auto& file : report.unknown_uws().files()) {
    if (file.file_information().path() == normalized_path) {
      EXPECT_EQ(file.removal_status(), expected_status);
      EXPECT_FALSE(found)
          << normalized_path
          << " appeared in the unknown UwS files more than once";
      found = true;
    }
  }
  EXPECT_TRUE(found) << normalized_path
                     << " didn't appear in the unknown UwS files";
}

}  // namespace

TEST_P(CleanerLoggingServiceTest, InitializeResetsRemovalStatus) {
  EXPECT_TRUE(
      FileRemovalStatusUpdater::GetInstance()->GetAllRemovalStatuses().empty());
}

TEST_P(CleanerLoggingServiceTest, UpdateRemovalStatus) {
  constexpr wchar_t kFile1[] = L"C:\\Program Files\\My Dear UwS\\File1.exe";
  constexpr wchar_t kFile2[] =
      L"C:\\Program Files\\My least favority UwS\\File2.exe";
  constexpr wchar_t kFolder1[] = L"C:\\Program Files\\My Dear UwS";
  constexpr wchar_t kFolder2[] = L"C:\\Program Files\\Nasty UwS";

  const std::vector<base::FilePath> paths{
      base::FilePath(kFile1), base::FilePath(kFile2), base::FilePath(kFolder1),
      base::FilePath(kFolder2),
  };

  // Creates a vector of all RemovalStatus enum values to improve readability
  // of loops in this test. Loops start from RemovalStatus_MIN + 1 to avoid
  // testing default value REMOVAL_STATUS_UNSPECIFIED, that is not written
  // by the cleaner code. Also, ensures that all RemovalStatus enumerators are
  // checked.
  std::vector<RemovalStatus> all_removal_status;
  for (int i = RemovalStatus_MIN + 1; i <= RemovalStatus_MAX; ++i) {
    if (RemovalStatus_IsValid(i))
      all_removal_status.push_back(static_cast<RemovalStatus>(i));
  }

  FileRemovalStatusUpdater* removal_status_updater =
      FileRemovalStatusUpdater::GetInstance();
  for (RemovalStatus removal_status : all_removal_status) {
    for (RemovalStatus new_removal_status : all_removal_status) {
      SCOPED_TRACE(::testing::Message()
                   << "removal_status " << removal_status
                   << ", new_removal_status " << new_removal_status);
      logging_service_->EnableUploads(true, registry_logger_.get());

      // Add two UwS to the logged proto with a shared file and a shared folder
      // to ensure that removal status is set for all occurrences of the same
      // file/folder.
      UwS uws;
      uws.set_id(1);
      AddFileToUwS(base::FilePath(kFile1), &uws);
      AddFolderToUwS(base::FilePath(kFolder1), &uws);
      logging_service_->AddDetectedUwS(uws);

      uws.set_id(2);  // kFile1 and kFolder1 are in both UwS.
      AddFileToUwS(base::FilePath(kFile2), &uws);
      AddFolderToUwS(base::FilePath(kFolder2), &uws);
      logging_service_->AddDetectedUwS(uws);

      // Initially, files and folders have removal status "matched only", which
      // was given in the proto passed to AddDetectedUwS().
      ChromeCleanerReport report;
      ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
      ExpectRemovalStatus(report, REMOVAL_STATUS_MATCHED_ONLY);

      // Any removal status can override "matched only".
      for (const base::FilePath& path : paths)
        removal_status_updater->UpdateRemovalStatus(path, removal_status);
      ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
      ExpectRemovalStatus(report, removal_status);

      // Tests if attempts to override removal status obey the rules specified
      // by GetRemovalStatusOverridePermissionMap().
      for (const base::FilePath& path : paths)
        removal_status_updater->UpdateRemovalStatus(path, new_removal_status);
      ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
      const internal::RemovalStatusOverridePermissionMap& decisions_map =
          internal::GetRemovalStatusOverridePermissionMap();
      const bool can_override = decisions_map.find(removal_status)
                                    ->second.find(new_removal_status)
                                    ->second == internal::kOkToOverride;
      ExpectRemovalStatus(report,
                          can_override ? new_removal_status : removal_status);

      // Reset the logging service, so one the current test doesn't interfere
      // with the next one.
      logging_service_->Terminate();
      logging_service_->Initialize(registry_logger_.get());
    }
  }
}

TEST_P(CleanerLoggingServiceTest, UpdateQuarantineStatus) {
  const base::FilePath kFile1(L"C:\\Program Files\\My Dear UwS\\File1.exe");

  // Creates a vector of all QuarantineStatus enum values to improve readability
  // of loops in this test and ensure that all QuarantineStatus enumerators are
  // checked.
  std::vector<QuarantineStatus> all_quarantine_status;
  for (int i = QuarantineStatus_MIN; i <= QuarantineStatus_MAX; ++i) {
    // Status cannot be set to QUARANTINE_STATUS_UNSPECIFIED - this is guarded
    // by an assert.
    QuarantineStatus status = static_cast<QuarantineStatus>(i);
    if (QuarantineStatus_IsValid(status) &&
        status != QUARANTINE_STATUS_UNSPECIFIED)
      all_quarantine_status.push_back(status);
  }

  FileRemovalStatusUpdater* removal_status_updater =
      FileRemovalStatusUpdater::GetInstance();
  for (QuarantineStatus status : all_quarantine_status) {
    logging_service_->EnableUploads(true, registry_logger_.get());

    UwS uws;
    uws.set_id(1);
    AddFileToUwS(kFile1, &uws);
    logging_service_->AddDetectedUwS(uws);

    ChromeCleanerReport report;
    // The default quarantine status should be |QUARANTINE_STATUS_UNSPECIFIED|.
    EXPECT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
    ExpectQuarantineStatus(report, QUARANTINE_STATUS_UNSPECIFIED);

    // Removal status has to be updated with a valid status.
    removal_status_updater->UpdateRemovalStatus(kFile1,
                                                REMOVAL_STATUS_MATCHED_ONLY);
    removal_status_updater->UpdateQuarantineStatus(kFile1, status);
    // It should always succeed to override |QUARANTINE_STATUS_UNSPECIFIED|.
    EXPECT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
    ExpectQuarantineStatus(report, status);

    // Reset the logging service, so one the current test doesn't interfere with
    // the next one.
    logging_service_->Terminate();
    logging_service_->Initialize(registry_logger_.get());
  }
}

TEST_P(CleanerLoggingServiceTest, UpdateRemovalStatus_UwSAdded) {
  constexpr wchar_t kFile1[] = L"C:\\Program Files\\My Dear UwS\\File1.exe";

  logging_service_->EnableUploads(true, registry_logger_.get());

  FileRemovalStatusUpdater* removal_status_updater =
      FileRemovalStatusUpdater::GetInstance();

  // Create an UwS and update the removal status of one of its files.
  UwS uws;
  uws.set_id(1);
  AddFileToUwS(base::FilePath(kFile1), &uws);
  logging_service_->AddDetectedUwS(uws);

  removal_status_updater->UpdateRemovalStatus(
      base::FilePath(kFile1), REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL);

  ChromeCleanerReport report;
  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  ExpectRemovalStatus(report, REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL);

  // Create another UwS with the same file. The file should already be
  // scheduled for removal.
  UwS uws2;
  uws2.set_id(2);
  AddFileToUwS(base::FilePath(kFile1), &uws2);
  logging_service_->AddDetectedUwS(uws2);

  ChromeCleanerReport report2;
  ASSERT_TRUE(report2.ParseFromString(logging_service_->RawReportContent()));
  ExpectRemovalStatus(report2, REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL);
}

TEST_P(CleanerLoggingServiceTest, UpdateRemovalStatus_UnknownUwS) {
  logging_service_->EnableUploads(true, registry_logger_.get());

  FileRemovalStatusUpdater* removal_status_updater =
      FileRemovalStatusUpdater::GetInstance();

  // Update the removal status of a real file.
  base::FilePath real_file = uws_dir_.GetPath().Append(L"real_file.exe");
  EXPECT_TRUE(CreateEmptyFile(real_file));
  removal_status_updater->UpdateRemovalStatus(real_file,
                                              REMOVAL_STATUS_MATCHED_ONLY);
  ChromeCleanerReport report;
  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  ExpectUnknownRemovalStatus(report, real_file, REMOVAL_STATUS_MATCHED_ONLY);

  removal_status_updater->UpdateRemovalStatus(
      real_file, REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL);
  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  ExpectUnknownRemovalStatus(report, real_file,
                             REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL);

  // Update the removeal status of a non-existant file and ensure it is still
  // recorded.
  base::FilePath fake_file = uws_dir_.GetPath().Append(L"fake_file.exe");
  removal_status_updater->UpdateRemovalStatus(fake_file,
                                              REMOVAL_STATUS_MATCHED_ONLY);
  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  ExpectUnknownRemovalStatus(report, fake_file, REMOVAL_STATUS_MATCHED_ONLY);

  removal_status_updater->UpdateRemovalStatus(
      fake_file, REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL);
  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  ExpectUnknownRemovalStatus(report, fake_file,
                             REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL);
}

TEST_P(CleanerLoggingServiceTest, AllExpectedRemovalsConfirmed) {
  const base::FilePath kFile1(L"C:\\Program Files\\uws.exe");
  const base::FilePath kFile2(L"C:\\Program Files\\persistant.exe");
  const base::FilePath kFile3(L"C:\\Program Files\\another_uws.exe");
  const base::FilePath kFile4(L"C:\\Program Files\\inactive.txt");

  logging_service_->EnableUploads(true, registry_logger_.get());

  FileRemovalStatusUpdater* removal_status_updater =
      FileRemovalStatusUpdater::GetInstance();

  // No detected UwS -> nothing to remove.
  EXPECT_TRUE(logging_service_->AllExpectedRemovalsConfirmed());

  UwS uws1;
  uws1.set_id(1);
  uws1.set_state(UwS::REMOVABLE);
  AddFileToUwS(kFile1, &uws1);
  logging_service_->AddDetectedUwS(uws1);
  EXPECT_FALSE(logging_service_->AllExpectedRemovalsConfirmed());
  removal_status_updater->UpdateRemovalStatus(kFile1,
                                              REMOVAL_STATUS_MATCHED_ONLY);
  EXPECT_FALSE(logging_service_->AllExpectedRemovalsConfirmed());
  removal_status_updater->UpdateRemovalStatus(kFile1,
                                              REMOVAL_STATUS_FAILED_TO_REMOVE);
  EXPECT_FALSE(logging_service_->AllExpectedRemovalsConfirmed());
  removal_status_updater->UpdateRemovalStatus(
      kFile1, REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL_FALLBACK);
  EXPECT_TRUE(logging_service_->AllExpectedRemovalsConfirmed());
  removal_status_updater->UpdateRemovalStatus(
      kFile1, REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL);
  EXPECT_TRUE(logging_service_->AllExpectedRemovalsConfirmed());

  UwS uws2;
  uws2.set_id(2);
  uws2.set_state(UwS::REPORT_ONLY);
  AddFileToUwS(kFile2, &uws2);
  logging_service_->AddDetectedUwS(uws2);
  EXPECT_TRUE(logging_service_->AllExpectedRemovalsConfirmed());

  UwS uws3;
  uws3.set_id(3);
  uws3.set_state(UwS::REMOVABLE);
  AddFileToUwS(kFile2, &uws3);
  AddFileToUwS(kFile3, &uws3);

  // Since this is inactive, it is not expected to be removed.
  AddFileToUwS(kFile4, &uws3);

  logging_service_->AddDetectedUwS(uws3);
  EXPECT_FALSE(logging_service_->AllExpectedRemovalsConfirmed());
  removal_status_updater->UpdateRemovalStatus(kFile2, REMOVAL_STATUS_REMOVED);
  EXPECT_FALSE(logging_service_->AllExpectedRemovalsConfirmed());
  removal_status_updater->UpdateRemovalStatus(kFile3, REMOVAL_STATUS_NOT_FOUND);
  EXPECT_TRUE(logging_service_->AllExpectedRemovalsConfirmed());

  // When another UwS is found with all files already scheduled for removal,
  // the removals should already be confirmed.
  UwS uws4;
  uws4.set_id(4);
  uws4.set_state(UwS::REMOVABLE);
  AddFileToUwS(kFile1, &uws4);
  logging_service_->AddDetectedUwS(uws4);
  EXPECT_TRUE(logging_service_->AllExpectedRemovalsConfirmed());
}

TEST_P(CleanerLoggingServiceTest, AddShortcutData) {
  const base::string16 kLnkPath = L"C:\\Users\\SomeUser";
  const base::string16 kExecutablePath1 =
      L"C:\\executable_path\\executable.exe";
  const base::string16 kExecutablePath2 = L"C:\\executable_path\\bad_file.exe";
  const std::string kHash = "HASHSTRING";
  const std::vector<base::string16> kCommandLineArguments = {
      L"some-argument", L"-ha", L"-ha", L"-ha"};

  logging_service_->AddShortcutData(kLnkPath, kExecutablePath1, kHash, {});
  logging_service_->AddShortcutData(kLnkPath, kExecutablePath2, kHash,
                                    kCommandLineArguments);

  ChromeCleanerReport report;
  ASSERT_TRUE(report.ParseFromString(logging_service_->RawReportContent()));
  ASSERT_EQ(report.system_report().shortcut_data().size(), 2);
  ChromeCleanerReport_SystemReport_ShortcutData shortcut1, shortcut2;
  shortcut1 = report.system_report().shortcut_data(0);
  shortcut2 = report.system_report().shortcut_data(1);

  EXPECT_EQ(shortcut1.lnk_path(), base::UTF16ToUTF8(kLnkPath));
  EXPECT_EQ(shortcut2.lnk_path(), base::UTF16ToUTF8(kLnkPath));
  EXPECT_EQ(shortcut1.executable_path(), base::UTF16ToUTF8(kExecutablePath1));
  EXPECT_EQ(shortcut2.executable_path(), base::UTF16ToUTF8(kExecutablePath2));
  EXPECT_EQ(shortcut1.executable_hash(), kHash);
  EXPECT_EQ(shortcut2.executable_hash(), kHash);
  EXPECT_EQ(shortcut1.command_line_arguments().size(), 0);
  EXPECT_EQ(shortcut2.command_line_arguments().size(), 4);
}

// TODO(csharp) add multi-thread tests.

INSTANTIATE_TEST_SUITE_P(All,
                         CleanerLoggingServiceTest,
                         testing::Values(ExecutionMode::kScanning,
                                         ExecutionMode::kCleanup),
                         GetParamNameForTest());

}  // namespace chrome_cleaner
