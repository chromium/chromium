// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/controllers/scanner_controller_impl.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/chrome_cleaner/engines/broker/engine_client_mock.h"
#include "chrome/chrome_cleaner/logging/logging_service_api.h"
#include "chrome/chrome_cleaner/logging/mock_logging_service.h"
#include "chrome/chrome_cleaner/logging/registry_logger.h"
#include "chrome/chrome_cleaner/parsers/shortcut_parser/broker/fake_shortcut_parser.h"
#include "chrome/chrome_cleaner/pup_data/test_uws.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_pup_data.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "chrome/chrome_cleaner/test/test_uws_catalog.h"
#include "components/chrome_cleaner/test/test_name_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::Unused;
using ::testing::WithArg;
using ::testing::WithArgs;

namespace chrome_cleaner {

namespace {

const unsigned int kUwSId1 = kGoogleTestAUwSID;
const unsigned int kUwSId2 = kGoogleTestBUwSID;
const unsigned int kInvalidUwSId = 42;

// Indices of EngineClient::StartScan's arguments.
constexpr int kFoundUwSCallbackPos = 3;
constexpr int kDoneCallbackPos = 4;

// Functor to call FoundUwSCallback with predefined arguments.
struct FoundUwSInvocation {
  FoundUwSInvocation(UwSId pup_id, const PUPData::PUP& pup)
      : pup_id_(pup_id), pup_(pup) {}

  UwSId pup_id_ = 0;
  PUPData::PUP pup_;
};

class CapturingScannerControllerImpl : public ScannerControllerImpl {
 public:
  CapturingScannerControllerImpl(
      EngineClient* engine_client,
      RegistryLogger* registry_logger,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      ShortcutParserAPI* shortcut_parser)
      : ScannerControllerImpl(engine_client,
                              registry_logger,
                              task_runner,
                              shortcut_parser) {}

  void TestFoundUwSIds(const std::vector<UwSId>& expected_pup_ids) const {
    EXPECT_EQ(pup_ids_.size(), expected_pup_ids.size());
    for (auto id : expected_pup_ids) {
      EXPECT_NE(std::find(pup_ids_.begin(), pup_ids_.end(), id),
                pup_ids_.end());
    }
  }

  void set_watchdog_timeout_in_seconds(uint32_t timeout) {
    watchdog_timeout_in_seconds_ = timeout;
  }

 protected:
  void DoneScanning(ResultCode status,
                    const std::vector<UwSId>& found_pups) override {
    status_ = status;
    pup_ids_ = found_pups;
    ScannerControllerImpl::DoneScanning(status, found_pups);
  }

 private:
  std::vector<UwSId> pup_ids_;
  ResultCode status_;
};

// |InvokeUploadResultCallback| is required because GMock's InvokeArgument
// action expects to use operator(), and a Callback only provides Run().
void InvokeUploadResultCallback(
    const LoggingServiceAPI::UploadResultCallback& upload_result_callback) {
  upload_result_callback.Run(true);
}

// Clears |pup| and optionally adds one file with the given |file_path|.
void PopulateSimplePUP(const wchar_t* file_path, PUPData::PUP* pup) {
  pup->ClearDiskFootprints();
  if (file_path)
    pup->AddDiskFootprint(base::FilePath(file_path));
}

class ScannerControllerImplTest : public ::testing::Test {
 public:
  // The following functions each call |found_callback| and/or |done_callback|
  // in various orders and then return EngineResultCode::kSuccess to
  // simulate the successful start of a scan. Note that |done_callback| will
  // also take a result code showing the end result of the scan.

  // Simulates a successful search with found UwS by calling |found_callback|
  // several times, then |done_callback| with EngineResultCode::kSuccess.
  uint32_t FireScanCallbacks(EngineClient::FoundUwSCallback found_callback,
                             EngineClient::DoneCallback* done_callback);

  // Simulates an internal bug in the sandboxed engine that the calling code
  // should deal gracefully with, by calling |found_callback| and
  // |done_callback| in the wrong order.
  uint32_t FireScanCallbacksWrongOrder(
      EngineClient::FoundUwSCallback found_callback,
      EngineClient::DoneCallback* done_callback);

