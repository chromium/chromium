// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/controllers/scanner_impl.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/chrome_cleaner/engines/broker/engine_client_mock.h"
#include "chrome/chrome_cleaner/engines/common/engine_result_codes.h"
#include "chrome/chrome_cleaner/logging/logging_service_api.h"
#include "chrome/chrome_cleaner/logging/mock_logging_service.h"
#include "chrome/chrome_cleaner/pup_data/test_uws.h"
#include "chrome/chrome_cleaner/settings/settings.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_pup_data.h"
#include "chrome/chrome_cleaner/test/test_settings_util.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "chrome/chrome_cleaner/test/test_uws_catalog.h"
#include "components/chrome_cleaner/test/test_name_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

using testing::_;
using testing::AllOf;
using testing::Contains;
using testing::DoAll;
using testing::Invoke;
using testing::Not;
using testing::Property;
using testing::Return;
using testing::ReturnRef;
using testing::WithArg;

const unsigned int kUwSId1 = kGoogleTestAUwSID;
const unsigned int kUwSId2 = kGoogleTestBUwSID;
const unsigned int kInvalidUwSID = 42;

// Indices of EngineClient::StartScan's arguments.
constexpr int kEnabledUwSArgPos = 0;
constexpr int kEnabledLocationsArgPos = 1;
constexpr int kFoundUwSCallbackPos = 3;
constexpr int kDoneCallbackPos = 4;

// Functor to call FoundUwSCallback with predefined arguments.
struct ReportUwS {
  ReportUwS() = default;

  ReportUwS(UwSId pup_id, const wchar_t* file_path) {
    pup_id_ = pup_id;

    if (file_path) {
      file_path_ = base::FilePath(file_path);
      pup_.AddDiskFootprint(file_path_);
    }
  }

  void operator()(EngineClient::FoundUwSCallback found_uws_callback) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(found_uws_callback, pup_id_, pup_));
  }

  base::FilePath file_path_;
  UwSId pup_id_ = 0;
  PUPData::PUP pup_;
};

// Functor to call DoneCallback with predefined arguments.
struct ReportDone {
  explicit ReportDone(uint32_t status) : status_(status) {}

  void operator()(EngineClient::DoneCallback* done_callback) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*done_callback), status_));
  }

  uint32_t status_;
};

class ScannerImplTest : public ::testing::Test {
 public:
  ScannerImplTest() : scanner_(mock_engine_client_.get()) {
    test_pup_data_.Reset({&TestUwSCatalog::GetInstance()});
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    static const std::vector<UwS::TraceLocation> kEmptyTraceLocations;
    ON_CALL(mock_settings_, locations_to_scan())
        .WillByDefault(ReturnRef(kEmptyTraceLocations));

    Settings::SetInstanceForTesting(&mock_settings_);
  }

  void TearDown() override {
    // Few tests use mocked logging service. Here we reset it.
    LoggingServiceAPI::SetInstanceForTesting(nullptr);
    Settings::SetInstanceForTesting(nullptr);
  }

  template <typename Action>
  void ExpectStartScanCall(const Action& action) {
    static const std::vector<UwSId> kEnabledUwS{kUwSId1, kUwSId2};

    // Every call to StartScan should be preceded by GetEnabledUwS.
    ::testing::InSequence s;
    EXPECT_CALL(*mock_engine_client_, GetEnabledUwS())
        .WillOnce(Return(kEnabledUwS));
    EXPECT_CALL(*mock_engine_client_,
                MockedStartScan(_, _, /*include_details=*/true, _, _))
        .WillOnce(action);
  }

  bool ReportUwSWithFile(int uws_id,
                         const wchar_t* file_name,
                         ReportUwS* report_functor) {
    base::FilePath file_path = temp_dir_.GetPath().Append(file_name);
    if (!CreateEmptyFile(file_path))
      return false;
    *report_functor = ReportUwS(uws_id, file_path.value().c_str());
    return true;
  }

  void RunScanAndExpect(ResultCode expected_result,
                        const std::vector<UwSId>& expected_uws) {
    expected_result_ = expected_result;
    expected_uws_ = expected_uws;
    RunScanLoop();
  }

  void RunScanAndStop() {
    expected_result_ = RESULT_CODE_CANCELED;
    expected_uws_.clear();
    RunScanLoop(/*stop_immediately=*/true);
  }

  void FoundUwS(UwSId new_uws) {
    EXPECT_EQ(found_uws_.find(new_uws), found_uws_.end());
    found_uws_.insert(new_uws);
  }

