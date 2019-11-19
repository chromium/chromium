// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/chrome_cleaner/engines/controllers/main_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/chrome_cleaner/components/component_api.h"
#include "chrome/chrome_cleaner/components/component_manager.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/ipc/chrome_prompt_ipc.h"
#include "chrome/chrome_cleaner/ipc/mock_chrome_prompt_ipc.h"
#include "chrome/chrome_cleaner/logging/logging_service_api.h"
#include "chrome/chrome_cleaner/logging/registry_logger.h"
#include "chrome/chrome_cleaner/os/file_remover_api.h"
#include "chrome/chrome_cleaner/os/rebooter_api.h"
#include "chrome/chrome_cleaner/scanner/signature_matcher_api.h"
#include "chrome/chrome_cleaner/test/test_component.h"
#include "chrome/chrome_cleaner/test/test_pup_data.h"
#include "chrome/chrome_cleaner/test/test_settings_util.h"
#include "chrome/chrome_cleaner/test/test_signature_matcher.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "chrome/chrome_cleaner/ui/main_dialog_api.h"
#include "components/chrome_cleaner/test/test_name_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace chrome_cleaner {

namespace {

const UwSId kFakePupId = 0;

class TestEngineFacade : public EngineFacadeInterface {
 public:
  class TestCleaner : public Cleaner {
   public:
    TestCleaner()
        : start_called_(false),
          clean_result_(RESULT_CODE_SUCCESS),
          stop_called_(false),
          is_completely_done_(true),
          get_cleanup_requirements_called_(false),
          get_cleanup_requirements_return_value_(true),
          post_reboot_clean_called_(false),
          post_reboot_clean_return_value_(RESULT_CODE_SUCCESS) {}
    // Cleaner.
    void Start(const std::vector<UwSId>& pup_ids,
               DoneCallback done_callback) override {
      start_called_ = true;
      pups_to_remove_ = pup_ids;
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, base::BindOnce(std::move(done_callback), clean_result_),
          delay_before_done_);
    }

    void StartPostReboot(const std::vector<UwSId>& pup_ids,
                         DoneCallback done_callback) override {
      pups_to_remove_ = pup_ids;
      post_reboot_clean_called_ = true;
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(std::move(done_callback),
                         post_reboot_clean_return_value_),
          delay_before_done_);
    }

    void Stop() override { stop_called_ = true; }

    bool IsCompletelyDone() const override { return is_completely_done_; }

    bool CanClean(const std::vector<UwSId>& pup_ids) override {
      get_cleanup_requirements_called_ = true;
      return get_cleanup_requirements_return_value_;
    }

    base::TimeDelta delay_before_done_;

    bool start_called_;
    ResultCode clean_result_;
    bool stop_called_;
    bool is_completely_done_;
    bool get_cleanup_requirements_called_;
    bool get_cleanup_requirements_return_value_;
    bool post_reboot_clean_called_;
    ResultCode post_reboot_clean_return_value_;
    std::vector<UwSId> pups_to_remove_;
  } test_cleaner_;

  class TestScanner : public Scanner {
   public:
    TestScanner()
        : scan_result_(RESULT_CODE_SUCCESS), is_completely_done_(true) {}
    bool Start(const FoundUwSCallback& found_uws_callback,
               DoneCallback done_callback) override {
      for (UwSId pup_id : found_pups_) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(found_uws_callback, pup_id));
      }
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(std::move(done_callback), scan_result_, found_pups_),
          delay_before_done_);
      return true;
    }
    void Stop() override {}
    bool IsCompletelyDone() const override { return is_completely_done_; }

    std::vector<UwSId> found_pups_;
    base::TimeDelta delay_before_done_;
    ResultCode scan_result_;

    bool is_completely_done_;
  } test_scanner_;

  Scanner* GetScanner() override { return &test_scanner_; }

  Cleaner* GetCleaner() override { return &test_cleaner_; }

  base::TimeDelta GetScanningWatchdogTimeout() const override {
    return base::TimeDelta::FromMilliseconds(150);
  }
};