  // Simulates an internal bug in the sandboxed engine that the calling code
  // should deal gracefully with, by returning
  // EngineResultCode::kWrongState but then calling |found_callback| and
  // |done_callback| anyway.
  uint32_t FireScanCallbacksAfterFailure(
      EngineClient::FoundUwSCallback found_callback,
      EngineClient::DoneCallback* done_callback);

  // Simulates an error which is caught in the sandboxed engine by calling
  // |found_callback| and then |done_callback| with
  // EngineResultCode::kEngineInternal.
  uint32_t FireScanFailureCallbacks(
      EngineClient::FoundUwSCallback found_callback,
      EngineClient::DoneCallback* done_callback);

  // Simulates a successful search including one UwS without any files by
  // calling |found_callback|, then |done_callback| with
  // EngineResultCode::kSuccess.
  uint32_t FireScanCallbacksWithoutFiles(
      EngineClient::FoundUwSCallback found_callback,
      EngineClient::DoneCallback* done_callback);

  // Some tests EXPECT_DEATH in debug builds. To avoid duplicating expectations
  // and logic between debug and release versions of the test, these methods
  // hold common test logic.
  // Note that expected_result_code won't be validated for DEATH tests.
  void RunScanOnly(ResultCode expected_result_code);
  void RunScanOnlyWithoutFiles(ResultCode expected_result_code);
  void RunScanOnlyStartScanFailureWithCallbacks();
  void RunScanOnlyWrongCallbackOrder();

 protected:
  ScannerControllerImplTest()
      : task_runner_(base::SequencedTaskRunnerHandle::Get()),
        test_registry_logger_(RegistryLogger::Mode::NOOP_FOR_TESTING),
        scanner_controller_(mock_engine_client_.get(),
                            &test_registry_logger_,
                            task_runner_,
                            &fake_shortcut_parser_) {
    test_pup_data_.Reset({&TestUwSCatalog::GetInstance()});
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    base::FilePath path_1 = temp_dir_.GetPath().Append(L"path1");
    ASSERT_TRUE(CreateEmptyFile(path_1));
    PopulateSimplePUP(path_1.value().c_str(), &pup_1_);

    base::FilePath path_2 = temp_dir_.GetPath().Append(L"path2");
    ASSERT_TRUE(CreateEmptyFile(path_2));
    PopulateSimplePUP(path_2.value().c_str(), &pup_2_);

    base::FilePath path_3 = temp_dir_.GetPath().Append(L"path3");
    ASSERT_TRUE(CreateEmptyFile(path_3));
    PopulateSimplePUP(path_3.value().c_str(), &pup_1_part2_);

    LoggingServiceAPI::SetInstanceForTesting(&mock_logging_service_);
  }

  void TearDown() override {
    LoggingServiceAPI::SetInstanceForTesting(nullptr);
  }

  void ExpectCallToSendLogsToSafeBrowsing() {
    EXPECT_CALL(mock_logging_service_, SendLogsToSafeBrowsing(_, _))
        .WillOnce(WithArgs<0>(Invoke(InvokeUploadResultCallback)));
  }

  // Sequentially fire all invocation in |found_uws_invocations_|.
  void FireAllFoundUwSInvocations(
      EngineClient::FoundUwSCallback found_callback) {
    for (const FoundUwSInvocation& invocation : found_uws_invocations_) {
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(found_callback, invocation.pup_id_, invocation.pup_));
    }
  }

  // Scoped task environment needs to be created before task runner.
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  scoped_refptr<MockEngineClient> mock_engine_client_{
      base::MakeRefCounted<MockEngineClient>()};
  RegistryLogger test_registry_logger_;
  PUPData::PUP pup_1_;
  PUPData::PUP pup_2_;
  PUPData::PUP pup_1_part2_;
  PUPData::PUP pup_without_files_;
  std::vector<FoundUwSInvocation> found_uws_invocations_;
  CapturingScannerControllerImpl scanner_controller_;
  MockLoggingService mock_logging_service_;
  base::ScopedTempDir temp_dir_;
  FakeShortcutParser fake_shortcut_parser_;