  void Done(ResultCode status, const std::vector<UwSId>& final_found_uws) {
    // Done should only be called once.
    EXPECT_FALSE(done_called_);
    done_called_ = true;

    EXPECT_EQ(expected_result_, status);

    EXPECT_THAT(final_found_uws,
                testing::UnorderedElementsAreArray(expected_uws_));

    // Some found UwS may be discarded during the final validation stage, so
    // the |final_found_uws| reported here should be a subset of |found_uws_|,
    // which was accumulated through the callbacks.
    for (const UwSId& id : final_found_uws)
      EXPECT_THAT(found_uws_, Contains(id));
  }

 protected:
  void RunScanLoop(bool stop_immediately = false) {
    done_called_ = false;
    scanner_.Start(
        base::BindRepeating(&ScannerImplTest::FoundUwS, base::Unretained(this)),
        base::BindOnce(&ScannerImplTest::Done, base::Unretained(this)));
    if (stop_immediately)
      scanner_.Stop();
    while (!scanner_.IsCompletelyDone())
      base::RunLoop().RunUntilIdle();
  }

  TestPUPData test_pup_data_;
  base::test::SingleThreadTaskEnvironment task_environment_;

  scoped_refptr<StrictMockEngineClient> mock_engine_client_{
      base::MakeRefCounted<StrictMockEngineClient>()};
  MockSettings mock_settings_;
  ScannerImpl scanner_;

  std::set<UwSId> found_uws_;
  bool done_called_ = false;

  ResultCode expected_result_;
  std::vector<UwSId> expected_uws_;

  base::ScopedTempDir temp_dir_;
};

void ValidateEnabledUwS(const std::vector<UwSId>& enabled_uws) {
  EXPECT_THAT(enabled_uws, Contains(kGoogleTestAUwSID));
  EXPECT_THAT(enabled_uws, Contains(kGoogleTestBUwSID));
  EXPECT_THAT(enabled_uws, Not(Contains(kInvalidUwSID)));
}

void ValidateEnabledLocations(
    const std::vector<UwS::TraceLocation>& enabled_locations) {
  EXPECT_THAT(enabled_locations,
              testing::UnorderedElementsAre(UwS::FOUND_IN_STARTUP,
                                            UwS::FOUND_IN_MODULES));
}

}  // namespace

TEST_F(ScannerImplTest, StartScan_Failure) {
  ExpectStartScanCall(Return(EngineResultCode::kEngineInternal));

  RunScanAndExpect(RESULT_CODE_SCANNING_ENGINE_ERROR, {});
}

TEST_F(ScannerImplTest, StartScan_ProvidesEnabledUwS) {
  ExpectStartScanCall(
      DoAll(WithArg<kEnabledUwSArgPos>(Invoke(ValidateEnabledUwS)),
            Return(EngineResultCode::kEngineInternal)));

  RunScanAndExpect(RESULT_CODE_SCANNING_ENGINE_ERROR, {});
}

TEST_F(ScannerImplTest, StartScan_ProvidesEnabledTraceLocations) {
  std::vector<UwS::TraceLocation> locations = {UwS::FOUND_IN_STARTUP,
                                               UwS::FOUND_IN_MODULES};
  EXPECT_CALL(mock_settings_, locations_to_scan())
      .WillRepeatedly(ReturnRef(locations));
  ExpectStartScanCall(
      DoAll(WithArg<kEnabledLocationsArgPos>(Invoke(ValidateEnabledLocations)),
            Return(EngineResultCode::kEngineInternal)));

  MockLoggingService mock_logging_service;
  LoggingServiceAPI::SetInstanceForTesting(&mock_logging_service);
  EXPECT_CALL(mock_logging_service,
              SetScannedLocations(testing::ElementsAreArray(locations)));

  RunScanAndExpect(RESULT_CODE_SCANNING_ENGINE_ERROR, {});
}

TEST_F(ScannerImplTest, StopScan) {
  ExpectStartScanCall(Return(EngineResultCode::kSuccess));

  RunScanAndStop();
}

TEST_F(ScannerImplTest, NoUwSFound) {
  ExpectStartScanCall(DoAll(
      WithArg<kDoneCallbackPos>(Invoke(ReportDone(EngineResultCode::kSuccess))),
      Return(EngineResultCode::kSuccess)));

  RunScanAndExpect(RESULT_CODE_SUCCESS, {});
}

TEST_F(ScannerImplTest, FoundUwS) {
  ReportUwS report_found_uws1;
  ASSERT_TRUE(ReportUwSWithFile(kUwSId1, L"file.exe", &report_found_uws1));

  ReportUwS report_found_uws2;
  ASSERT_TRUE(
      ReportUwSWithFile(kUwSId2, L"very_bad_file.exe", &report_found_uws2));

  ExpectStartScanCall(DoAll(
      WithArg<kFoundUwSCallbackPos>(Invoke(report_found_uws1)),
      WithArg<kFoundUwSCallbackPos>(Invoke(report_found_uws2)),
      WithArg<kDoneCallbackPos>(Invoke(ReportDone(EngineResultCode::kSuccess))),
      Return(EngineResultCode::kSuccess)));

  RunScanAndExpect(RESULT_CODE_SUCCESS, {kUwSId1, kUwSId2});
}

