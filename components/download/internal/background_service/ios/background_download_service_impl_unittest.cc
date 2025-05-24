// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/ios/background_download_service_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/download/internal/background_service/client_set.h"
#include "components/download/internal/background_service/ios/background_download_task_helper.h"
#include "components/download/internal/background_service/test/black_hole_log_sink.h"
#include "components/download/internal/background_service/test/mock_file_monitor.h"
#include "components/download/internal/background_service/test/test_store.h"
#include "components/download/public/background_service/test/empty_logger.h"
#include "components/download/public/background_service/test/mock_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using ::base::test::RunCallback;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::NiceMock;
using ServiceStatus = download::BackgroundDownloadService::ServiceStatus;
using StartResult = download::DownloadParams::StartResult;

const char kURL[] = "https://www.example.com/test";
const char kGuid[] = "1234";
const char kGuid1[] = "5678";
const char kGuid2[] = "aabb";
const char kGuid3[] = "yyds";
const base::FilePath::CharType kFilePath[] =
    FILE_PATH_LITERAL("downloaded_file.zip");
const char kCompletionHistogram[] = "Download.Service.Finish.Type";
const char kStartResultHistogram[] = "Download.Service.Request.StartResult";
const char kServiceStartUpResultHistogram[] =
    "Download.Service.StartUpStatus.Initialization";

namespace download {
namespace {

MATCHER_P(CompletionInfoIs, file_path, "") {
  return arg.path == file_path;
}

class MockBackgroundDownloadTaskHelper : public BackgroundDownloadTaskHelper {
 public:
  MockBackgroundDownloadTaskHelper() = default;
  ~MockBackgroundDownloadTaskHelper() override = default;
  MOCK_METHOD(void,
              StartDownload,
              (const std::string& guid,
               const base::FilePath& target_path,
               const RequestParams&,
               const SchedulingParams&,
               CompletionCallback,
               UpdateCallback),
              (override));
};

// Test fixture for BackgroundDownloadServiceImpl.
class BackgroundDownloadServiceImplTest : public PlatformTest {
 protected:
  BackgroundDownloadServiceImplTest() = default;
  ~BackgroundDownloadServiceImplTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    auto store = std::make_unique<test::TestStore>();
    store_ = store.get();
    auto model = std::make_unique<ModelImpl>(std::move(store));
    model_ = model.get();
    auto client = std::make_unique<NiceMock<test::MockClient>>();
    client_ = client.get();
    auto clients = std::make_unique<DownloadClientMap>();
    clients->insert(std::make_pair(DownloadClient::TEST, std::move(client)));
    auto client_set = std::make_unique<ClientSet>(std::move(clients));
    auto download_helper = std::make_unique<MockBackgroundDownloadTaskHelper>();
    download_helper_ = download_helper.get();
    auto file_monitor = std::make_unique<NiceMock<MockFileMonitor>>();
    file_monitor_ = file_monitor.get();
    auto logger = std::make_unique<test::EmptyLogger>();
    service_ = std::make_unique<BackgroundDownloadServiceImpl>(
        std::move(client_set), std::move(model), std::move(download_helper),
        std::move(file_monitor), dir_.GetPath(), std::move(logger), &log_sink_,
        &clock_);
    ON_CALL(*file_monitor_, DeleteUnknownFiles(_, _, _))
        .WillByDefault(base::test::RunOnceCallbackRepeatedly<2>());
    service_->Initialize(base::DoNothing());
  }

  InitializableBackgroundDownloadService* service() { return service_.get(); }
  std::unique_ptr<std::vector<Entry>> empty_entries() {
    return std::make_unique<std::vector<Entry>>();
  }
  DownloadParams CreateDownloadParams(const std::string& url) {
    DownloadParams download_params;
    download_params.client = DownloadClient::TEST;
    download_params.guid = kGuid;
    download_params.callback = start_callback_.Get();
    download_params.request_params.url = GURL(url);
    download_params.custom_data["foo"] = "foobar";
    return download_params;
  }

  void Init() { InitWithData(empty_entries()); }