  TestPUPData test_pup_data_;
};

typedef ScannerControllerImplTest ScannerControllerImplDeathTest;

#if DCHECK_IS_ON()
#define ScannerControllerImplTest_MAYBE_DEATH ScannerControllerImplDeathTest
#else
#define ScannerControllerImplTest_MAYBE_DEATH ScannerControllerImplTest
#endif

uint32_t ScannerControllerImplTest::FireScanCallbacks(
    EngineClient::FoundUwSCallback found_callback,
    EngineClient::DoneCallback* done_callback) {
  FireAllFoundUwSInvocations(found_callback);
  task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(*done_callback),
                                                   EngineResultCode::kSuccess));

  return EngineResultCode::kSuccess;
}

uint32_t ScannerControllerImplTest::FireScanCallbacksWrongOrder(
    EngineClient::FoundUwSCallback found_callback,
    EngineClient::DoneCallback* done_callback) {
  task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(*done_callback),
                                                   EngineResultCode::kSuccess));
  found_uws_invocations_.emplace_back(kUwSId1, pup_1_);
  FireAllFoundUwSInvocations(found_callback);

  return EngineResultCode::kSuccess;
}

uint32_t ScannerControllerImplTest::FireScanCallbacksAfterFailure(
    EngineClient::FoundUwSCallback found_callback,
    EngineClient::DoneCallback* done_callback) {
  found_uws_invocations_.emplace_back(kUwSId1, pup_1_);
  FireAllFoundUwSInvocations(found_callback);
  task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(*done_callback),
                                                   EngineResultCode::kSuccess));

  return EngineResultCode::kWrongState;
}

uint32_t ScannerControllerImplTest::FireScanFailureCallbacks(
    EngineClient::FoundUwSCallback found_callback,
    EngineClient::DoneCallback* done_callback) {
  found_uws_invocations_.emplace_back(kUwSId1, pup_1_);
  FireAllFoundUwSInvocations(found_callback);
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(std::move(*done_callback),
                                        EngineResultCode::kSandboxUnavailable));

  return EngineResultCode::kSuccess;
}

uint32_t ScannerControllerImplTest::FireScanCallbacksWithoutFiles(
    EngineClient::FoundUwSCallback found_callback,
    EngineClient::DoneCallback* done_callback) {
  FireAllFoundUwSInvocations(found_callback);
  task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(*done_callback),
                                                   EngineResultCode::kSuccess));

  return EngineResultCode::kSuccess;
}

}  // namespace

void ScannerControllerImplTest::RunScanOnly(ResultCode expected_result_code) {
  // Ensure that we are always turning |include_details| on. If this changes,
  // the expected results in AddDetectedUwS will also change.
  EXPECT_CALL(*mock_engine_client_,
              MockedStartScan(_, _, /*include_details=*/false, _, _))
      .Times(0);
  EXPECT_CALL(*mock_engine_client_,
              MockedStartScan(_, _, /*include_details=*/true, _, _))
      .WillOnce(WithArgs<kFoundUwSCallbackPos, kDoneCallbackPos>(
          Invoke(this, &ScannerControllerImplTest::FireScanCallbacks)));

  EXPECT_CALL(*mock_engine_client_, Finalize())
      .WillOnce(Return(EngineResultCode::kSuccess));

  // AddDetectedUwS should be called with the two overloaded arguments, and no
  // others.
  EXPECT_CALL(mock_logging_service_, AddDetectedUwS(_, _)).Times(0);

  // The callbacks for |pup_1_| and |pup_1_part2_| should be merged
  // into a single call to AddDetectedUwS with 2 files.
  EXPECT_CALL(
      mock_logging_service_,
      AddDetectedUwS(AllOf(PupHasId(kUwSId1), PupHasFileListSize(2)), _));

  // If the second UwS had an invalid ID, it should not be logged at all.
  if (expected_result_code != RESULT_CODE_ENGINE_REPORTED_UNSUPPORTED_UWS) {
    EXPECT_CALL(
        mock_logging_service_,
        AddDetectedUwS(AllOf(PupHasId(kUwSId2), PupHasFileListSize(1)), _));
  }

  EXPECT_CALL(mock_logging_service_, SetExitCode(Eq(expected_result_code)))
      .Times(1);
  ExpectCallToSendLogsToSafeBrowsing();

  EXPECT_EQ(expected_result_code, scanner_controller_.ScanOnly());
}

