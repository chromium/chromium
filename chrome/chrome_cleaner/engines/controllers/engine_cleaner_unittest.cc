// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/chrome_cleaner/engines/broker/engine_client_mock.h"
#include "chrome/chrome_cleaner/engines/common/engine_result_codes.h"
#include "chrome/chrome_cleaner/engines/controllers/engine_cleaner.h"
#include "chrome/chrome_cleaner/logging/logging_service_api.h"
#include "chrome/chrome_cleaner/logging/mock_logging_service.h"
#include "chrome/chrome_cleaner/logging/proto/removal_status.pb.h"
#include "chrome/chrome_cleaner/os/file_removal_status_updater.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "chrome/chrome_cleaner/pup_data/test_uws.h"
#include "chrome/chrome_cleaner/settings/settings.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_pup_data.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "chrome/chrome_cleaner/test/test_uws_catalog.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::WithArg;

unsigned int kScanOnlyUwS = kGoogleTestAUwSID;
unsigned int kRemovableUwS = kGoogleTestBUwSID;

MATCHER_P(EqPathCaseInsensitive, path, "") {
  return base::EqualsCaseInsensitiveASCII(path.value(), arg.value());
}

class EngineCleanerTest : public testing::Test {
 public:
  EngineCleanerTest() {
    test_pup_data_.Reset({&TestUwSCatalog::GetInstance()});
  }

  void SetUp() override {
    LoggingServiceAPI::SetInstanceForTesting(&mock_logging_service_);

    engine_cleaner_ =
        std::make_unique<EngineCleaner>(mock_engine_client_.get());

    ASSERT_TRUE(PUPData::IsKnownPUP(kScanOnlyUwS));
    ASSERT_FALSE(PUPData::IsConfirmedUwS(kScanOnlyUwS));
    ASSERT_FALSE(PUPData::IsRemovable(kScanOnlyUwS));
    ASSERT_TRUE(PUPData::IsKnownPUP(kRemovableUwS));
    ASSERT_TRUE(PUPData::IsConfirmedUwS(kRemovableUwS));
    ASSERT_TRUE(PUPData::IsRemovable(kRemovableUwS));
  }

  void TearDown() override {
    LoggingServiceAPI::SetInstanceForTesting(nullptr);
    Settings::SetInstanceForTesting(nullptr);
  }

  void RunCleanup(const std::vector<UwSId>& pup_ids) {
    engine_cleaner_->Start(pup_ids, base::BindOnce(&EngineCleanerTest::Done,
                                                   base::Unretained(this)));
    while (!engine_cleaner_->IsCompletelyDone())
      base::RunLoop().RunUntilIdle();
  }

  void RunCleanupAndStop(const std::vector<UwSId>& pup_ids) {
    engine_cleaner_->Start(pup_ids, base::BindOnce(&EngineCleanerTest::Done,
                                                   base::Unretained(this)));
    engine_cleaner_->Stop();
    while (!engine_cleaner_->IsCompletelyDone())
      base::RunLoop().RunUntilIdle();
  }

  void Done(ResultCode status) { done_status_ = status; }

 protected:
  scoped_refptr<StrictMockEngineClient> mock_engine_client_{
      base::MakeRefCounted<StrictMockEngineClient>()};
  MockLoggingService mock_logging_service_;

  TestPUPData test_pup_data_;
  std::unique_ptr<EngineCleaner> engine_cleaner_;

  base::test::SingleThreadTaskEnvironment task_environment_;
  ResultCode done_status_ = RESULT_CODE_INVALID;
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

}  // namespace

TEST_F(EngineCleanerTest, Start_Failure) {
  EXPECT_CALL(*mock_engine_client_, MockedStartCleanup(_, _))
      .WillOnce(Return(EngineResultCode::kEngineInternal));
  EXPECT_CALL(*mock_engine_client_, needs_reboot()).WillOnce(Return(false));

  RunCleanup({kRemovableUwS});

  EXPECT_EQ(RESULT_CODE_FAILED, done_status_);
}

TEST_F(EngineCleanerTest, Stop) {
  EXPECT_CALL(*mock_engine_client_, MockedStartCleanup(_, _))
      .WillOnce(Return(EngineResultCode::kSuccess));
  EXPECT_CALL(*mock_engine_client_, needs_reboot()).WillOnce(Return(false));

  RunCleanupAndStop({kRemovableUwS});

  EXPECT_EQ(RESULT_CODE_CANCELED, done_status_);
}