class TestMainController : public MainController {
 public:
  explicit TestMainController(ChromePromptIPC* chrome_prompt_ipc)
      : MainController(&test_rebooter_,
                       &test_registry_logger_,
                       chrome_prompt_ipc),
        test_registry_logger_(RegistryLogger::Mode::NOOP_FOR_TESTING),
        test_main_dialog_(this) {}

  MainDialogAPI* main_dialog() override { return &test_main_dialog_; }

  void set_scanning_watchdog_timeout(base::TimeDelta timeout) {
    scanning_watchdog_timeout_ = timeout;
  }

  void set_user_response_watchdog_timeout(base::TimeDelta timeout) {
    user_response_watchdog_timeout_ = timeout;
  }

  void set_user_response_delay(base::TimeDelta delay) {
    test_main_dialog_.set_user_response_delay(delay);
  }

  void set_cleaning_watchdog_timeout(base::TimeDelta timeout) {
    cleaning_watchdog_timeout_ = timeout;
  }

  // TODO(joenotcharles): See about testing log uploads in these tests.
  RegistryLogger test_registry_logger_;

  class TestMainDialog : public MainDialogAPI {
   public:
    explicit TestMainDialog(MainDialogDelegate* delegate)
        : MainDialogAPI(delegate),
          no_pups_found_called_(false),
          confirm_cleanup_called_(false),
          accept_cleanup_(true),
          cleanup_done_called_(false) {}
    bool Create() override { return true; }
    void NoPUPsFound() override {
      no_pups_found_called_ = true;
      delegate()->OnClose();
    }
    void ConfirmCleanup(
        const std::vector<UwSId>& found_pups,
        const FilePathSet& files_to_remove,
        const std::vector<base::string16>& registry_keys) override {
      confirm_cleanup_called_ = true;
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&MainDialogDelegate::AcceptedCleanup,
                         base::Unretained(delegate()), accept_cleanup_),
          user_response_delay_);
    }
    void CleanupDone(ResultCode cleanup_result) override {
      cleanup_done_called_ = true;
      delegate()->OnClose();
    }
    void Close() override { delegate()->OnClose(); }
    void DisableExtensions(const std::vector<base::string16>& extensions,
                           base::OnceCallback<void(bool)> on_disable) override {
      std::move(on_disable).Run(true);
    }
    void set_user_response_delay(base::TimeDelta delay) {
      user_response_delay_ = delay;
    }

    bool no_pups_found_called_;
    bool confirm_cleanup_called_;
    bool accept_cleanup_;
    bool cleanup_done_called_;
    base::TimeDelta user_response_delay_;
  } test_main_dialog_;

  class TestRebooter : public RebooterAPI {
   public:
    TestRebooter() {}
    void AppendPostRebootSwitch(const std::string& unused) override {}
    void AppendPostRebootSwitchASCII(const std::string& unused1,
                                     const std::string& unused2) override {}
    bool RegisterPostRebootRun(const base::CommandLine* current_command_line,
                               const std::string& cleanup_id,
                               ExecutionMode execution_mode,
                               bool logs_uploads_allowed) override {
      post_reboot_registered_ = true;
      return true;
    }
    void UnregisterPostRebootRun() override { post_reboot_registered_ = false; }

    bool post_reboot_registered_ = false;
  } test_rebooter_;
};

class MainControllerTest : public testing::TestWithParam<ExecutionMode> {
 public:
  MainControllerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  TestMainController* test_main_controller() {
    return test_main_controller_.get();
  }

  void SetUp() override {
    execution_mode_ = GetParam();

    EXPECT_CALL(mock_settings_, execution_mode())
        .WillRepeatedly(testing::Return(GetParam()));
    EXPECT_CALL(mock_settings_, logs_upload_allowed())
        .WillRepeatedly(testing::Return(false));
    Settings::SetInstanceForTesting(&mock_settings_);

    if (scanning_mode())
      EXPECT_CALL(mock_chrome_prompt_ipc_, Initialize(_));

    test_main_controller_ = std::make_unique<TestMainController>(
        scanning_mode() ? &mock_chrome_prompt_ipc_ : nullptr);
    test_main_controller_->SetEngineFacade(&test_engine_facade_);

    // There is a ScopedLoggingService over this scope. To avoid keeping
    // any global logging state information, we close the current session
    // and initialize a new one.
    LoggingServiceAPI::GetInstance()->Terminate();
    LoggingServiceAPI::GetInstance()->Initialize(
        &test_main_controller_->test_registry_logger_);
  }