void ScannerControllerImplTest::RunScanOnlyWithoutFiles(
    ResultCode expected_result_code) {
  // Ensure that we are always turning |include_details| on. If this changes,
  // the expected results in AddDetectedUwS will also change. Namely if we
  // don't request details, we shouldn't be ignoring UwS that has no details.
  EXPECT_CALL(*mock_engine_client_,
              MockedStartScan(_, _, /*include_details=*/false, _, _))
      .Times(0);
  EXPECT_CALL(*mock_engine_client_,
              MockedStartScan(_, _, /*include_details=*/true, _, _))
      .WillOnce(WithArgs<kFoundUwSCallbackPos, kDoneCallbackPos>(Invoke(
          this, &ScannerControllerImplTest::FireScanCallbacksWithoutFiles)));

  EXPECT_CALL(*mock_engine_client_, Finalize())
      .WillOnce(Return(EngineResultCode::kSuccess));

  // It should appear that no UwS was found, since the only UwS that was
  // detected had no files.
  EXPECT_CALL(mock_logging_service_, AddDetectedUwS(_, _)).Times(0);
  EXPECT_CALL(mock_logging_service_, SetExitCode(Eq(expected_result_code)))
      .Times(1);
  ExpectCallToSendLogsToSafeBrowsing();

  EXPECT_EQ(expected_result_code, scanner_controller_.ScanOnly());

  scanner_controller_.TestFoundUwSIds({});

  // TODO(joenotcharles): Also validate that no UwS was written to the registry
  // logger.
}

TEST_F(ScannerControllerImplTest, ScanOnly) {
  found_uws_invocations_.emplace_back(kUwSId1, pup_1_);
  found_uws_invocations_.emplace_back(kUwSId2, pup_2_);
  found_uws_invocations_.emplace_back(kUwSId1, pup_1_part2_);

  RunScanOnly(RESULT_CODE_SUCCESS);

  scanner_controller_.TestFoundUwSIds({kUwSId1, kUwSId2});
  // TODO(joenotcharles): Should this also verify things in the registry logger?
  // Currently getting logs like "Failed to write '42' to found UwS registry
  // entry".
}

TEST_F(ScannerControllerImplTest, ScanOnlyFoundUnsupportedUwS) {
  base::FilePath path = temp_dir_.GetPath().Append(L"path_to_invalid_uws");
  ASSERT_TRUE(CreateEmptyFile(path));
  PopulateSimplePUP(path.value().c_str(), &pup_2_);
  found_uws_invocations_.emplace_back(kUwSId1, pup_1_);
  found_uws_invocations_.emplace_back(kInvalidUwSId, pup_2_);
  found_uws_invocations_.emplace_back(kUwSId1, pup_1_part2_);

  RunScanOnly(RESULT_CODE_ENGINE_REPORTED_UNSUPPORTED_UWS);

  scanner_controller_.TestFoundUwSIds({kUwSId1});
}

TEST_F(ScannerControllerImplTest, ScanOnlyStartScanFailure) {
  EXPECT_CALL(*mock_engine_client_, MockedStartScan(_, _, _, _, _))
      .WillOnce(Return(EngineResultCode::kWrongState));
  EXPECT_CALL(*mock_engine_client_, Finalize()).Times(0);

  EXPECT_CALL(mock_logging_service_,
              SetExitCode(Eq(RESULT_CODE_SCANNING_ENGINE_ERROR)))
      .Times(1);
  ExpectCallToSendLogsToSafeBrowsing();

  EXPECT_EQ(RESULT_CODE_SCANNING_ENGINE_ERROR, scanner_controller_.ScanOnly());

  scanner_controller_.TestFoundUwSIds({});
}

void ScannerControllerImplTest::RunScanOnlyStartScanFailureWithCallbacks() {
  EXPECT_CALL(*mock_engine_client_, MockedStartScan(_, _, _, _, _))
      .WillOnce(WithArgs<kFoundUwSCallbackPos, kDoneCallbackPos>(Invoke(
          this, &ScannerControllerImplTest::FireScanCallbacksAfterFailure)));

  EXPECT_CALL(mock_logging_service_,
              SetExitCode(Eq(RESULT_CODE_SCANNING_ENGINE_ERROR)))
      .Times(1);
  ExpectCallToSendLogsToSafeBrowsing();

  EXPECT_EQ(RESULT_CODE_SCANNING_ENGINE_ERROR, scanner_controller_.ScanOnly());
}

