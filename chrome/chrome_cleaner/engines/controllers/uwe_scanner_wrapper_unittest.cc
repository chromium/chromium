// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/controllers/uwe_scanner_wrapper.h"

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/chrome_cleaner/chrome_utils/extension_id.h"
#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "chrome/chrome_cleaner/engines/broker/engine_client_mock.h"
#include "chrome/chrome_cleaner/engines/common/engine_result_codes.h"
#include "chrome/chrome_cleaner/engines/controllers/scanner_impl.h"
#include "chrome/chrome_cleaner/logging/logging_service_api.h"
#include "chrome/chrome_cleaner/logging/mock_logging_service.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "chrome/chrome_cleaner/pup_data/test_uws.h"
#include "chrome/chrome_cleaner/settings/settings.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_pup_data.h"
#include "chrome/chrome_cleaner/test/test_settings_util.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "chrome/chrome_cleaner/test/test_uws_catalog.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

using ::chrome_cleaner::FoilUwSData_Attribute_FLAGS_STATE_CONFIRMED_UWS;
using ::chrome_cleaner::UwEMatcher;
using testing::_;
using testing::AllOf;
using testing::Contains;
using testing::DoAll;
using testing::Invoke;
using testing::Not;
using testing::Property;
using testing::Return;
using testing::WithArg;

const unsigned int kUwSId1 = kGoogleTestAUwSID;
const unsigned int kUwSId2 = kGoogleTestBUwSID;

// Indices of EngineClient::StartScan's arguments.
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

struct ReportDone {
  explicit ReportDone(uint32_t status) : status_(status) {}

  void operator()(EngineClient::DoneCallback* done_callback) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*done_callback), status_));
  }

  uint32_t status_;
};

class UwEScannerWrapperTest : public testing::Test {
 public:
  UwEScannerWrapperTest() {
    test_pup_data_.Reset({&TestUwSCatalog::GetInstance()});
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ON_CALL(mock_settings_, locations_to_scan())
        .WillByDefault(testing::ReturnRef(empty_trace_locations_));
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

  void RunScanAndExpect(ResultCode expected_result,
                        const std::vector<UwSId>& expected_uws,
                        const std::vector<ExtensionID>& expected_uwe) {
    expected_result_ = expected_result;
    expected_uws_ = expected_uws;
    expected_uwe_ = expected_uwe;
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
    PUPData::PUP* pup = PUPData::GetPUP(new_uws);
    ASSERT_TRUE(pup);
    if (!pup->matched_extensions.empty()) {
      ExtensionID new_uwe = pup->matched_extensions.back().id;
      EXPECT_EQ(found_uwe_.find(new_uwe), found_uwe_.end());
      found_uwe_.insert(new_uwe);
    }
  }

  void Done(ResultCode status, const std::vector<UwSId>& final_found_uws) {
    // Done should only be called once.
    EXPECT_FALSE(done_called_);
    done_called_ = true;

    EXPECT_EQ(expected_result_, status);

    std::vector<ExtensionID> final_found_uwe;
    for (const UwSId& id : final_found_uws) {
      PUPData::PUP* pup = PUPData::GetPUP(id);
      for (const ForceInstalledExtension& extension : pup->matched_extensions) {
        final_found_uwe.push_back(extension.id);
      }
    }

    EXPECT_THAT(final_found_uws,
                testing::UnorderedElementsAreArray(expected_uws_));
    EXPECT_THAT(final_found_uwe,
                testing::UnorderedElementsAreArray(expected_uwe_));

    // Some found UwS may be discarded during the final validation stage, so
    // the |final_found_uws| reported here should be a subset of |found_uws_|,
    // which was accumulated through the callbacks.
    for (const UwSId& id : final_found_uws) {
      PUPData::PUP* pup = PUPData::GetPUP(id);
      for (const ForceInstalledExtension& extension : pup->matched_extensions) {
        EXPECT_THAT(found_uwe_, Contains(extension.id));
      }
      EXPECT_THAT(found_uws_, Contains(id));
    }
  }

  bool ReportUwSWithFile(int uws_id,
                         const wchar_t* file_name,
                         ReportUwS* report_functor) {
    if (!report_functor) {
      return false;
    }
    base::FilePath file_path = temp_dir_.GetPath().Append(file_name);
    if (!CreateEmptyFile(file_path)) {
      return false;
    }
    *report_functor = ReportUwS(uws_id, file_path.value().c_str());
    return true;
  }

 protected:
  void RunScanLoop(bool stop_immediately = false) {
    done_called_ = false;
    scanner_->Start(
        base::BindRepeating(&UwEScannerWrapperTest::FoundUwS,
                            base::Unretained(this)),
        base::BindOnce(&UwEScannerWrapperTest::Done, base::Unretained(this)));
    if (stop_immediately)
      scanner_->Stop();
    while (!scanner_->IsCompletelyDone())
      base::RunLoop().RunUntilIdle();
  }

  UwEMatchers matchers_;
  TestPUPData test_pup_data_;
  base::test::SingleThreadTaskEnvironment task_environment_;

  scoped_refptr<StrictMockEngineClient> mock_engine_client_{
      base::MakeRefCounted<StrictMockEngineClient>()};
  MockSettings mock_settings_;
  std::unique_ptr<UwEScannerWrapper> scanner_;

  std::set<UwSId> found_uws_;
  std::set<ExtensionID> found_uwe_;
  bool done_called_ = false;

  ResultCode expected_result_;
  std::vector<UwSId> expected_uws_;
  std::vector<ExtensionID> expected_uwe_;

  base::ScopedTempDir temp_dir_;
  const std::vector<UwS::TraceLocation> empty_trace_locations_;
};

}  // namespace