  void TearDown() override {
    // There is a ScopedLoggingService over this scope. The temporary session
    // is closed and a new one is started.
    LoggingServiceAPI::GetInstance()->Terminate();
    LoggingServiceAPI::GetInstance()->Initialize(nullptr);

    test_main_controller_.reset();

    Settings::SetInstanceForTesting(nullptr);
  }

  TestEngineFacade::TestScanner* test_scanner() {
    return &test_engine_facade_.test_scanner_;
  }

  TestEngineFacade::TestCleaner* test_cleaner() {
    return &test_engine_facade_.test_cleaner_;
  }

 protected:
  bool scanning_mode() { return execution_mode_ == ExecutionMode::kScanning; }
  bool cleanup_mode() { return execution_mode_ == ExecutionMode::kCleanup; }

  MockSettings mock_settings_;

  // Use StrictMock to be sure no unexpected IPC messages are sent.
  testing::StrictMock<MockChromePromptIPC> mock_chrome_prompt_ipc_;

 private:
  TestEngineFacade test_engine_facade_;
  std::unique_ptr<TestMainController> test_main_controller_;
  base::test::TaskEnvironment task_environment_;
  ExecutionMode execution_mode_;
};

class SimpleTestPUPData : public TestPUPData {
 public:
  SimpleTestPUPData(UwSId pup_id, PUPData::Flags flags) {
    AddPUP(pup_id, flags, nullptr, PUPData::kMaxFilesToRemoveSmallUwS);
    // For removable UwS, include an active file so there's something to delete.
    if (flags & PUPData::FLAGS_ACTION_REMOVE) {
      constexpr wchar_t kActiveFileName[] = L"C:\\active_file_name.bin";
      AddDiskFootprint(pup_id, CSIDL_STARTUP, kActiveFileName,
                       PUPData::DISK_MATCH_ANY_FILE);

      PUPData::PUP* pup = PUPData::GetPUP(pup_id);
      DCHECK(pup);
      pup->AddDiskFootprint(base::FilePath(kActiveFileName));

      pup->expanded_registry_footprints.push_back(PUPData::RegistryFootprint(
          RegKeyPath(HKEY_USERS, L"Software\\bad-software\\bad-key"),
          base::string16(), base::string16(), REGISTRY_VALUE_MATCH_KEY));
    }
  }
};

ExecutionMode kExecutionModes[] = {ExecutionMode::kScanning,
                                   ExecutionMode::kCleanup};

}  // namespace

TEST_P(MainControllerTest, PostRebootValidateCleanup_ScanSuccessNothingFound) {
  ScopedIsPostReboot is_post_reboot;

  test_scanner()->scan_result_ = RESULT_CODE_SUCCESS;

  EXPECT_EQ(RESULT_CODE_POST_REBOOT_SUCCESS,
            test_main_controller()->ValidateCleanup());

  EXPECT_FALSE(test_cleaner()->post_reboot_clean_called_);
  EXPECT_FALSE(test_cleaner()->start_called_);
  EXPECT_FALSE(test_main_controller()->test_main_dialog_.no_pups_found_called_);
  EXPECT_FALSE(test_main_controller()->test_rebooter_.post_reboot_registered_);
}

TEST_P(MainControllerTest, PostRebootValidateCleanup_ScanNoPupsFound) {
  ScopedIsPostReboot is_post_reboot;

  test_scanner()->scan_result_ = RESULT_CODE_NO_PUPS_FOUND;

  EXPECT_EQ(RESULT_CODE_POST_REBOOT_SUCCESS,
            test_main_controller()->ValidateCleanup());

  EXPECT_FALSE(test_cleaner()->post_reboot_clean_called_);
  EXPECT_FALSE(test_cleaner()->start_called_);
  EXPECT_FALSE(test_main_controller()->test_main_dialog_.no_pups_found_called_);
  EXPECT_FALSE(test_main_controller()->test_rebooter_.post_reboot_registered_);
}