TEST_F(EngineCleanerTest, Clean_Success) {
  std::vector<UwSId> saved_enabled_uws;
  auto save_enabled_uws =
      [&saved_enabled_uws](const std::vector<UwSId>& enabled_uws) {
        saved_enabled_uws = enabled_uws;
      };
  EXPECT_CALL(*mock_engine_client_, MockedStartCleanup(_, _))
      .WillOnce(
          DoAll(WithArg<0>(Invoke(save_enabled_uws)),
                WithArg<1>(Invoke(ReportDone(EngineResultCode::kSuccess))),
                Return(EngineResultCode::kSuccess)));
  EXPECT_CALL(*mock_engine_client_, needs_reboot()).WillOnce(Return(false));
  EXPECT_CALL(mock_logging_service_, AllExpectedRemovalsConfirmed())
      .WillOnce(Return(true));

  RunCleanup({kRemovableUwS});

  EXPECT_THAT(saved_enabled_uws,
              ::testing::UnorderedElementsAre(kRemovableUwS));
  EXPECT_EQ(RESULT_CODE_SUCCESS, done_status_);
}

TEST_F(EngineCleanerTest, Clean_NotAllDetectedFilesRemoved) {
  EXPECT_CALL(*mock_engine_client_, MockedStartCleanup(_, _))
      .WillOnce(
          DoAll(WithArg<1>(Invoke(ReportDone(EngineResultCode::kSuccess))),
                Return(EngineResultCode::kSuccess)));
  EXPECT_CALL(*mock_engine_client_, needs_reboot()).WillOnce(Return(false));
  EXPECT_CALL(mock_logging_service_, AllExpectedRemovalsConfirmed())
      .WillOnce(Return(false));

  RunCleanup({kRemovableUwS});

  EXPECT_EQ(RESULT_CODE_FAILED, done_status_);
}

TEST_F(EngineCleanerTest, Reboot_NotAllDetectedFilesRemoved) {
  EXPECT_CALL(*mock_engine_client_, MockedStartCleanup(_, _))
      .WillOnce(
          DoAll(WithArg<1>(Invoke(ReportDone(EngineResultCode::kSuccess))),
                Return(EngineResultCode::kSuccess)));
  EXPECT_CALL(*mock_engine_client_, needs_reboot()).WillOnce(Return(true));
  EXPECT_CALL(mock_logging_service_, AllExpectedRemovalsConfirmed())
      .WillOnce(Return(false));

  RunCleanup({kRemovableUwS});

  EXPECT_EQ(RESULT_CODE_PENDING_REBOOT, done_status_);
}

TEST_F(EngineCleanerTest, UpdatesRemovalStatusForMissingMatchedFiles) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath missing_file = temp_dir.GetPath().Append(L"missing.exe");
  base::FilePath existing_file = temp_dir.GetPath().Append(L"existing.exe");
  ASSERT_TRUE(CreateEmptyFile(existing_file));

  PUPData::PUP* removable_pup = PUPData::GetPUP(kRemovableUwS);
  removable_pup->AddDiskFootprint(missing_file);
  removable_pup->AddDiskFootprint(existing_file);

  FileRemovalStatusUpdater* removal_status_updater =
      FileRemovalStatusUpdater::GetInstance();
  removal_status_updater->Clear();

  EXPECT_CALL(*mock_engine_client_, MockedStartCleanup(_, _))
      .WillOnce(
          DoAll(WithArg<1>(Invoke(ReportDone(EngineResultCode::kSuccess))),
                Return(EngineResultCode::kSuccess)));
  EXPECT_CALL(*mock_engine_client_, needs_reboot()).WillOnce(Return(false));

  RunCleanup({kRemovableUwS});

  EXPECT_EQ(removal_status_updater->GetRemovalStatus(missing_file),
            REMOVAL_STATUS_NOT_FOUND);
  EXPECT_EQ(removal_status_updater->GetRemovalStatus(existing_file),
            REMOVAL_STATUS_UNSPECIFIED);
}

TEST_F(EngineCleanerTest, Clean_Failure) {
  EXPECT_CALL(*mock_engine_client_, MockedStartCleanup(_, _))
      .WillOnce(DoAll(
          WithArg<1>(Invoke(ReportDone(EngineResultCode::kSandboxUnavailable))),
          Return(EngineResultCode::kSuccess)));
  EXPECT_CALL(*mock_engine_client_, needs_reboot()).WillOnce(Return(false));

  RunCleanup({kRemovableUwS});

  EXPECT_EQ(RESULT_CODE_FAILED, done_status_);
}

}  // namespace chrome_cleaner