TEST_F(UwEScannerWrapperTest, NoUwEFound) {
  const std::vector<ForceInstalledExtension> extensions;
  scanner_ = std::make_unique<UwEScannerWrapper>(
      std::make_unique<ScannerImpl>(mock_engine_client_.get()), &matchers_,
      extensions);
  ExpectStartScanCall(DoAll(
      WithArg<kDoneCallbackPos>(Invoke(ReportDone(EngineResultCode::kSuccess))),
      Return(EngineResultCode::kSuccess)));
  RunScanAndExpect(RESULT_CODE_SUCCESS, {}, {});
}

TEST_F(UwEScannerWrapperTest, FoundUwE) {
  ExtensionID report_found_extension1 =
      ExtensionID::Create(kGoogleTestAUwEID).value();
  ReportUwS report_found_uws1;
  ForceInstalledExtension matched1(report_found_extension1,
                                   POLICY_EXTENSION_FORCELIST, "*", "");
  ASSERT_TRUE(ReportUwSWithFile(kUwSId1, L"bad_file.exe", &report_found_uws1));

  ExtensionID report_found_extension2 =
      ExtensionID::Create(kGoogleTestBUwEID).value();
  ReportUwS report_found_uws2;
  ForceInstalledExtension matched2(report_found_extension2,
                                   POLICY_EXTENSION_FORCELIST, "*", "");
  ASSERT_TRUE(ReportUwSWithFile(kUwSId2, L"bad.bat", &report_found_uws2));

  UwEMatcher matcher1;
  matcher1.add_uws_id(kGoogleTestAUwSID);
  matcher1.set_flags(FoilUwSData_Attribute_FLAGS_STATE_CONFIRMED_UWS);

  UwEMatcher_MatcherCriteria criteria1;
  criteria1.add_extension_id(
      ExtensionID::Create(kGoogleTestAUwEID).value().AsString());
  criteria1.add_update_url("*");
  criteria1.add_install_method(UwEMatcher::POLICY_EXTENSION_FORCELIST);
  *matcher1.mutable_criteria() = criteria1;

  UwEMatcher matcher2;
  matcher2.add_uws_id(kGoogleTestBUwSID);
  matcher2.set_flags(FoilUwSData_Attribute_FLAGS_STATE_CONFIRMED_UWS);

  UwEMatcher_MatcherCriteria criteria2;
  criteria2.add_extension_id(
      ExtensionID::Create(kGoogleTestBUwEID).value().AsString());
  criteria2.add_update_url("*");
  criteria2.add_install_method(UwEMatcher::POLICY_EXTENSION_FORCELIST);
  *matcher2.mutable_criteria() = criteria2;

  UwEMatchers matchers;
  *matchers.mutable_uwe_matcher()->Add() = matcher1;
  *matchers.mutable_uwe_matcher()->Add() = matcher2;

  const std::vector<ForceInstalledExtension> extensions{matched1, matched2};

  scanner_ = std::make_unique<UwEScannerWrapper>(
      std::make_unique<ScannerImpl>(mock_engine_client_.get()), &matchers,
      extensions);

  ExpectStartScanCall(DoAll(
      WithArg<kFoundUwSCallbackPos>(Invoke(report_found_uws1)),
      WithArg<kFoundUwSCallbackPos>(Invoke(report_found_uws2)),
      WithArg<kDoneCallbackPos>(Invoke(ReportDone(EngineResultCode::kSuccess))),
      Return(EngineResultCode::kSuccess)));

  RunScanAndExpect(RESULT_CODE_SUCCESS, {kUwSId1, kUwSId2},
                   {report_found_extension1, report_found_extension2});
}

TEST_F(UwEScannerWrapperTest, ScanFailure) {
  const std::vector<ForceInstalledExtension> extensions;
  scanner_ = std::make_unique<UwEScannerWrapper>(
      std::make_unique<ScannerImpl>(mock_engine_client_.get()), &matchers_,
      extensions);
  ExpectStartScanCall(DoAll(WithArg<kDoneCallbackPos>(Invoke(
                                ReportDone(EngineResultCode::kEngineInternal))),
                            Return(EngineResultCode::kSuccess)));

  RunScanAndExpect(RESULT_CODE_SCANNING_ENGINE_ERROR, {}, {});
}

}  // namespace chrome_cleaner