TEST_P(MainControllerTest, PostRebootValidateCleanup_ScanReportOnlyFound) {
  ScopedIsPostReboot is_post_reboot;

  SimpleTestPUPData test_pup_data(kFakePupId, PUPData::FLAGS_NONE);
  test_scanner()->found_pups_.push_back(kFakePupId);
  test_scanner()->scan_result_ = RESULT_CODE_REPORT_ONLY_PUPS_FOUND;

  EXPECT_EQ(RESULT_CODE_POST_REBOOT_SUCCESS,
            test_main_controller()->ValidateCleanup());

  EXPECT_FALSE(test_cleaner()->post_reboot_clean_called_);
  EXPECT_FALSE(test_cleaner()->start_called_);
  EXPECT_FALSE(test_main_controller()->test_rebooter_.post_reboot_registered_);
}

TEST_P(MainControllerTest, PostRebootValidateCleanup_RemainingSuccess) {
  ScopedIsPostReboot is_post_reboot;

  SimpleTestPUPData test_pup_data(kFakePupId, PUPData::FLAGS_ACTION_REMOVE);
  test_scanner()->found_pups_.push_back(kFakePupId);
  test_cleaner()->post_reboot_clean_return_value_ = RESULT_CODE_SUCCESS;

  EXPECT_EQ(RESULT_CODE_POST_REBOOT_SUCCESS,
            test_main_controller()->ValidateCleanup());

  EXPECT_TRUE(test_cleaner()->post_reboot_clean_called_);
  EXPECT_THAT(test_cleaner()->pups_to_remove_,
              testing::ElementsAre(kFakePupId));
  EXPECT_FALSE(test_cleaner()->start_called_);
  EXPECT_FALSE(test_main_controller()->test_main_dialog_.no_pups_found_called_);
  EXPECT_FALSE(test_main_controller()->test_rebooter_.post_reboot_registered_);
}

TEST_P(MainControllerTest, PostRebootValidateCleanup_RemainingFailed) {
  ScopedIsPostReboot is_post_reboot;

  SimpleTestPUPData test_pup_data(kFakePupId, PUPData::FLAGS_ACTION_REMOVE);
  test_scanner()->found_pups_.push_back(kFakePupId);
  test_cleaner()->post_reboot_clean_return_value_ = RESULT_CODE_FAILED;

  EXPECT_EQ(RESULT_CODE_POST_CLEANUP_VALIDATION_FAILED,
            test_main_controller()->ValidateCleanup());

  EXPECT_FALSE(test_cleaner()->start_called_);
  EXPECT_TRUE(test_cleaner()->post_reboot_clean_called_);
  EXPECT_FALSE(test_main_controller()->test_main_dialog_.no_pups_found_called_);
  EXPECT_FALSE(test_main_controller()->test_rebooter_.post_reboot_registered_);
}

TEST_P(MainControllerTest, PostRebootValidateCleanup_EngineFailedOnScan) {
  ScopedIsPostReboot is_post_reboot;

  SimpleTestPUPData test_pup_data(kFakePupId, PUPData::FLAGS_ACTION_REMOVE);
  test_scanner()->found_pups_.push_back(kFakePupId);
  test_scanner()->scan_result_ = RESULT_CODE_SCANNING_ENGINE_ERROR;

  EXPECT_EQ(RESULT_CODE_POST_CLEANUP_VALIDATION_FAILED,
            test_main_controller()->ValidateCleanup());

  EXPECT_FALSE(test_cleaner()->start_called_);
  EXPECT_FALSE(test_cleaner()->post_reboot_clean_called_);
  EXPECT_FALSE(test_main_controller()->test_main_dialog_.no_pups_found_called_);
  EXPECT_FALSE(test_main_controller()->test_rebooter_.post_reboot_registered_);
}

TEST_P(MainControllerTest, PostRebootValidateCleanup_EngineFailedInCleanup) {
  ScopedIsPostReboot is_post_reboot;

  SimpleTestPUPData test_pup_data(kFakePupId, PUPData::FLAGS_ACTION_REMOVE);
  test_scanner()->found_pups_.push_back(kFakePupId);
  test_cleaner()->post_reboot_clean_return_value_ =
      RESULT_CODE_SCANNING_ENGINE_ERROR;

  EXPECT_EQ(RESULT_CODE_POST_CLEANUP_VALIDATION_FAILED,
            test_main_controller()->ValidateCleanup());

  EXPECT_FALSE(test_cleaner()->start_called_);
  EXPECT_TRUE(test_cleaner()->post_reboot_clean_called_);
  EXPECT_FALSE(test_main_controller()->test_main_dialog_.no_pups_found_called_);
  EXPECT_FALSE(test_main_controller()->test_rebooter_.post_reboot_registered_);
}