TEST_F(ScannerImplTest, ScanFailure) {
  ExpectStartScanCall(DoAll(WithArg<kDoneCallbackPos>(Invoke(
                                ReportDone(EngineResultCode::kEngineInternal))),
                            Return(EngineResultCode::kSuccess)));

  RunScanAndExpect(RESULT_CODE_SCANNING_ENGINE_ERROR, {});
}

TEST_F(ScannerImplTest, LogsFoundUwS) {
  ReportUwS report_found_uws1a;
  ASSERT_TRUE(ReportUwSWithFile(kUwSId1, L"file.exe", &report_found_uws1a));

  ReportUwS report_found_uws1b;
  ASSERT_TRUE(
      ReportUwSWithFile(kUwSId1, L"another_file.exe", &report_found_uws1b));

  ReportUwS report_found_uws2;
  ASSERT_TRUE(
      ReportUwSWithFile(kUwSId2, L"very_bad_file.exe", &report_found_uws2));

  MockLoggingService mock_logging_service;
  LoggingServiceAPI::SetInstanceForTesting(&mock_logging_service);

  ExpectStartScanCall(DoAll(
      WithArg<kFoundUwSCallbackPos>(Invoke(report_found_uws1a)),
      WithArg<kFoundUwSCallbackPos>(Invoke(report_found_uws1b)),
      WithArg<kFoundUwSCallbackPos>(Invoke(report_found_uws2)),
      WithArg<kDoneCallbackPos>(Invoke(ReportDone(EngineResultCode::kSuccess))),
      Return(EngineResultCode::kSuccess)));

  EXPECT_CALL(
      mock_logging_service,
      AddDetectedUwS(AllOf(PupHasId(kUwSId1), PupHasFileListSize(2)), _));
  EXPECT_CALL(
      mock_logging_service,
      AddDetectedUwS(AllOf(PupHasId(kUwSId2), PupHasFileListSize(1)), _));

  RunScanAndExpect(RESULT_CODE_SUCCESS, {kUwSId1, kUwSId2});
}

TEST_F(ScannerImplTest, LogsDoneCallbackOperation) {
  ReportUwS report_found_uws;
  ASSERT_TRUE(ReportUwSWithFile(kUwSId1, L"file.exe", &report_found_uws));

  ExpectStartScanCall(
      DoAll(WithArg<kFoundUwSCallbackPos>(Invoke(report_found_uws)),
            WithArg<kDoneCallbackPos>(
                Invoke(ReportDone(EngineResultCode::kNotEnoughSpace))),
            Return(EngineResultCode::kSuccess)));

  // Expect to find one UwS from the ReportUwSWithFile call even though
  // ReportDone included an error code.
  RunScanAndExpect(RESULT_CODE_SCANNING_ENGINE_ERROR, {kUwSId1});
}

TEST_F(ScannerImplTest, StoresFoundUwSInPupData) {
  ReportUwS report_found_uws1;
  ASSERT_TRUE(ReportUwSWithFile(kUwSId1, L"file.exe", &report_found_uws1));

  ReportUwS report_found_uws2;
  ASSERT_TRUE(
      ReportUwSWithFile(kUwSId1, L"another_file.exe", &report_found_uws2));

  ExpectStartScanCall(DoAll(
      WithArg<kFoundUwSCallbackPos>(Invoke(report_found_uws1)),
      WithArg<kFoundUwSCallbackPos>(Invoke(report_found_uws2)),
      WithArg<kDoneCallbackPos>(Invoke(ReportDone(EngineResultCode::kSuccess))),
      Return(EngineResultCode::kSuccess)));

  RunScanAndExpect(RESULT_CODE_SUCCESS, {kUwSId1});

  const PUPData::PUP* pup = PUPData::GetPUP(kUwSId1);
  ASSERT_NE(nullptr, pup);
  ASSERT_EQ(2ULL, pup->expanded_disk_footprints.size());
  ASSERT_TRUE(
      pup->expanded_disk_footprints.Contains(report_found_uws1.file_path_));
  ASSERT_TRUE(
      pup->expanded_disk_footprints.Contains(report_found_uws2.file_path_));
}