TEST_F(ScannerControllerImplTest_MAYBE_DEATH,
       ScanOnlyStartScanFailureWithCallbacks) {
#if DCHECK_IS_ON()
  ASSERT_DEATH({ RunScanOnlyStartScanFailureWithCallbacks(); }, "");
#else
  RunScanOnlyStartScanFailureWithCallbacks();
#endif

  scanner_controller_.TestFoundUwSIds({});
}

TEST_F(ScannerControllerImplTest, ScanOnlyStartScanSuccessWithCallbackFailure) {
  EXPECT_CALL(*mock_engine_client_, MockedStartScan(_, _, _, _, _))
      .WillOnce(WithArgs<kFoundUwSCallbackPos, kDoneCallbackPos>(
          Invoke(this, &ScannerControllerImplTest::FireScanFailureCallbacks)));
  EXPECT_CALL(*mock_engine_client_, Finalize()).Times(1);

  EXPECT_CALL(mock_logging_service_, AddDetectedUwS(_, _)).Times(1);
  EXPECT_CALL(mock_logging_service_,
              SetExitCode(Eq(RESULT_CODE_SCANNING_ENGINE_ERROR)))
      .Times(1);
  ExpectCallToSendLogsToSafeBrowsing();

  EXPECT_EQ(RESULT_CODE_SCANNING_ENGINE_ERROR, scanner_controller_.ScanOnly());

  scanner_controller_.TestFoundUwSIds({kUwSId1});
}

void ScannerControllerImplTest::RunScanOnlyWrongCallbackOrder() {
  EXPECT_CALL(*mock_engine_client_, MockedStartScan(_, _, _, _, _))
      .WillOnce(WithArgs<kFoundUwSCallbackPos, kDoneCallbackPos>(Invoke(
          this, &ScannerControllerImplTest::FireScanCallbacksWrongOrder)));
  EXPECT_CALL(*mock_engine_client_, Finalize())
      .WillOnce(Return(EngineResultCode::kSuccess));

  EXPECT_CALL(mock_logging_service_, SetExitCode(Eq(RESULT_CODE_NO_PUPS_FOUND)))
      .Times(1);
  ExpectCallToSendLogsToSafeBrowsing();

  scanner_controller_.ScanOnly();
}

TEST_F(ScannerControllerImplTest_MAYBE_DEATH, ScanOnlyWrongCallbackOrder) {
#if DCHECK_IS_ON()
  ASSERT_DEATH({ RunScanOnlyWrongCallbackOrder(); }, "");
#else
  RunScanOnlyWrongCallbackOrder();
#endif

  scanner_controller_.TestFoundUwSIds({});
}

TEST_F(ScannerControllerImplTest, ScanOnlyWithoutFiles) {
  PopulateSimplePUP(nullptr, &pup_without_files_);
  found_uws_invocations_.emplace_back(kUwSId1, pup_without_files_);
  RunScanOnlyWithoutFiles(RESULT_CODE_NO_PUPS_FOUND);
}

TEST_F(ScannerControllerImplTest, ScanOnlyWithDisappearingFile) {
  // Use a file name that doesn't have a file on disk, to simulate a file
  // that's deleted during scanning.
  PopulateSimplePUP(L"C:\\missing_file.txt", &pup_without_files_);
  found_uws_invocations_.emplace_back(kUwSId1, pup_without_files_);
  RunScanOnlyWithoutFiles(RESULT_CODE_NO_PUPS_FOUND);
}

TEST_F(ScannerControllerImplTest, ScanOnlyFoundUnsupportedUwSWithoutFiles) {
  PopulateSimplePUP(nullptr, &pup_without_files_);
  found_uws_invocations_.emplace_back(kInvalidUwSId, pup_without_files_);
  RunScanOnlyWithoutFiles(RESULT_CODE_ENGINE_REPORTED_UNSUPPORTED_UWS);
}