TEST_P(MainControllerTest, PostRebootValidateCleanup_RequestedAnotherReboot) {
  ScopedIsPostReboot is_post_reboot;

  SimpleTestPUPData test_pup_data(kFakePupId, PUPData::FLAGS_ACTION_REMOVE);
  test_scanner()->found_pups_.push_back(kFakePupId);
  test_cleaner()->post_reboot_clean_return_value_ = RESULT_CODE_PENDING_REBOOT;

  EXPECT_EQ(RESULT_CODE_POST_CLEANUP_VALIDATION_FAILED,
            test_main_controller()->ValidateCleanup());

  EXPECT_FALSE(test_cleaner()->start_called_);
  EXPECT_TRUE(test_cleaner()->post_reboot_clean_called_);
  EXPECT_FALSE(test_main_controller()->test_main_dialog_.no_pups_found_called_);
  EXPECT_FALSE(test_main_controller()->test_rebooter_.post_reboot_registered_);
}

TEST_P(MainControllerTest, ScanningNothingSuccess) {
  ResultCode result = test_main_controller()->ScanAndClean();
  EXPECT_FALSE(test_cleaner()->post_reboot_clean_called_);
  EXPECT_FALSE(test_cleaner()->start_called_);
  EXPECT_TRUE(test_main_controller()->test_main_dialog_.no_pups_found_called_);
  EXPECT_FALSE(test_main_controller()->test_rebooter_.post_reboot_registered_);
  EXPECT_EQ(RESULT_CODE_NO_PUPS_FOUND, result);
}

TEST_P(MainControllerTest, ScanningReportOnlySuccess) {
  SimpleTestPUPData test_pup_data(kFakePupId, PUPData::FLAGS_NONE);
  test_scanner()->found_pups_.push_back(kFakePupId);
  ResultCode result = test_main_controller()->ScanAndClean();
  EXPECT_FALSE(test_cleaner()->post_reboot_clean_called_);
  EXPECT_FALSE(test_cleaner()->start_called_);
  EXPECT_TRUE(test_main_controller()->test_main_dialog_.no_pups_found_called_);
  EXPECT_FALSE(test_main_controller()->test_rebooter_.post_reboot_registered_);
  EXPECT_EQ(RESULT_CODE_REPORT_ONLY_PUPS_FOUND, result);
}

TEST_P(MainControllerTest, ScanningEngineFailed) {
  SimpleTestPUPData test_pup_data(kFakePupId, PUPData::FLAGS_ACTION_REMOVE);
  test_scanner()->found_pups_.push_back(kFakePupId);
  test_scanner()->scan_result_ = RESULT_CODE_SCANNING_ENGINE_ERROR;

  ResultCode result = test_main_controller()->ScanAndClean();

  EXPECT_FALSE(test_cleaner()->post_reboot_clean_called_);
  EXPECT_FALSE(test_cleaner()->start_called_);
  EXPECT_TRUE(test_main_controller()->test_main_dialog_.no_pups_found_called_);
  EXPECT_EQ(RESULT_CODE_SCANNING_ENGINE_ERROR, result);
}

TEST_P(MainControllerTest, GetCleanupRequirementsFailed) {
  SimpleTestPUPData test_pup_data(kFakePupId, PUPData::FLAGS_ACTION_REMOVE);
  test_scanner()->found_pups_.push_back(kFakePupId);
  test_cleaner()->get_cleanup_requirements_return_value_ = false;
  ResultCode result = test_main_controller()->ScanAndClean();
  EXPECT_FALSE(test_cleaner()->post_reboot_clean_called_);
  EXPECT_FALSE(test_cleaner()->start_called_);
  EXPECT_TRUE(test_cleaner()->get_cleanup_requirements_called_);
  EXPECT_TRUE(test_main_controller()->test_main_dialog_.no_pups_found_called_);
  EXPECT_EQ(RESULT_CODE_CLEANUP_REQUIREMENTS_FAILED, result);
}