TEST_F(ScannerImplTest, FoundDisabledUwS) {
  MockLoggingService mock_logging_service;
  LoggingServiceAPI::SetInstanceForTesting(&mock_logging_service);

  ReportUwS report_found_uws;
  ASSERT_TRUE(ReportUwSWithFile(kUwSId1, L"file.exe", &report_found_uws));

  ReportUwS report_invalid_uws;
  ASSERT_TRUE(ReportUwSWithFile(kInvalidUwSID, L"very_bad_file.exe",
                                &report_invalid_uws));

  ExpectStartScanCall(DoAll(
      WithArg<kFoundUwSCallbackPos>(Invoke(report_found_uws)),
      WithArg<kFoundUwSCallbackPos>(Invoke(report_invalid_uws)),
      WithArg<kDoneCallbackPos>(Invoke(ReportDone(EngineResultCode::kSuccess))),
      Return(EngineResultCode::kSuccess)));

  // Should log only the valid UwS.
  EXPECT_CALL(
      mock_logging_service,
      AddDetectedUwS(AllOf(PupHasId(kUwSId1), PupHasFileListSize(1)), _));

  // Expect the valid UwS to be found, but the final result code is
  // RESULT_CODE_ENGINE_REPORTED_UNSUPPORTED_UWS because this is a serious
  // error.
  RunScanAndExpect(RESULT_CODE_ENGINE_REPORTED_UNSUPPORTED_UWS, {kUwSId1});

  // Disabled UwS should have been reported through the FoundUwS callback even
  // though only the valid UwS is included in the final result.
  EXPECT_THAT(found_uws_,
              testing::UnorderedElementsAre(kUwSId1, kInvalidUwSID));

  // Note that disabled UwS should not be added to PUPData. This can't easily
  // be tested because GetPUP(invalid_id) will assert in debug builds but not
  // release builds. But if the code is changed to attempt to add it, the test
  // will assert while adding the invalid UwS anyway.
}

TEST_F(ScannerImplTest, FoundUwSWithoutFiles) {
  MockLoggingService mock_logging_service;
  LoggingServiceAPI::SetInstanceForTesting(&mock_logging_service);

  // This UwS has no files at all.
  ReportUwS report_found_uws1(kUwSId1, nullptr);

  // This UwS has a file, but that file will not be found on disk. This
  // simulates the case where a file is deleted while the scanner is running.
  ReportUwS report_found_uws2(kUwSId2, L"C:\\file.exe");

  ExpectStartScanCall(DoAll(
      WithArg<kFoundUwSCallbackPos>(Invoke(report_found_uws1)),
      WithArg<kFoundUwSCallbackPos>(Invoke(report_found_uws2)),
      WithArg<kDoneCallbackPos>(Invoke(ReportDone(EngineResultCode::kSuccess))),
      Return(EngineResultCode::kSuccess)));

  // Should not log any detected UwS.
  EXPECT_CALL(mock_logging_service, AddDetectedUwS(_, _)).Times(0);

  RunScanAndExpect(RESULT_CODE_SUCCESS, {});

  // Should not add the detected UwS to PUPData.
  for (auto id : {kUwSId1, kUwSId2}) {
    const PUPData::PUP* pup = PUPData::GetPUP(id);
    ASSERT_NE(nullptr, pup);
    ASSERT_EQ(0ULL, pup->expanded_disk_footprints.size());
  }

  // Disabled UwS should have been reported through the FoundUwS callback even
  // though only the valid UwS is included in the final result.
  EXPECT_THAT(found_uws_, testing::UnorderedElementsAre(kUwSId1, kUwSId2));
}

TEST_F(ScannerImplTest, FoundDisabledUwSWithoutFiles) {
  MockLoggingService mock_logging_service;
  LoggingServiceAPI::SetInstanceForTesting(&mock_logging_service);
  ReportUwS report_found_uws(kInvalidUwSID, nullptr);

  ExpectStartScanCall(DoAll(
      WithArg<kFoundUwSCallbackPos>(Invoke(report_found_uws)),
      WithArg<kDoneCallbackPos>(Invoke(ReportDone(EngineResultCode::kSuccess))),
      Return(EngineResultCode::kSuccess)));

  // Should not log any detected UwS.
  EXPECT_CALL(mock_logging_service, AddDetectedUwS(_, _)).Times(0);

  RunScanAndExpect(RESULT_CODE_ENGINE_REPORTED_UNSUPPORTED_UWS, {});

  // UwS without files and disabled UwS should have been reported through the
  // FoundUwS callback event though only the valid UwS is included in the final
  // result.
  EXPECT_THAT(found_uws_, testing::UnorderedElementsAre(kInvalidUwSID));

  // Note that disabled UwS should not be added to PUPData. This can't easily
  // be tested because GetPUP(invalid_id) will assert in debug builds but not
  // release builds. But if the code is changed to attempt to add it, the test
  // will assert while adding the invalid UwS anyway.
}

}  // namespace chrome_cleaner