TEST_F(ScannerControllerImplTest,
       ScanOnlyFoundUnsupportedUwSWithDisappearingFile) {
  // Use a file name that doesn't have a file on disk, to simulate a file
  // that's deleted during scanning.
  PopulateSimplePUP(L"C:\\missing_file.txt", &pup_without_files_);
  found_uws_invocations_.emplace_back(kInvalidUwSId, pup_without_files_);
  RunScanOnlyWithoutFiles(RESULT_CODE_ENGINE_REPORTED_UNSUPPORTED_UWS);
}

TEST_F(ScannerControllerImplTest, ReportFoundModifiedTargetPathInShortcuts) {
  ShortcutInformation fake_parsed_shortcut;
  fake_parsed_shortcut.lnk_path = base::FilePath(L"C:\\Fake\\path");
  fake_parsed_shortcut.target_path = L"C:\\fake\\path\\whatever.exe";
  fake_shortcut_parser_.SetShortcutsToReturn({fake_parsed_shortcut});
  EXPECT_CALL(mock_logging_service_, SetFoundModifiedChromeShortcuts(true));
  RunScanOnlyWithoutFiles(RESULT_CODE_NO_PUPS_FOUND);
}

TEST_F(ScannerControllerImplTest, ReportFoundModifiedArgumentsInShortcuts) {
  ShortcutInformation fake_parsed_shortcut;
  fake_parsed_shortcut.lnk_path = base::FilePath(L"C:\\Fake\\path");
  fake_parsed_shortcut.target_path = L"C:\\fake\\path\\chrome.exe";
  fake_parsed_shortcut.command_line_arguments = L"--some-flag";
  fake_shortcut_parser_.SetShortcutsToReturn({fake_parsed_shortcut});
  EXPECT_CALL(mock_logging_service_, SetFoundModifiedChromeShortcuts(true));
  RunScanOnlyWithoutFiles(RESULT_CODE_NO_PUPS_FOUND);
}

TEST_F(ScannerControllerImplTest, ReportNotFoundModifiedShortcuts) {
  ShortcutInformation fake_parsed_shortcut;
  fake_parsed_shortcut.lnk_path = base::FilePath(L"C:\\Fake\\path");
  fake_parsed_shortcut.target_path = L"C:\\fake\\path\\chrome.exe";

  fake_shortcut_parser_.SetShortcutsToReturn({fake_parsed_shortcut});

  EXPECT_CALL(mock_logging_service_, SetFoundModifiedChromeShortcuts(false));
  RunScanOnlyWithoutFiles(RESULT_CODE_NO_PUPS_FOUND);
}

TEST_F(ScannerControllerImplTest, TimeoutDuringStartScan) {
  scanner_controller_.set_watchdog_timeout_in_seconds(1);

  ON_CALL(*mock_engine_client_, MockedStartScan(_, _, _, _, _))
      .WillByDefault(DoAll(
          // Sleep for more than a second.
          InvokeWithoutArgs([]() { ::Sleep(1100); }),
          // The return value should be ignored.
          Return(RESULT_CODE_INVALID)));

  EXPECT_EXIT({ scanner_controller_.ScanOnly(); },
              ::testing::ExitedWithCode(
                  RESULT_CODE_WATCHDOG_TIMEOUT_WITHOUT_REMOVABLE_UWS),
              "");
}

TEST_F(ScannerControllerImplTest, TimeoutBeforeDone) {
  scanner_controller_.set_watchdog_timeout_in_seconds(1);

  auto report_uws_found = [this](EngineClient::FoundUwSCallback callback) {
    // kUwSId2 is a removable UwS.
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(callback, kUwSId2, pup_2_));
  };

  // The |done_callback| won't be called, so the watchdog should terminate the
  // process.
  ON_CALL(*mock_engine_client_, MockedStartScan(_, _, _, _, _))
      .WillByDefault(
          DoAll(WithArg<kFoundUwSCallbackPos>(Invoke(report_uws_found)),
                Return(EngineResultCode::kSuccess)));

  EXPECT_EXIT({ scanner_controller_.ScanOnly(); },
              ::testing::ExitedWithCode(
                  RESULT_CODE_WATCHDOG_TIMEOUT_WITH_REMOVABLE_UWS),
              "");
}

}  // namespace chrome_cleaner