TEST_P(MainControllerTest, CleanupDenied) {
  SimpleTestPUPData test_pup_data(kFakePupId, PUPData::FLAGS_ACTION_REMOVE);
  test_scanner()->found_pups_.push_back(kFakePupId);
  test_main_controller()->test_main_dialog_.accept_cleanup_ = false;
  ResultCode result = test_main_controller()->ScanAndClean();
  EXPECT_FALSE(test_cleaner()->post_reboot_clean_called_);
  EXPECT_FALSE(test_cleaner()->start_called_);
  EXPECT_TRUE(test_cleaner()->get_cleanup_requirements_called_);
  EXPECT_TRUE(
      test_main_controller()->test_main_dialog_.confirm_cleanup_called_);
  EXPECT_FALSE(test_main_controller()->test_rebooter_.post_reboot_registered_);
  EXPECT_EQ(RESULT_CODE_CLEANUP_PROMPT_DENIED, result);
}

TEST_P(MainControllerTest, CleanupAcceptedAndSucceeded) {
  SimpleTestPUPData test_pup_data(kFakePupId, PUPData::FLAGS_ACTION_REMOVE);
  test_scanner()->found_pups_.push_back(kFakePupId);
  ResultCode result = test_main_controller()->ScanAndClean();
  EXPECT_FALSE(test_cleaner()->post_reboot_clean_called_);
  EXPECT_TRUE(test_cleaner()->get_cleanup_requirements_called_);
  EXPECT_TRUE(
      test_main_controller()->test_main_dialog_.confirm_cleanup_called_);
  EXPECT_TRUE(test_cleaner()->start_called_);
  EXPECT_THAT(test_cleaner()->pups_to_remove_,
              testing::ElementsAre(kFakePupId));
  EXPECT_TRUE(test_main_controller()->test_main_dialog_.cleanup_done_called_);
  EXPECT_FALSE(test_main_controller()->test_rebooter_.post_reboot_registered_);
  EXPECT_EQ(RESULT_CODE_SUCCESS, result);
}

TEST_P(MainControllerTest, CleanupAcceptedButFailed) {
  SimpleTestPUPData test_pup_data(kFakePupId, PUPData::FLAGS_ACTION_REMOVE);
  test_scanner()->found_pups_.push_back(kFakePupId);
  test_cleaner()->clean_result_ = RESULT_CODE_FAILED;
  ResultCode result = test_main_controller()->ScanAndClean();
  EXPECT_FALSE(test_cleaner()->post_reboot_clean_called_);
  EXPECT_TRUE(test_cleaner()->get_cleanup_requirements_called_);
  EXPECT_TRUE(
      test_main_controller()->test_main_dialog_.confirm_cleanup_called_);
  EXPECT_TRUE(test_cleaner()->start_called_);
  EXPECT_TRUE(test_main_controller()->test_main_dialog_.cleanup_done_called_);
  EXPECT_FALSE(test_main_controller()->test_rebooter_.post_reboot_registered_);
  EXPECT_EQ(RESULT_CODE_FAILED, result);
}

TEST_P(MainControllerTest, CleanupEngineFailed) {
  SimpleTestPUPData test_pup_data(kFakePupId, PUPData::FLAGS_ACTION_REMOVE);
  test_scanner()->found_pups_.push_back(kFakePupId);
  test_cleaner()->clean_result_ = RESULT_CODE_SCANNING_ENGINE_ERROR;

  ResultCode result = test_main_controller()->ScanAndClean();

  EXPECT_FALSE(test_cleaner()->post_reboot_clean_called_);
  EXPECT_TRUE(test_cleaner()->get_cleanup_requirements_called_);
  EXPECT_TRUE(
      test_main_controller()->test_main_dialog_.confirm_cleanup_called_);
  EXPECT_TRUE(test_cleaner()->start_called_);
  EXPECT_TRUE(test_main_controller()->test_main_dialog_.cleanup_done_called_);
  EXPECT_EQ(RESULT_CODE_SCANNING_ENGINE_ERROR, result);
}