  // Initializes with preloaded |entries| from the database.
  void InitWithData(std::unique_ptr<std::vector<Entry>> entries) {
    store_->TriggerInit(/*success=*/true, std::move(entries));
    file_monitor_->TriggerInit(/*success=*/true);
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir dir_;
  base::SimpleTestClock clock_;
  MockBackgroundDownloadTaskHelper* download_helper_;
  test::TestStore* store_;
  Model* model_;
  test::MockClient* client_;
  base::MockCallback<DownloadParams::StartCallback> start_callback_;
  MockFileMonitor* file_monitor_;
  base::HistogramTester histogram_tester_;

 private:
  test::BlackHoleLogSink log_sink_;
  std::unique_ptr<InitializableBackgroundDownloadService> service_;
};

TEST_F(BackgroundDownloadServiceImplTest, InitSuccess) {
  EXPECT_EQ(ServiceStatus::STARTING_UP, service()->GetStatus());
  EXPECT_CALL(*client_, OnServiceInitialized(false, _));
  Init();
  EXPECT_EQ(ServiceStatus::READY, service()->GetStatus());
  histogram_tester_.ExpectBucketCount(kServiceStartUpResultHistogram,
                                      stats::StartUpResult::SUCCESS, 1);
}

TEST_F(BackgroundDownloadServiceImplTest, InitDbFailure) {
  EXPECT_EQ(ServiceStatus::STARTING_UP, service()->GetStatus());
  EXPECT_CALL(*client_, OnServiceUnavailable());
  store_->TriggerInit(/*success=*/false, empty_entries());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(ServiceStatus::UNAVAILABLE, service()->GetStatus());
  histogram_tester_.ExpectBucketCount(
      kServiceStartUpResultHistogram,
      stats::StartUpResult::FAILURE_REASON_MODEL, 1);
}

TEST_F(BackgroundDownloadServiceImplTest, InitFileMonitorFailure) {
  EXPECT_EQ(ServiceStatus::STARTING_UP, service()->GetStatus());
  EXPECT_CALL(*client_, OnServiceUnavailable());
  store_->TriggerInit(/*success=*/true, empty_entries());
  file_monitor_->TriggerInit(/*success=*/false);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(ServiceStatus::UNAVAILABLE, service()->GetStatus());
  histogram_tester_.ExpectBucketCount(
      kServiceStartUpResultHistogram,
      stats::StartUpResult::FAILURE_REASON_FILE_MONITOR, 1);
}

// Db records that are not associated with any registered clients or unfinished
// downloads should be pruned.
TEST_F(BackgroundDownloadServiceImplTest, InitDbPruned) {
  EXPECT_EQ(ServiceStatus::STARTING_UP, service()->GetStatus());
  EXPECT_CALL(*client_, OnServiceInitialized(false, _));
  std::unique_ptr<std::vector<Entry>> entries =
      std::make_unique<std::vector<Entry>>();

  // Build an entry without a valid client.
  Entry entry_invalid_client;
  entry_invalid_client.state = Entry::State::COMPLETE;
  entry_invalid_client.guid = kGuid;
  entry_invalid_client.create_time = clock_.Now();
  entries->emplace_back(std::move(entry_invalid_client));

  // Build unfinished entry.
  Entry entry_unfinished;
  entry_unfinished.client = DownloadClient::TEST;
  entry_unfinished.state = Entry::State::ACTIVE;
  entry_unfinished.guid = kGuid1;
  entry_unfinished.create_time = clock_.Now();
  entries->emplace_back(std::move(entry_unfinished));

  // Build an expired entry.
  Entry entry_expired;
  entry_expired.client = DownloadClient::TEST;
  entry_expired.state = Entry::State::COMPLETE;
  entry_expired.guid = kGuid2;
  entry_expired.create_time = clock_.Now() - base::Days(300);
  entries->emplace_back(std::move(entry_expired));

  // Build a completed entry that should be kept.
  Entry entry;
  entry.client = DownloadClient::TEST;
  entry.state = Entry::State::COMPLETE;
  entry.guid = kGuid3;
  entry.create_time = clock_.Now();
  entries->emplace_back(std::move(entry));

  InitWithData(std::move(entries));
  EXPECT_EQ(ServiceStatus::READY, service()->GetStatus());
  EXPECT_FALSE(model_->Get(kGuid))
      << "Entry with invalid client should be pruned.";
  EXPECT_FALSE(model_->Get(kGuid1)) << "Unfinished entry should be pruned.";
  EXPECT_FALSE(model_->Get(kGuid2)) << "Expired entry should be pruned.";
  EXPECT_TRUE(model_->Get(kGuid3)) << "This entry should be kept.";
}

TEST_F(BackgroundDownloadServiceImplTest, StartDownloadDbFailure) {
  Init();
  EXPECT_CALL(start_callback_, Run(kGuid, StartResult::INTERNAL_ERROR));
  auto download_params = CreateDownloadParams(kURL);
  service()->StartDownload(std::move(download_params));
  store_->TriggerUpdate(/*success=*/false);
  EXPECT_EQ(kGuid, store_->LastUpdatedEntry()->guid);
  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectBucketCount(
      kStartResultHistogram, DownloadParams::StartResult::INTERNAL_ERROR, 1);
}

// Verifies the case for failure in platform api.
TEST_F(BackgroundDownloadServiceImplTest, StartDownloadHelperFailure) {
  Init();
  EXPECT_CALL(start_callback_, Run(kGuid, StartResult::ACCEPTED));
  EXPECT_CALL(*download_helper_, StartDownload(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<4>(/*success=*/false, base::FilePath(), 0));
  EXPECT_CALL(*client_,
              OnDownloadFailed(kGuid, CompletionInfoIs(base::FilePath()),
                               download::Client::FailureReason::UNKNOWN));
  auto download_params = CreateDownloadParams(kURL);
  service()->StartDownload(std::move(download_params));
  store_->TriggerUpdate(/*success=*/true);
  EXPECT_EQ(kGuid, store_->LastRemovedEntry());
  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectBucketCount(kCompletionHistogram,
                                      CompletionType::FAIL, 1);
}

// Verifies the case for a successful download.
TEST_F(BackgroundDownloadServiceImplTest, StartDownloadSuccess) {
  Init();
  EXPECT_CALL(start_callback_, Run(kGuid, StartResult::ACCEPTED));
  EXPECT_CALL(*download_helper_, StartDownload(_, _, _, _, _, _))
      .WillOnce(
          RunOnceCallback<4>(/*success=*/true, base::FilePath(kFilePath), 0));
  EXPECT_CALL(
      *client_,
      OnDownloadSucceeded(kGuid, CompletionInfoIs(base::FilePath(kFilePath))));
  auto download_params = CreateDownloadParams(kURL);
  service()->StartDownload(std::move(download_params));
  store_->TriggerUpdate(/*success=*/true);
  EXPECT_EQ(kGuid, store_->LastUpdatedEntry()->guid);
  EXPECT_EQ(clock_.Now(), store_->LastUpdatedEntry()->create_time);
  EXPECT_EQ(clock_.Now(), store_->LastUpdatedEntry()->completion_time);
  EXPECT_EQ(Entry::State::COMPLETE, store_->LastUpdatedEntry()->state);
  EXPECT_EQ(dir_.GetPath().AppendASCII(kGuid),
            store_->LastUpdatedEntry()->target_file_path);
  EXPECT_EQ("foobar", store_->LastUpdatedEntry()->custom_data.at("foo"));
  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectBucketCount(kCompletionHistogram,
                                      CompletionType::SUCCEED, 1);
}

// Verifies Client::OnDownloadUpdated() is called.
TEST_F(BackgroundDownloadServiceImplTest, OnDownloadUpdated) {
  Init();
  EXPECT_CALL(start_callback_, Run(kGuid, StartResult::ACCEPTED));
  EXPECT_CALL(*download_helper_, StartDownload(_, _, _, _, _, _))
      .WillOnce(RunCallback<5>(10u));
  EXPECT_CALL(*client_, OnDownloadUpdated(kGuid, 0u, 10u));
  auto download_params = CreateDownloadParams(kURL);
  service()->StartDownload(std::move(download_params));

  // Advance the time to make sure the update is not throttled.
  clock_.Advance(base::Seconds(11));
  store_->TriggerUpdate(/*success=*/true);
  EXPECT_EQ(kGuid, store_->LastUpdatedEntry()->guid);
  EXPECT_EQ(10u, store_->LastUpdatedEntry()->bytes_downloaded);
  EXPECT_EQ("foobar", store_->LastUpdatedEntry()->custom_data.at("foo"));
  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace download