TEST_P(MainControllerTest, ComponentsWhenPUPFound) {
  SimpleTestPUPData test_pup_data(kFakePupId, PUPData::FLAGS_ACTION_REMOVE);
  test_scanner()->found_pups_.push_back(kFakePupId);
  TestComponent::Calls calls = {};
  test_main_controller()->AddComponent(std::make_unique<TestComponent>(&calls));
  test_main_controller()->ScanAndClean();
  EXPECT_TRUE(calls.pre_scan);
  EXPECT_TRUE(calls.post_scan);
  EXPECT_THAT(calls.post_scan_found_pups, testing::ElementsAre(kFakePupId));
  EXPECT_TRUE(calls.pre_cleanup);
  EXPECT_TRUE(calls.post_cleanup);
  EXPECT_FALSE(calls.post_validation);
  EXPECT_TRUE(calls.on_close);
}

TEST_P(MainControllerTest, ComponentsWhenNoPUPFound) {
  TestComponent::Calls calls = {};
  test_main_controller()->AddComponent(std::make_unique<TestComponent>(&calls));
  test_main_controller()->ScanAndClean();
  EXPECT_TRUE(calls.pre_scan);
  EXPECT_TRUE(calls.post_scan);
  EXPECT_TRUE(calls.post_scan_found_pups.empty());
  EXPECT_FALSE(calls.pre_cleanup);
  EXPECT_FALSE(calls.post_cleanup);
  EXPECT_FALSE(calls.post_validation);
  EXPECT_TRUE(calls.on_close);
}

TEST_P(MainControllerTest, ComponentsOnValidateCleanup) {
  ScopedIsPostReboot is_post_reboot;

  TestComponent::Calls calls = {};
  test_main_controller()->AddComponent(std::make_unique<TestComponent>(&calls));
  test_main_controller()->ValidateCleanup();

  EXPECT_FALSE(calls.pre_scan);
  EXPECT_FALSE(calls.post_scan);
  EXPECT_TRUE(calls.post_scan_found_pups.empty());
  EXPECT_FALSE(calls.pre_cleanup);
  EXPECT_FALSE(calls.post_cleanup);
  EXPECT_TRUE(calls.post_validation);
  EXPECT_TRUE(calls.on_close);
}

// TODO(joenotcharles): add test to make sure that the waits in the
// SafeBrowsingReporter code are only canceled when an explicit reboot is
// requested.

INSTANTIATE_TEST_SUITE_P(All,
                         MainControllerTest,
                         testing::ValuesIn(kExecutionModes),
                         GetParamNameForTest());

class MainControllerWatchdogTest : public MainControllerTest {
 public:
  void ExpectSuccess(ResultCode expected_result) {
    EXPECT_EQ(expected_result, test_main_controller()->ScanAndClean());
  }

  void ExpectDeath(int expected_exit_code) {
    EXPECT_EXIT({ test_main_controller()->ScanAndClean(); },
                ::testing::ExitedWithCode(expected_exit_code), "");
  }
};

TEST_P(MainControllerWatchdogTest, Success) {
  SimpleTestPUPData test_pup_data(kFakePupId, PUPData::FLAGS_ACTION_REMOVE);
  test_main_controller()->set_scanning_watchdog_timeout(
      base::TimeDelta::FromMilliseconds(500));
  test_scanner()->delay_before_done_ = base::TimeDelta::FromMilliseconds(50);
  test_main_controller()->set_user_response_watchdog_timeout(
      base::TimeDelta::FromMilliseconds(500));
  test_main_controller()->set_user_response_delay(
      base::TimeDelta::FromMilliseconds(50));
  test_main_controller()->set_cleaning_watchdog_timeout(
      base::TimeDelta::FromMilliseconds(500));
  test_cleaner()->delay_before_done_ = base::TimeDelta::FromMilliseconds(50);
  test_scanner()->found_pups_.push_back(kFakePupId);
  ExpectSuccess(RESULT_CODE_SUCCESS);
}

TEST_P(MainControllerWatchdogTest, ScannerHangsNoRemovableUwS) {
  SimpleTestPUPData test_pup_data(kFakePupId, 0);
  test_scanner()->found_pups_.push_back(kFakePupId);
  test_scanner()->delay_before_done_ = base::TimeDelta::FromMilliseconds(300);
  // Note: There is a single timeout for the process running in cleanup
  // execution mode, that will include both the scanner and the cleaner steps.
  // This test simulates the scanner hanging in that scenario to make sure that
  // the watchdog timeout is triggered.
  test_main_controller()->set_cleaning_watchdog_timeout(
      base::TimeDelta::FromMilliseconds(200));

  if (scanning_mode()) {
    ExpectDeath(RESULT_CODE_WATCHDOG_TIMEOUT_WITHOUT_REMOVABLE_UWS);
  } else if (cleanup_mode()) {
    ExpectDeath(RESULT_CODE_WATCHDOG_TIMEOUT_CLEANING);
  } else {
    ExpectSuccess(RESULT_CODE_REPORT_ONLY_PUPS_FOUND);
  }
}

TEST_P(MainControllerWatchdogTest, ScannerHangsWithRemovableUwS) {
  SimpleTestPUPData test_pup_data(kFakePupId, PUPData::FLAGS_ACTION_REMOVE);
  test_scanner()->found_pups_.push_back(kFakePupId);
  test_scanner()->delay_before_done_ = base::TimeDelta::FromMilliseconds(300);
  // Note: There is a single timeout for the process running in cleanup
  // execution mode, that will include both the scanner and the cleaner steps.
  // This test simulates the scanner hanging in that scenario, to make sure that
  // the watchdog timeout is triggered.
  test_main_controller()->set_cleaning_watchdog_timeout(
      base::TimeDelta::FromMilliseconds(200));

  if (scanning_mode()) {
    ExpectDeath(RESULT_CODE_WATCHDOG_TIMEOUT_WITH_REMOVABLE_UWS);
  } else if (cleanup_mode()) {
    ExpectDeath(RESULT_CODE_WATCHDOG_TIMEOUT_CLEANING);
  } else {
    ExpectSuccess(RESULT_CODE_SUCCESS);
  }
}

TEST_P(MainControllerWatchdogTest, ScannerHangsWaitingForUserResponse) {
  SimpleTestPUPData test_pup_data(kFakePupId, PUPData::FLAGS_ACTION_REMOVE);
  test_scanner()->found_pups_.push_back(kFakePupId);
  test_main_controller()->set_user_response_watchdog_timeout(
      base::TimeDelta::FromMilliseconds(50));
  test_main_controller()->set_user_response_delay(
      base::TimeDelta::FromMilliseconds(500));
  // Note: There is a single timeout for the process running in cleanup
  // execution mode, that will include both the scanner and the cleaner steps.
  // Even though the process in cleanup execution mode doesn't wait for a user
  // response, we still want to test that scenario in case requirements change
  // in the future.
  test_main_controller()->set_cleaning_watchdog_timeout(
      base::TimeDelta::FromMilliseconds(200));

  // Only scanning mode should set a user-response watchdog.
  if (scanning_mode()) {
    ExpectDeath(RESULT_CODE_WATCHDOG_TIMEOUT_WAITING_FOR_PROMPT_RESPONSE);
  } else if (cleanup_mode()) {
    ExpectDeath(RESULT_CODE_WATCHDOG_TIMEOUT_CLEANING);
  } else {
    ExpectSuccess(RESULT_CODE_SUCCESS);
  }
}

TEST_P(MainControllerWatchdogTest, CleanerHangs) {
  SimpleTestPUPData test_pup_data(kFakePupId, PUPData::FLAGS_ACTION_REMOVE);
  test_scanner()->found_pups_.push_back(kFakePupId);
  test_main_controller()->set_cleaning_watchdog_timeout(
      base::TimeDelta::FromMilliseconds(50));
  test_cleaner()->delay_before_done_ = base::TimeDelta::FromMilliseconds(500);

  if (cleanup_mode()) {
    ExpectDeath(RESULT_CODE_WATCHDOG_TIMEOUT_CLEANING);
  } else {
    ExpectSuccess(RESULT_CODE_SUCCESS);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         MainControllerWatchdogTest,
                         testing::ValuesIn(kExecutionModes),
                         GetParamNameForTest());

}  // namespace chrome_cleaner
