// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/controller_impl.h"

#include <stdint.h>
#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/download/internal/background_service/client_set.h"
#include "components/download/internal/background_service/config.h"
#include "components/download/internal/background_service/entry.h"
#include "components/download/internal/background_service/entry_utils.h"
#include "components/download/internal/background_service/file_monitor.h"
#include "components/download/internal/background_service/model_impl.h"
#include "components/download/internal/background_service/navigation_monitor_impl.h"
#include "components/download/internal/background_service/scheduler/scheduler.h"
#include "components/download/internal/background_service/stats.h"
#include "components/download/internal/background_service/test/black_hole_log_sink.h"
#include "components/download/internal/background_service/test/entry_utils.h"
#include "components/download/internal/background_service/test/test_device_status_listener.h"
#include "components/download/internal/background_service/test/test_download_driver.h"
#include "components/download/internal/background_service/test/test_store.h"
#include "components/download/public/background_service/test/empty_client.h"
#include "components/download/public/background_service/test/mock_client.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;

namespace download {

namespace {

const base::FilePath::CharType kDownloadDirPath[] =
    FILE_PATH_LITERAL("/test/downloads");

bool GuidInEntryList(const std::vector<Entry>& entries,
                     const std::string& guid) {
  for (const auto& entry : entries) {
    if (entry.guid == guid)
      return true;
  }
  return false;
}

DriverEntry BuildDriverEntry(const Entry& entry, DriverEntry::State state) {
  DriverEntry dentry;
  dentry.guid = entry.guid;
  dentry.state = state;
  return dentry;
}

void NotifyTaskFinished(bool success) {}

class UploadClient : public test::MockClient {
 public:
  UploadClient() = default;
  ~UploadClient() override = default;

  void GetUploadData(const std::string& guid,
                     GetUploadDataCallback callback) override;
  void SetUploadResponseDelayForGuid(const std::string& guid,
                                     unsigned int delay);

 private:
  std::map<std::string, unsigned int> upload_response_delay_;

  DISALLOW_COPY_AND_ASSIGN(UploadClient);
};

void UploadClient::GetUploadData(const std::string& guid,
                                 GetUploadDataCallback callback) {
  scoped_refptr<network::ResourceRequestBody> post_body =
      new network::ResourceRequestBody();
  unsigned int delay = upload_response_delay_[guid];
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(std::move(callback), post_body),
      base::TimeDelta::FromSeconds(delay));
}

void UploadClient::SetUploadResponseDelayForGuid(const std::string& guid,
                                                 unsigned int delay) {
  upload_response_delay_[guid] = delay;
}

class MockTaskScheduler : public TaskScheduler {
 public:
  MockTaskScheduler() = default;
  ~MockTaskScheduler() override = default;

  // TaskScheduler implementation.
  MOCK_METHOD6(ScheduleTask,
               void(DownloadTaskType, bool, bool, int, int64_t, int64_t));
  MOCK_METHOD1(CancelTask, void(DownloadTaskType));
};

class MockScheduler : public Scheduler {
 public:
  MockScheduler() = default;
  ~MockScheduler() override = default;

  MOCK_METHOD1(Reschedule, void(const Model::EntryList&));
  MOCK_METHOD2(Next, Entry*(const Model::EntryList&, const DeviceStatus&));
};

class MockFileMonitor : public FileMonitor {
 public:
  MockFileMonitor() = default;
  ~MockFileMonitor() override = default;

  void TriggerInit(bool success);
  void TriggerHardRecover(bool success);

  void Initialize(const FileMonitor::InitCallback& callback) override;
  MOCK_METHOD2(DeleteUnknownFiles,
               void(const Model::EntryList&, const std::vector<DriverEntry>&));
  MOCK_METHOD2(CleanupFilesForCompletedEntries,
               void(const Model::EntryList&, const base::Closure&));
  MOCK_METHOD2(DeleteFiles,
               void(const std::set<base::FilePath>&, stats::FileCleanupReason));
  void HardRecover(const FileMonitor::InitCallback&) override;

 private:
  FileMonitor::InitCallback init_callback_;
  FileMonitor::InitCallback recover_callback_;
};

void MockFileMonitor::TriggerInit(bool success) {
  init_callback_.Run(success);
}

void MockFileMonitor::TriggerHardRecover(bool success) {
  recover_callback_.Run(success);
}

void MockFileMonitor::Initialize(const FileMonitor::InitCallback& callback) {
  init_callback_ = callback;
}

void MockFileMonitor::HardRecover(const FileMonitor::InitCallback& callback) {
  recover_callback_ = callback;
}

class DownloadServiceControllerImplTest : public testing::Test {
 public:
  DownloadServiceControllerImplTest()
      : task_runner_(new base::TestMockTimeTaskRunner),
        handle_(task_runner_),
        controller_(nullptr),
        client_(nullptr),
        driver_(nullptr),
        store_(nullptr),
        model_(nullptr),
        device_status_listener_(nullptr),
        scheduler_(nullptr),
        file_monitor_(nullptr),
        init_callback_called_(false) {
    start_callback_ =
        base::Bind(&DownloadServiceControllerImplTest::StartCallback,
                   base::Unretained(this));
  }

  ~DownloadServiceControllerImplTest() override = default;

  void SetUp() override {
    auto client = std::make_unique<NiceMock<test::MockClient>>();
    auto client3 = std::make_unique<NiceMock<UploadClient>>();
    auto driver = std::make_unique<test::TestDownloadDriver>();
    auto store = std::make_unique<test::TestStore>();
    config_ = std::make_unique<Configuration>();
    config_->max_retry_count = 1;
    config_->max_resumption_count = 4;
    config_->file_keep_alive_time = base::TimeDelta::FromMinutes(10);
    config_->file_cleanup_window = base::TimeDelta::FromMinutes(5);
    config_->max_concurrent_downloads = 5;
    config_->max_running_downloads = 5;

    log_sink_ = std::make_unique<test::BlackHoleLogSink>();

    client_ = client.get();
    client3_ = client3.get();
    driver_ = driver.get();
    store_ = store.get();

    auto clients = std::make_unique<DownloadClientMap>();
    clients->insert(std::make_pair(DownloadClient::TEST, std::move(client)));
    clients->insert(std::make_pair(DownloadClient::TEST_2,
                                   std::make_unique<test::EmptyClient>()));
    clients->insert(std::make_pair(DownloadClient::TEST_3, std::move(client3)));
    auto client_set = std::make_unique<ClientSet>(std::move(clients));
    auto model = std::make_unique<ModelImpl>(std::move(store));
    auto device_status_listener =
        std::make_unique<test::TestDeviceStatusListener>();
    auto scheduler = std::make_unique<NiceMock<MockScheduler>>();
    auto task_scheduler = std::make_unique<MockTaskScheduler>();

    auto download_file_dir = base::FilePath(kDownloadDirPath);
    auto file_monitor = std::make_unique<NiceMock<MockFileMonitor>>();

    model_ = model.get();
    device_status_listener_ = device_status_listener.get();
    scheduler_ = scheduler.get();
    task_scheduler_ = task_scheduler.get();
    file_monitor_ = file_monitor.get();

    controller_ = std::make_unique<ControllerImpl>(
        config_.get(), log_sink_.get(), std::move(client_set),
        std::move(driver), std::move(model), std::move(device_status_listener),
        &navigation_monitor, std::move(scheduler), std::move(task_scheduler),
        std::move(file_monitor), download_file_dir);
  }

 protected:
  void OnInitCompleted() {
    EXPECT_TRUE(controller_->GetState() == Controller::State::READY ||
                controller_->GetState() == Controller::State::UNAVAILABLE);
    init_callback_called_ = true;
  }

  void InitializeController() {
    controller_->Initialize(
        base::Bind(&DownloadServiceControllerImplTest::OnInitCompleted,
                   base::Unretained(this)));
  }

  DownloadParams MakeDownloadParams() {
    DownloadParams params;
    params.client = DownloadClient::TEST;
    params.guid = base::GenerateGUID();
    params.callback = start_callback_;
    return params;
  }

  MOCK_METHOD2(StartCallback,
               void(const std::string&, DownloadParams::StartResult));

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle handle_;

  std::unique_ptr<ControllerImpl> controller_;
  std::unique_ptr<Configuration> config_;
  std::unique_ptr<LogSink> log_sink_;
  NavigationMonitorImpl navigation_monitor;
  test::MockClient* client_;
  UploadClient* client3_;
  test::TestDownloadDriver* driver_;
  test::TestStore* store_;
  ModelImpl* model_;
  test::TestDeviceStatusListener* device_status_listener_;
  MockScheduler* scheduler_;
  MockTaskScheduler* task_scheduler_;
  MockFileMonitor* file_monitor_;

  DownloadParams::StartCallback start_callback_;
  bool init_callback_called_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadServiceControllerImplTest);
};

}  // namespace

TEST_F(DownloadServiceControllerImplTest, SuccessfulInitModelFirst) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(0);
  EXPECT_EQ(controller_->GetState(), Controller::State::CREATED);

  InitializeController();
  EXPECT_TRUE(store_->init_called());
  EXPECT_EQ(controller_->GetState(), Controller::State::INITIALIZING);

  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>());
  file_monitor_->TriggerInit(true);
  EXPECT_EQ(controller_->GetState(), Controller::State::INITIALIZING);

  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  driver_->MakeReady();
  EXPECT_EQ(controller_->GetState(), Controller::State::READY);

  task_runner_->RunUntilIdle();

  histogram_tester.ExpectBucketCount(
      "Download.Service.StartUpStatus.Initialization",
      static_cast<base::HistogramBase::Sample>(stats::StartUpResult::SUCCESS),
      1);
}

TEST_F(DownloadServiceControllerImplTest, SuccessfulInitDriverFirst) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(0);
  EXPECT_EQ(controller_->GetState(), Controller::State::CREATED);

  InitializeController();
  EXPECT_TRUE(store_->init_called());
  EXPECT_EQ(controller_->GetState(), Controller::State::INITIALIZING);

  driver_->MakeReady();
  EXPECT_FALSE(init_callback_called_);
  EXPECT_EQ(controller_->GetState(), Controller::State::INITIALIZING);

  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>());
  file_monitor_->TriggerInit(true);
  EXPECT_EQ(controller_->GetState(), Controller::State::READY);

  task_runner_->RunUntilIdle();
  EXPECT_TRUE(init_callback_called_);

  histogram_tester.ExpectBucketCount(
      "Download.Service.StartUpStatus.Initialization",
      static_cast<base::HistogramBase::Sample>(stats::StartUpResult::SUCCESS),
      1);
}

TEST_F(DownloadServiceControllerImplTest, HardRecoveryAfterFailedModel) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*client_, OnServiceInitialized(true, _)).Times(0);
  EXPECT_EQ(controller_->GetState(), Controller::State::CREATED);

  InitializeController();
  driver_->MakeReady();
  store_->TriggerInit(false, std::make_unique<std::vector<Entry>>());
  file_monitor_->TriggerInit(true);

  EXPECT_EQ(controller_->GetState(), Controller::State::RECOVERING);
  driver_->TriggerHardRecoverComplete(true);
  store_->TriggerHardRecover(true);
  file_monitor_->TriggerHardRecover(true);

  EXPECT_CALL(*client_, OnServiceInitialized(true, _)).Times(1);
  task_runner_->RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      "Download.Service.StartUpStatus.Initialization",
      static_cast<base::HistogramBase::Sample>(stats::StartUpResult::FAILURE),
      1);
  histogram_tester.ExpectBucketCount(
      "Download.Service.StartUpStatus.Initialization",
      static_cast<base::HistogramBase::Sample>(
          stats::StartUpResult::FAILURE_REASON_MODEL),
      1);
  histogram_tester.ExpectBucketCount(
      "Download.Service.StartUpStatus.Recovery",
      static_cast<base::HistogramBase::Sample>(stats::StartUpResult::SUCCESS),
      1);
}

TEST_F(DownloadServiceControllerImplTest, HardRecoveryAfterFailedFileMonitor) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*client_, OnServiceInitialized(true, _)).Times(0);
  EXPECT_EQ(controller_->GetState(), Controller::State::CREATED);

  InitializeController();
  driver_->MakeReady();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>());
  file_monitor_->TriggerInit(false);

  EXPECT_EQ(controller_->GetState(), Controller::State::RECOVERING);
  driver_->TriggerHardRecoverComplete(true);
  store_->TriggerHardRecover(true);
  file_monitor_->TriggerHardRecover(true);

  EXPECT_CALL(*client_, OnServiceInitialized(true, _)).Times(1);
  task_runner_->RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      "Download.Service.StartUpStatus.Initialization",
      static_cast<base::HistogramBase::Sample>(stats::StartUpResult::FAILURE),
      1);
  histogram_tester.ExpectBucketCount(
      "Download.Service.StartUpStatus.Initialization",
      static_cast<base::HistogramBase::Sample>(
          stats::StartUpResult::FAILURE_REASON_FILE_MONITOR),
      1);
  histogram_tester.ExpectBucketCount(
      "Download.Service.StartUpStatus.Recovery",
      static_cast<base::HistogramBase::Sample>(stats::StartUpResult::SUCCESS),
      1);
}

TEST_F(DownloadServiceControllerImplTest, HardRecoveryFails) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*client_, OnServiceInitialized(true, _)).Times(0);
  EXPECT_EQ(controller_->GetState(), Controller::State::CREATED);

  InitializeController();
  driver_->MakeReady();
  store_->TriggerInit(false, std::make_unique<std::vector<Entry>>());
  file_monitor_->TriggerInit(true);

  EXPECT_EQ(controller_->GetState(), Controller::State::RECOVERING);
  driver_->TriggerHardRecoverComplete(true);
  store_->TriggerHardRecover(true);
  file_monitor_->TriggerHardRecover(false);

  EXPECT_CALL(*client_, OnServiceUnavailable()).Times(1);
  task_runner_->RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      "Download.Service.StartUpStatus.Initialization",
      static_cast<base::HistogramBase::Sample>(stats::StartUpResult::FAILURE),
      1);
  histogram_tester.ExpectBucketCount(
      "Download.Service.StartUpStatus.Initialization",
      static_cast<base::HistogramBase::Sample>(
          stats::StartUpResult::FAILURE_REASON_MODEL),
      1);
  histogram_tester.ExpectBucketCount(
      "Download.Service.StartUpStatus.Recovery",
      static_cast<base::HistogramBase::Sample>(stats::StartUpResult::FAILURE),
      1);
  histogram_tester.ExpectBucketCount(
      "Download.Service.StartUpStatus.Recovery",
      static_cast<base::HistogramBase::Sample>(
          stats::StartUpResult::FAILURE_REASON_FILE_MONITOR),
      1);
}

TEST_F(DownloadServiceControllerImplTest, SuccessfulInitWithExistingDownload) {
  Entry entry1 = test::BuildBasicEntry();
  Entry entry2 = test::BuildBasicEntry();
  Entry entry3 =
      test::BuildEntry(DownloadClient::INVALID, base::GenerateGUID());

  std::vector<Entry> entries = {entry1, entry2, entry3};
  std::vector<DownloadMetaData> expected_downloads = {
      util::BuildDownloadMetaData(&entry1, driver_),
      util::BuildDownloadMetaData(&entry2, driver_)};

  EXPECT_CALL(*client_,
              OnServiceInitialized(false, testing::UnorderedElementsAreArray(
                                              expected_downloads)));

  InitializeController();
  driver_->MakeReady();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);

  task_runner_->RunUntilIdle();
  EXPECT_TRUE(init_callback_called_);
}

TEST_F(DownloadServiceControllerImplTest, UnknownFileDeletion) {
  Entry entry1 = test::BuildBasicEntry();
  Entry entry2 = test::BuildBasicEntry();
  Entry entry3 = test::BuildBasicEntry();

  std::vector<Entry> entries = {entry1, entry2, entry3};

  DriverEntry dentry1 =
      BuildDriverEntry(entry1, DriverEntry::State::IN_PROGRESS);
  DriverEntry dentry3 =
      BuildDriverEntry(entry3, DriverEntry::State::IN_PROGRESS);
  std::vector<DriverEntry> dentries = {dentry1, dentry3};

  EXPECT_CALL(*file_monitor_, DeleteUnknownFiles(_, _)).Times(1);

  driver_->AddTestData(dentries);
  InitializeController();
  driver_->MakeReady();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest,
       CleanupTaskCallsFileMonitorAndSchedulesNewTaskInFuture) {
  Entry entry1 = test::BuildBasicEntry();
  Entry entry2 = test::BuildBasicEntry();
  Entry entry3 = test::BuildBasicEntry(Entry::State::COMPLETE);
  entry3.completion_time = base::Time::Now();

  std::vector<Entry> entries = {entry1, entry2, entry3};

  EXPECT_CALL(*file_monitor_, CleanupFilesForCompletedEntries(_, _)).Times(2);
  EXPECT_CALL(*task_scheduler_,
              ScheduleTask(DownloadTaskType::CLEANUP_TASK, _, _, _, _, _))
      .Times(1);

  InitializeController();
  driver_->MakeReady();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);
  controller_->OnStartScheduledTask(DownloadTaskType::CLEANUP_TASK,
                                    base::BindOnce(&NotifyTaskFinished));

  task_runner_->RunUntilIdle();
  controller_->OnStopScheduledTask(DownloadTaskType::CLEANUP_TASK);
}

TEST_F(DownloadServiceControllerImplTest, GetOwnerOfDownload) {
  Entry entry = test::BuildBasicEntry();
  std::vector<Entry> entries = {entry};

  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  InitializeController();
  driver_->MakeReady();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);

  task_runner_->RunUntilIdle();

  EXPECT_EQ(DownloadClient::TEST, controller_->GetOwnerOfDownload(entry.guid));
  EXPECT_EQ(DownloadClient::INVALID,
            controller_->GetOwnerOfDownload(base::GenerateGUID()));
}

TEST_F(DownloadServiceControllerImplTest, AddDownloadAccepted) {
  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  // Set up the Controller.
  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>());
  file_monitor_->TriggerInit(true);
  driver_->MakeReady();
  task_runner_->RunUntilIdle();

  // Trigger the download.
  DownloadParams params = MakeDownloadParams();
  EXPECT_CALL(*this,
              StartCallback(params.guid, DownloadParams::StartResult::ACCEPTED))
      .Times(1);
  controller_->StartDownload(params);

  // TODO(dtrainor): Compare the full DownloadParams with the full Entry.
  store_->TriggerUpdate(true);

  std::vector<Entry> entries = store_->updated_entries();
  Entry entry = entries[0];
  DCHECK_EQ(entry.client, DownloadClient::TEST);
  EXPECT_TRUE(base::StartsWith(entry.target_file_path.value(), kDownloadDirPath,
                               base::CompareCase::SENSITIVE));

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, AddDownloadFailsWithBackoff) {
  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  Entry entry = test::BuildBasicEntry();
  std::vector<Entry> entries = {entry};

  // Set up the Controller.
  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);
  driver_->MakeReady();
  task_runner_->RunUntilIdle();

  // Set the failure expectations.
  config_->max_scheduled_downloads = 1U;

  // Trigger the download.
  DownloadParams params = MakeDownloadParams();
  EXPECT_CALL(*this,
              StartCallback(params.guid, DownloadParams::StartResult::BACKOFF))
      .Times(1);
  controller_->StartDownload(params);

  EXPECT_FALSE(GuidInEntryList(store_->updated_entries(), params.guid));

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest,
       AddDownloadFailsWithDuplicateGuidInModel) {
  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  Entry entry = test::BuildBasicEntry();
  std::vector<Entry> entries = {entry};

  // Set up the Controller.
  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);
  driver_->MakeReady();
  task_runner_->RunUntilIdle();

  // Trigger the download.
  DownloadParams params = MakeDownloadParams();
  params.guid = entry.guid;
  EXPECT_CALL(
      *this,
      StartCallback(params.guid, DownloadParams::StartResult::UNEXPECTED_GUID))
      .Times(1);
  controller_->StartDownload(params);

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, AddDownloadFailsWithDuplicateCall) {
  testing::InSequence sequence;
  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  // Set up the Controller.
  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>());
  file_monitor_->TriggerInit(true);
  driver_->MakeReady();
  task_runner_->RunUntilIdle();

  // Trigger the download twice.
  DownloadParams params = MakeDownloadParams();
  EXPECT_CALL(
      *this,
      StartCallback(params.guid, DownloadParams::StartResult::UNEXPECTED_GUID))
      .Times(1);
  EXPECT_CALL(*this,
              StartCallback(params.guid, DownloadParams::StartResult::ACCEPTED))
      .Times(1);
  controller_->StartDownload(params);
  controller_->StartDownload(params);
  store_->TriggerUpdate(true);

  EXPECT_TRUE(GuidInEntryList(store_->updated_entries(), params.guid));

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, AddDownloadFailsWithBadClient) {
  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  // Set up the Controller.
  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>());
  file_monitor_->TriggerInit(true);
  driver_->MakeReady();
  task_runner_->RunUntilIdle();

  // Trigger the download.
  DownloadParams params = MakeDownloadParams();
  params.client = DownloadClient::INVALID;
  EXPECT_CALL(*this,
              StartCallback(params.guid,
                            DownloadParams::StartResult::UNEXPECTED_CLIENT))
      .Times(1);
  controller_->StartDownload(params);

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, AddDownloadFailsWithClientCancel) {
  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  // Set up the Controller.
  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>());
  file_monitor_->TriggerInit(true);
  driver_->MakeReady();
  task_runner_->RunUntilIdle();

  // Trigger the download.
  DownloadParams params = MakeDownloadParams();
  EXPECT_CALL(
      *this,
      StartCallback(params.guid, DownloadParams::StartResult::CLIENT_CANCELLED))
      .Times(1);
  controller_->StartDownload(params);

  controller_->CancelDownload(params.guid);
  store_->TriggerUpdate(true);

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, AddDownloadFailsWithInternalError) {
  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  // Set up the Controller.
  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>());
  file_monitor_->TriggerInit(true);
  driver_->MakeReady();
  task_runner_->RunUntilIdle();

  // Trigger the download.
  DownloadParams params = MakeDownloadParams();
  EXPECT_CALL(*this, StartCallback(params.guid,
                                   DownloadParams::StartResult::INTERNAL_ERROR))
      .Times(1);
  controller_->StartDownload(params);

  store_->TriggerUpdate(false);

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, Pause) {
  // Setup download service test data.
  Entry entry1 = test::BuildBasicEntry(Entry::State::AVAILABLE);
  Entry entry2 = test::BuildBasicEntry(Entry::State::ACTIVE);
  Entry entry3 = test::BuildBasicEntry(Entry::State::COMPLETE);
  entry3.completion_time = base::Time::Now();
  std::vector<Entry> entries = {entry1, entry2, entry3};

  // Setup download driver test data.
  DriverEntry dentry2 =
      BuildDriverEntry(entry2, DriverEntry::State::IN_PROGRESS);
  DriverEntry dentry3 = BuildDriverEntry(entry3, DriverEntry::State::COMPLETE);
  dentry3.done = true;
  driver_->AddTestData(std::vector<DriverEntry>{dentry2, dentry3});

  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  device_status_listener_->SetDeviceStatus(
      DeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));

  // The default network status is disconnected so no entries will be polled
  // from the scheduler.
  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);
  driver_->MakeReady();

  // Pause in progress available entry.
  EXPECT_EQ(Entry::State::AVAILABLE, model_->Get(entry1.guid)->state);
  controller_->PauseDownload(entry1.guid);
  EXPECT_FALSE(driver_->Find(entry1.guid).has_value());
  EXPECT_EQ(Entry::State::PAUSED, model_->Get(entry1.guid)->state);

  // Pause in progress active entry.
  controller_->PauseDownload(entry2.guid);
  EXPECT_TRUE(driver_->Find(entry2.guid)->paused);
  EXPECT_EQ(Entry::State::PAUSED, model_->Get(entry2.guid)->state);

  // Entries in complete states can't be paused.
  controller_->PauseDownload(entry3.guid);
  EXPECT_FALSE(driver_->Find(entry3.guid).has_value());
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entry3.guid)->state);

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, Resume) {
  // Setup download service test data.
  Entry entry1 = test::BuildBasicEntry(Entry::State::PAUSED);
  Entry entry2 = test::BuildBasicEntry(Entry::State::ACTIVE);
  std::vector<Entry> entries = {entry1, entry2};

  // Setup download driver test data.
  DriverEntry dentry1 =
      BuildDriverEntry(entry1, DriverEntry::State::IN_PROGRESS);
  dentry1.paused = true;
  DriverEntry dentry2 =
      BuildDriverEntry(entry2, DriverEntry::State::IN_PROGRESS);
  driver_->AddTestData(std::vector<DriverEntry>{dentry1, dentry2});

  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  device_status_listener_->SetDeviceStatus(
      DeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));

  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);
  driver_->MakeReady();

  // Resume the paused download.
  EXPECT_EQ(Entry::State::PAUSED, model_->Get(entry1.guid)->state);
  controller_->ResumeDownload(entry1.guid);
  EXPECT_FALSE(driver_->Find(entry1.guid)->paused);
  EXPECT_EQ(Entry::State::ACTIVE, model_->Get(entry1.guid)->state);

  // Entries in active state can't be resumed.
  controller_->ResumeDownload(entry2.guid);
  EXPECT_EQ(Entry::State::ACTIVE, model_->Get(entry2.guid)->state);
  EXPECT_FALSE(driver_->Find(entry2.guid)->paused);

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, Cancel) {
  // Setup download service test data.
  Entry entry = test::BuildBasicEntry(Entry::State::ACTIVE);
  std::vector<Entry> entries = {entry};

  // Setup download driver test data.
  DriverEntry dentry = BuildDriverEntry(entry, DriverEntry::State::IN_PROGRESS);
  driver_->AddTestData(std::vector<DriverEntry>{dentry});

  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);
  EXPECT_CALL(*client_,
              OnDownloadFailed(entry.guid, _, Client::FailureReason::CANCELLED))
      .Times(1);

  device_status_listener_->SetDeviceStatus(
      DeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));
  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);
  driver_->MakeReady();

  controller_->CancelDownload(entry.guid);
  EXPECT_EQ(nullptr, model_->Get(entry.guid));

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, OnDownloadFailed) {
  // Setup download service test data.
  Entry entry = test::BuildBasicEntry(Entry::State::ACTIVE);
  std::vector<Entry> entries = {entry};

  // Setup download driver test data.
  DriverEntry dentry = BuildDriverEntry(entry, DriverEntry::State::IN_PROGRESS);
  driver_->AddTestData(std::vector<DriverEntry>{dentry});

  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);
  EXPECT_CALL(*client_,
              OnDownloadFailed(entry.guid, _, Client::FailureReason::NETWORK))
      .Times(1);

  device_status_listener_->SetDeviceStatus(
      DeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));
  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);
  driver_->MakeReady();

  driver_->NotifyDownloadFailed(dentry, FailureType::NOT_RECOVERABLE);
  EXPECT_EQ(nullptr, model_->Get(entry.guid));

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, OnDownloadFailedFromDriverCancel) {
  // Setup download service test data.
  Entry entry1 = test::BuildBasicEntry(Entry::State::ACTIVE);
  Entry entry2 = test::BuildBasicEntry(Entry::State::ACTIVE);
  std::vector<Entry> entries = {entry1, entry2};

  // Setup download driver test data.
  DriverEntry dentry1 =
      BuildDriverEntry(entry1, DriverEntry::State::IN_PROGRESS);
  DriverEntry dentry2 =
      BuildDriverEntry(entry2, DriverEntry::State::IN_PROGRESS);
  driver_->AddTestData(std::vector<DriverEntry>{dentry1, dentry2});

  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);
  EXPECT_CALL(*client_,
              OnDownloadFailed(entry1.guid, _, Client::FailureReason::NETWORK))
      .Times(1);
  EXPECT_CALL(*client_,
              OnDownloadFailed(entry2.guid, _, Client::FailureReason::NETWORK))
      .Times(1);

  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);
  driver_->MakeReady();

  DriverEntry done_dentry1 =
      BuildDriverEntry(entry1, DriverEntry::State::CANCELLED);
  done_dentry1.done = true;

  DriverEntry done_dentry2 =
      BuildDriverEntry(entry2, DriverEntry::State::CANCELLED);
  done_dentry2.done = true;

  // A "Done" entry should fail even if it is considered recoverable.
  driver_->NotifyDownloadFailed(done_dentry1, FailureType::RECOVERABLE);
  EXPECT_EQ(nullptr, model_->Get(entry1.guid));

  driver_->NotifyDownloadFailed(done_dentry2, FailureType::NOT_RECOVERABLE);
  EXPECT_EQ(nullptr, model_->Get(entry2.guid));

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, NoopResumeDoesNotHitAttemptCounts) {
  // Setup download service test data.
  Entry entry1 = test::BuildBasicEntry(Entry::State::ACTIVE);
  Entry entry2 = test::BuildBasicEntry(Entry::State::ACTIVE);
  entry2.scheduling_params.network_requirements =
      SchedulingParams::NetworkRequirements::UNMETERED;
  std::vector<Entry> entries = {entry1, entry2};

  // Setup download driver test data.
  DriverEntry dentry1 =
      BuildDriverEntry(entry1, DriverEntry::State::IN_PROGRESS);
  DriverEntry dentry2 =
      BuildDriverEntry(entry2, DriverEntry::State::IN_PROGRESS);
  driver_->AddTestData(std::vector<DriverEntry>{dentry1, dentry2});

  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);
  EXPECT_CALL(*client_, OnDownloadFailed(_, _, _)).Times(0);

  device_status_listener_->SetDeviceStatus(
      DeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));

  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);
  driver_->MakeReady();

  config_->max_retry_count = 1;
  config_->max_resumption_count = 1;

  device_status_listener_->NotifyObserver(
      DeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));
  device_status_listener_->NotifyObserver(
      DeviceStatus(BatteryStatus::CHARGING, NetworkStatus::METERED));
  device_status_listener_->NotifyObserver(
      DeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));
  device_status_listener_->NotifyObserver(
      DeviceStatus(BatteryStatus::CHARGING, NetworkStatus::METERED));
  device_status_listener_->NotifyObserver(
      DeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, RetryOnFailure) {
  // Setup download service test data.
  Entry entry1 = test::BuildBasicEntry(Entry::State::ACTIVE);
  Entry entry2 = test::BuildBasicEntry(Entry::State::ACTIVE);
  Entry entry3 = test::BuildBasicEntry(Entry::State::ACTIVE);
  std::vector<Entry> entries = {entry1, entry2, entry3};

  // Setup download driver test data.
  DriverEntry dentry1 =
      BuildDriverEntry(entry1, DriverEntry::State::INTERRUPTED);
  dentry1.can_resume = false;
  DriverEntry dentry2 =
      BuildDriverEntry(entry2, DriverEntry::State::INTERRUPTED);
  dentry2.can_resume = false;
  DriverEntry dentry3 =
      BuildDriverEntry(entry3, DriverEntry::State::INTERRUPTED);
  dentry3.can_resume = true;
  driver_->AddTestData(std::vector<DriverEntry>{dentry1, dentry2, dentry3});

  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  // Set up the Controller.
  device_status_listener_->SetDeviceStatus(
      DeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));

  config_->max_retry_count = 3;
  config_->max_resumption_count = 4;

  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);
  driver_->MakeReady();
  task_runner_->RunUntilIdle();

  // Test retry on failure.
  EXPECT_CALL(*client_, OnDownloadSucceeded(entry1.guid, _)).Times(1);
  base::FilePath path = base::FilePath::FromUTF8Unsafe("123");
  driver_->NotifyDownloadFailed(dentry1, FailureType::RECOVERABLE);
  driver_->NotifyDownloadFailed(dentry1, FailureType::RECOVERABLE);
  driver_->NotifyDownloadSucceeded(dentry1);

  EXPECT_CALL(*client_,
              OnDownloadFailed(entry2.guid, _, Client::FailureReason::NETWORK))
      .Times(1);
  driver_->NotifyDownloadFailed(dentry2, FailureType::RECOVERABLE);
  driver_->NotifyDownloadFailed(dentry2, FailureType::RECOVERABLE);
  driver_->NotifyDownloadFailed(dentry2, FailureType::RECOVERABLE);
  driver_->NotifyDownloadFailed(dentry2, FailureType::RECOVERABLE);
  // Failed entry should exist because we retry after a delay.
  EXPECT_NE(nullptr, model_->Get(entry2.guid));

  task_runner_->FastForwardUntilNoTasksRemain();
  // Retry is done, and failed entry should be removed.
  EXPECT_EQ(nullptr, model_->Get(entry2.guid));

  EXPECT_CALL(*client_,
              OnDownloadFailed(entry3.guid, _, Client::FailureReason::NETWORK))
      .Times(1);
  driver_->NotifyDownloadFailed(dentry3, FailureType::RECOVERABLE);
  driver_->NotifyDownloadFailed(dentry3, FailureType::RECOVERABLE);
  driver_->NotifyDownloadFailed(dentry3, FailureType::RECOVERABLE);
  driver_->NotifyDownloadFailed(dentry3, FailureType::RECOVERABLE);
  driver_->NotifyDownloadFailed(dentry3, FailureType::RECOVERABLE);
  // Failed entry should exist because we retry after a delay.
  EXPECT_NE(nullptr, model_->Get(entry3.guid));

  task_runner_->FastForwardUntilNoTasksRemain();
  // Retry is done, and failed entry should be removed.
  EXPECT_EQ(nullptr, model_->Get(entry2.guid));
}

TEST_F(DownloadServiceControllerImplTest, OnDownloadSucceeded) {
  // Setup download service test data.
  Entry entry = test::BuildBasicEntry(Entry::State::ACTIVE);
  std::vector<Entry> entries = {entry};

  // Setup download driver test data.
  DriverEntry dentry = BuildDriverEntry(entry, DriverEntry::State::IN_PROGRESS);
  driver_->AddTestData(std::vector<DriverEntry>{dentry});

  CompletionInfo completion_info(dentry.current_file_path,
                                 dentry.bytes_downloaded, entry.url_chain,
                                 entry.response_headers);
  completion_info.hash256 = "01234567ABCDEF";
  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);
  EXPECT_CALL(*client_, OnDownloadSucceeded(entry.guid, completion_info))
      .Times(1);

  device_status_listener_->SetDeviceStatus(
      DeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));
  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);
  driver_->MakeReady();

  DriverEntry done_dentry =
      BuildDriverEntry(entry, DriverEntry::State::COMPLETE);
  done_dentry.done = true;
  done_dentry.current_file_path = completion_info.path;
  done_dentry.bytes_downloaded = completion_info.bytes_downloaded;
  done_dentry.hash256 = completion_info.hash256;
  base::Time now = base::Time::Now();
  done_dentry.completion_time = now;

  int64_t start_time = 0;
  EXPECT_CALL(*task_scheduler_,
              ScheduleTask(DownloadTaskType::CLEANUP_TASK, _, _, _, _, _))
      .WillOnce(SaveArg<4>(&start_time));
  driver_->NotifyDownloadSucceeded(done_dentry);
  Entry* updated_entry = model_->Get(entry.guid);
  DCHECK(updated_entry);
  EXPECT_EQ(Entry::State::COMPLETE, updated_entry->state);
  EXPECT_EQ(completion_info.bytes_downloaded, updated_entry->bytes_downloaded);
  EXPECT_EQ(completion_info.path, updated_entry->target_file_path);
  EXPECT_EQ(now, updated_entry->completion_time);
  EXPECT_LE(done_dentry.completion_time + config_->file_keep_alive_time,
            now + base::TimeDelta::FromSeconds(start_time));
  EXPECT_EQ(completion_info.hash256, done_dentry.hash256);
  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, CompletionInfoPropagated) {
  // Create initial Entry and DriverEntry objects.
  Entry succeeded_entry = test::BuildBasicEntry(Entry::State::ACTIVE);
  ASSERT_TRUE(succeeded_entry.response_headers);
  Entry succeeded_with_hash_entry = test::BuildBasicEntry(Entry::State::ACTIVE);
  ASSERT_TRUE(succeeded_with_hash_entry.response_headers);
  Entry failed_entry = test::BuildBasicEntry(Entry::State::ACTIVE);
  ASSERT_TRUE(failed_entry.response_headers);
  std::vector<Entry> entries = {succeeded_entry, succeeded_with_hash_entry,
                                failed_entry};

  DriverEntry succeeded_dentry =
      BuildDriverEntry(succeeded_entry, DriverEntry::State::IN_PROGRESS);
  DriverEntry succeeded_with_hash_dentry = BuildDriverEntry(
      succeeded_with_hash_entry, DriverEntry::State::IN_PROGRESS);
  DriverEntry failed_dentry =
      BuildDriverEntry(failed_entry, DriverEntry::State::IN_PROGRESS);

  driver_->AddTestData(std::vector<DriverEntry>{
      succeeded_dentry, succeeded_with_hash_dentry, failed_dentry});

  // Mock expectations.
  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  CompletionInfo succeeded_completion_info;
  EXPECT_CALL(*client_, OnDownloadSucceeded(succeeded_entry.guid, _))
      .WillOnce(SaveArg<1>(&succeeded_completion_info));

  CompletionInfo succeeded_with_hash_completion_info;
  EXPECT_CALL(*client_, OnDownloadSucceeded(succeeded_with_hash_entry.guid, _))
      .WillOnce(SaveArg<1>(&succeeded_with_hash_completion_info));

  CompletionInfo failed_completion_info;
  EXPECT_CALL(*client_, OnDownloadFailed(failed_entry.guid, _, _))
      .WillOnce(SaveArg<1>(&failed_completion_info));

  // Initialize and complete the downloads.
  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);
  driver_->MakeReady();

  DriverEntry succeeded_done_dentry =
      BuildDriverEntry(succeeded_entry, DriverEntry::State::COMPLETE);
  succeeded_done_dentry.done = true;
  succeeded_done_dentry.current_file_path =
      base::FilePath::FromUTF8Unsafe("abc");

  DriverEntry succeeded_with_hash_done_dentry =
      BuildDriverEntry(succeeded_with_hash_entry, DriverEntry::State::COMPLETE);
  succeeded_with_hash_done_dentry.done = true;
  succeeded_with_hash_done_dentry.current_file_path =
      base::FilePath::FromUTF8Unsafe("abc");
  succeeded_with_hash_done_dentry.hash256 = "01234567ABCDEF";

  DriverEntry failed_done_dentry =
      BuildDriverEntry(failed_entry, DriverEntry::State::COMPLETE);
  succeeded_done_dentry.current_file_path =
      base::FilePath::FromUTF8Unsafe("xyz");
  failed_done_dentry.done = true;

  driver_->NotifyDownloadSucceeded(succeeded_done_dentry);
  driver_->NotifyDownloadSucceeded(succeeded_with_hash_done_dentry);
  driver_->NotifyDownloadFailed(failed_done_dentry,
                                FailureType::NOT_RECOVERABLE);
  task_runner_->RunUntilIdle();

  // Check the CompletionInfo provided when the download completes.
  ASSERT_TRUE(succeeded_completion_info.response_headers);
  EXPECT_EQ(succeeded_completion_info.response_headers->raw_headers(),
            succeeded_entry.response_headers->raw_headers());
  EXPECT_EQ(succeeded_completion_info.path,
            succeeded_done_dentry.current_file_path);
  EXPECT_EQ(succeeded_completion_info.hash256, "");

  ASSERT_TRUE(succeeded_with_hash_completion_info.response_headers);
  EXPECT_EQ(succeeded_with_hash_completion_info.response_headers->raw_headers(),
            succeeded_with_hash_entry.response_headers->raw_headers());
  EXPECT_EQ(succeeded_with_hash_completion_info.path,
            succeeded_with_hash_done_dentry.current_file_path);
  EXPECT_EQ(succeeded_with_hash_completion_info.hash256,
            succeeded_with_hash_done_dentry.hash256);

  ASSERT_TRUE(failed_completion_info.response_headers);
  EXPECT_EQ(failed_completion_info.response_headers->raw_headers(),
            failed_entry.response_headers->raw_headers());
  EXPECT_EQ(failed_completion_info.path, base::FilePath());
}

TEST_F(DownloadServiceControllerImplTest, CleanupTaskScheduledAtEarliestTime) {
  // Setup download service test data.
  // File keep alive time is 10 minutes.
  // entry1 should be ignored.
  Entry entry1 = test::BuildBasicEntry(Entry::State::ACTIVE);
  entry1.completion_time = base::Time::Now() - base::TimeDelta::FromMinutes(7);
  entry1.last_cleanup_check_time = entry1.completion_time;
  Entry entry2 = test::BuildBasicEntry(Entry::State::COMPLETE);
  entry2.completion_time = base::Time::Now() - base::TimeDelta::FromMinutes(1);
  entry2.last_cleanup_check_time = entry2.completion_time;
  Entry entry3 = test::BuildBasicEntry(Entry::State::COMPLETE);
  entry3.completion_time = base::Time::Now() - base::TimeDelta::FromMinutes(2);
  entry3.last_cleanup_check_time = entry3.completion_time;

  // For entry4, keep_alive_until time should be considered instead.
  Entry entry4 = test::BuildBasicEntry(Entry::State::COMPLETE);
  entry4.completion_time = base::Time::Now() - base::TimeDelta::FromMinutes(5);
  entry4.last_cleanup_check_time =
      base::Time::Now() - base::TimeDelta::FromMinutes(1);
  std::vector<Entry> entries = {entry1, entry2, entry3, entry4};

  // Setup download driver test data.
  DriverEntry dentry1 =
      BuildDriverEntry(entry1, DriverEntry::State::IN_PROGRESS);
  driver_->AddTestData(std::vector<DriverEntry>{dentry1});

  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);
  driver_->MakeReady();

  DriverEntry done_dentry1 =
      BuildDriverEntry(entry1, DriverEntry::State::COMPLETE);
  done_dentry1.done = true;
  done_dentry1.bytes_downloaded = 1024;
  done_dentry1.completion_time = base::Time::Now();
  done_dentry1.current_file_path = base::FilePath::FromUTF8Unsafe("123");

  // Since keep_alive_time is 10 minutes and oldest completion time was 2
  // minutes ago, we should see the cleanup window start at 8 minutes.
  EXPECT_CALL(*task_scheduler_, ScheduleTask(DownloadTaskType::CLEANUP_TASK,
                                             false, false, 0, 480, 780))
      .Times(1);
  driver_->NotifyDownloadSucceeded(done_dentry1);
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entry1.guid)->state);

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, OnDownloadUpdated) {
  // Setup download service test data.
  Entry entry = test::BuildBasicEntry(Entry::State::ACTIVE);
  std::vector<Entry> entries = {entry};

  // Setup download driver test data.
  DriverEntry dentry = BuildDriverEntry(entry, DriverEntry::State::IN_PROGRESS);
  driver_->AddTestData(std::vector<DriverEntry>{dentry});

  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);

  DriverEntry dentry_update;
  dentry_update.state = DriverEntry::State::IN_PROGRESS;
  dentry_update.guid = entry.guid;
  dentry_update.bytes_downloaded = 1024;
  driver_->MakeReady();

  EXPECT_CALL(*client_, OnDownloadUpdated(entry.guid, /* bytes_uploaded= */ 0u,
                                          dentry_update.bytes_downloaded));
  driver_->NotifyDownloadUpdate(dentry_update);
  EXPECT_EQ(Entry::State::ACTIVE, model_->Get(entry.guid)->state);

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest, DownloadCompletionTest) {
  // TODO(dtrainor): Simulate a UNKNOWN once that is supported.
  // Setup download service test data.
  Entry entry1 = test::BuildBasicEntry(Entry::State::ACTIVE);
  Entry entry2 = test::BuildBasicEntry(Entry::State::ACTIVE);
  Entry entry3 = test::BuildBasicEntry(Entry::State::ACTIVE);
  entry3.scheduling_params.cancel_time = base::Time::Now();
  std::vector<Entry> entries = {entry1, entry2, entry3};

  DriverEntry dentry1 =
      BuildDriverEntry(entry1, DriverEntry::State::IN_PROGRESS);
  DriverEntry dentry2 =
      BuildDriverEntry(entry2, DriverEntry::State::IN_PROGRESS);
  driver_->AddTestData(std::vector<DriverEntry>{dentry1, dentry2});

  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  // Test FailureReason::TIMEDOUT.
  EXPECT_CALL(*client_,
              OnDownloadFailed(entry3.guid, _, Client::FailureReason::TIMEDOUT))
      .Times(1);

  // Set up the Controller.

  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);
  driver_->MakeReady();
  task_runner_->RunUntilIdle();

  // Test FailureReason::CANCELLED.
  EXPECT_CALL(*client_, OnDownloadFailed(entry1.guid, _,
                                         Client::FailureReason::CANCELLED))
      .Times(1);
  controller_->CancelDownload(entry1.guid);

  // Test FailureReason::NETWORK.
  EXPECT_CALL(*client_,
              OnDownloadFailed(entry2.guid, _, Client::FailureReason::NETWORK))
      .Times(1);
  driver_->NotifyDownloadFailed(dentry2, FailureType::NOT_RECOVERABLE);

  task_runner_->RunUntilIdle();
}

TEST_F(DownloadServiceControllerImplTest,
       UploadTestForSuccessPauseCancelFailureTimeout) {
  auto create_entry = [this](unsigned int delay) {
    Entry entry = test::BuildBasicEntry(Entry::State::ACTIVE);
    entry.client = DownloadClient::TEST_3;
    client3_->SetUploadResponseDelayForGuid(entry.guid, delay);
    return entry;
  };

  auto verify_entry =
      [this](const std::string& guid,
             base::Optional<Entry::State> expected_state,
             base::Optional<DriverEntry::State> expected_driver_state,
             bool has_upload_data) {
        auto* entry = model_->Get(guid);
        auto driver_entry = driver_->Find(guid);
        EXPECT_EQ(expected_state.has_value(), entry != nullptr);
        if (expected_state.has_value()) {
          EXPECT_EQ(expected_state.value(), entry->state);
          EXPECT_EQ(has_upload_data, entry->has_upload_data);
        }

        EXPECT_EQ(expected_driver_state.has_value(), driver_entry.has_value());
        if (expected_driver_state.has_value()) {
          EXPECT_EQ(expected_driver_state.value(), driver_entry.value().state);
        }
      };

  // entry1 - successful flow, entry2 - cancel before client response,
  // entry3 - client response timeout, entry4 - network failure.
  // entry5 - pause before client response.
  Entry entry1 = create_entry(15);
  Entry entry2 = create_entry(25);
  Entry entry3 = create_entry(50);
  Entry entry4 = create_entry(10);
  Entry entry5 = create_entry(25);
  config_->pending_upload_timeout_delay = base::TimeDelta::FromSeconds(30);
  config_->max_concurrent_downloads = 8u;
  config_->max_running_downloads = 8u;
  config_->max_retry_count = 4u;
  std::vector<Entry> entries = {entry1, entry2, entry3, entry4, entry5};

  EXPECT_CALL(*client3_, OnServiceInitialized(false, _)).Times(1);

  // Set up the Controller.
  device_status_listener_->SetDeviceStatus(
      DeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));

  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);
  driver_->MakeReady();
  task_runner_->RunUntilIdle();

  // No driver entry yet as entries are waiting for client response.
  verify_entry(entry1.guid, Entry::State::ACTIVE, base::nullopt, false);
  verify_entry(entry2.guid, Entry::State::ACTIVE, base::nullopt, false);
  verify_entry(entry3.guid, Entry::State::ACTIVE, base::nullopt, false);
  verify_entry(entry4.guid, Entry::State::ACTIVE, base::nullopt, false);
  verify_entry(entry5.guid, Entry::State::ACTIVE, base::nullopt, false);

  // At 20 seconds.
  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(20));

  // Test that entry1 is marked as upload and is in progress.
  verify_entry(entry1.guid, Entry::State::ACTIVE,
               DriverEntry::State::IN_PROGRESS, true);

  // Successfully complete the upload for entry1.
  EXPECT_CALL(*client3_, OnDownloadSucceeded(entry1.guid, _)).Times(1);
  auto dentry1 = driver_->Find(entry1.guid);
  dentry1.value().state = DriverEntry::State::COMPLETE;
  driver_->NotifyDownloadSucceeded(dentry1.value());
  task_runner_->RunUntilIdle();
  verify_entry(entry1.guid, Entry::State::COMPLETE,
               DriverEntry::State::COMPLETE, true);

  // Call PauseDownload before client response for entry5.
  controller_->PauseDownload(entry5.guid);
  task_runner_->RunUntilIdle();
  verify_entry(entry5.guid, Entry::State::PAUSED, base::nullopt, false);

  // Test CancelDownload before client response for entry2.
  EXPECT_CALL(*client3_, OnDownloadFailed(entry2.guid, _,
                                          Client::FailureReason::CANCELLED))
      .Times(1);
  controller_->CancelDownload(entry2.guid);
  task_runner_->RunUntilIdle();
  verify_entry(entry2.guid, base::nullopt, base::nullopt, false);

  // At 25 seconds.
  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(5));

  // Entry2, entry5 receive client response.
  verify_entry(entry2.guid, base::nullopt, base::nullopt, false);
  verify_entry(entry5.guid, Entry::State::PAUSED, base::nullopt, true);

  // Entry3 timeouts before client response.
  EXPECT_CALL(
      *client3_,
      OnDownloadFailed(entry3.guid, _, Client::FailureReason::UPLOAD_TIMEDOUT))
      .Times(1);

  // At 40 seconds.
  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(15));
  verify_entry(entry3.guid, base::nullopt, base::nullopt, false);

  // Test network failure for entry4. First check the entry is in progress.
  verify_entry(entry4.guid, Entry::State::ACTIVE,
               DriverEntry::State::IN_PROGRESS, true);
  EXPECT_CALL(*client3_,
              OnDownloadFailed(entry4.guid, _, Client::FailureReason::NETWORK))
      .Times(1);
  DriverEntry dentry4 =
      BuildDriverEntry(entry4, DriverEntry::State::INTERRUPTED);
  driver_->NotifyDownloadFailed(dentry4, FailureType::NOT_RECOVERABLE);
  task_runner_->RunUntilIdle();
  verify_entry(entry4.guid, base::nullopt, base::nullopt, false);

  // Entry5 is still paused, call ResumeDownload. It should make another fresh
  // request for data.
  verify_entry(entry5.guid, Entry::State::PAUSED, base::nullopt, true);
  controller_->ResumeDownload(entry5.guid);
  task_runner_->RunUntilIdle();
  verify_entry(entry5.guid, Entry::State::ACTIVE, base::nullopt, true);

  // At 65 seconds. Entry5 receives data for the second time and continues.
  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(25));
  verify_entry(entry5.guid, Entry::State::ACTIVE,
               DriverEntry::State::IN_PROGRESS, true);
}

TEST_F(DownloadServiceControllerImplTest, StartupRecovery) {
  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  std::vector<Entry> entries;
  std::vector<DriverEntry> driver_entries;
  entries.push_back(test::BuildBasicEntry(Entry::State::NEW));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::IN_PROGRESS));
  entries.push_back(test::BuildBasicEntry(Entry::State::NEW));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::COMPLETE));
  entries.push_back(test::BuildBasicEntry(Entry::State::NEW));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::CANCELLED));
  entries.push_back(test::BuildBasicEntry(Entry::State::NEW));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::INTERRUPTED));
  entries.push_back(test::BuildBasicEntry(Entry::State::NEW));

  entries.push_back(test::BuildBasicEntry(Entry::State::AVAILABLE));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::IN_PROGRESS));
  entries.push_back(test::BuildBasicEntry(Entry::State::AVAILABLE));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::COMPLETE));
  entries.push_back(test::BuildBasicEntry(Entry::State::AVAILABLE));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::CANCELLED));
  entries.push_back(test::BuildBasicEntry(Entry::State::AVAILABLE));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::INTERRUPTED));
  entries.push_back(test::BuildBasicEntry(Entry::State::AVAILABLE));

  entries.push_back(test::BuildBasicEntry(Entry::State::ACTIVE));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::IN_PROGRESS));
  entries.push_back(test::BuildBasicEntry(Entry::State::ACTIVE));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::COMPLETE));
  entries.push_back(test::BuildBasicEntry(Entry::State::ACTIVE));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::CANCELLED));
  entries.push_back(test::BuildBasicEntry(Entry::State::ACTIVE));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::INTERRUPTED));
  entries.push_back(test::BuildBasicEntry(Entry::State::ACTIVE));

  entries.push_back(test::BuildBasicEntry(Entry::State::PAUSED));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::IN_PROGRESS));
  entries.push_back(test::BuildBasicEntry(Entry::State::PAUSED));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::COMPLETE));
  entries.push_back(test::BuildBasicEntry(Entry::State::PAUSED));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::CANCELLED));
  entries.push_back(test::BuildBasicEntry(Entry::State::PAUSED));
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::INTERRUPTED));
  entries.push_back(test::BuildBasicEntry(Entry::State::PAUSED));

  entries.push_back(test::BuildBasicEntry(Entry::State::COMPLETE));
  entries.back().completion_time = base::Time::Now();
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::IN_PROGRESS));
  entries.push_back(test::BuildBasicEntry(Entry::State::COMPLETE));
  entries.back().completion_time = base::Time::Now();
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::COMPLETE));
  entries.push_back(test::BuildBasicEntry(Entry::State::COMPLETE));
  entries.back().completion_time = base::Time::Now();
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::CANCELLED));
  entries.push_back(test::BuildBasicEntry(Entry::State::COMPLETE));
  entries.back().completion_time = base::Time::Now();
  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::INTERRUPTED));
  entries.push_back(test::BuildBasicEntry(Entry::State::COMPLETE));
  entries.back().completion_time = base::Time::Now();

  // Set up the Controller.
  device_status_listener_->SetDeviceStatus(
      DeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));

  InitializeController();
  driver_->AddTestData(driver_entries);
  driver_->MakeReady();
  store_->AutomaticallyTriggerAllFutureCallbacks(true);
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);

  // Allow the initialization routines and persistent layers to do their thing.
  task_runner_->RunUntilIdle();

  // Validate Model and DownloadDriver states.
  // Note that we are accessing the Model instead of the Store here to make it
  // easier to query the states.
  // TODO(dtrainor): Check more of the DriverEntry state to validate that the
  // entries are either paused or resumed accordingly.

  // Entry::State::NEW.
  EXPECT_EQ(Entry::State::AVAILABLE, model_->Get(entries[0].guid)->state);
  EXPECT_EQ(Entry::State::AVAILABLE, model_->Get(entries[1].guid)->state);
  EXPECT_EQ(Entry::State::AVAILABLE, model_->Get(entries[2].guid)->state);
  EXPECT_EQ(Entry::State::AVAILABLE, model_->Get(entries[3].guid)->state);
  EXPECT_EQ(Entry::State::AVAILABLE, model_->Get(entries[4].guid)->state);
  EXPECT_EQ(base::nullopt, driver_->Find(entries[0].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[1].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[2].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[3].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[4].guid));

  // Entry::State::AVAILABLE.
  EXPECT_EQ(Entry::State::ACTIVE, model_->Get(entries[5].guid)->state);
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entries[6].guid)->state);
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entries[7].guid)->state);
  EXPECT_EQ(Entry::State::ACTIVE, model_->Get(entries[8].guid)->state);
  EXPECT_EQ(Entry::State::AVAILABLE, model_->Get(entries[9].guid)->state);
  EXPECT_NE(base::nullopt, driver_->Find(entries[5].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[6].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[7].guid));
  EXPECT_NE(base::nullopt, driver_->Find(entries[8].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[9].guid));

  // Entry::State::ACTIVE.
  EXPECT_EQ(Entry::State::ACTIVE, model_->Get(entries[10].guid)->state);
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entries[11].guid)->state);
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entries[12].guid)->state);
  EXPECT_EQ(Entry::State::ACTIVE, model_->Get(entries[13].guid)->state);
  EXPECT_EQ(Entry::State::ACTIVE, model_->Get(entries[14].guid)->state);
  EXPECT_NE(base::nullopt, driver_->Find(entries[10].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[11].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[12].guid));
  EXPECT_NE(base::nullopt, driver_->Find(entries[13].guid));
  EXPECT_NE(base::nullopt, driver_->Find(entries[14].guid));

  // Entry::State::PAUSED.
  EXPECT_EQ(Entry::State::PAUSED, model_->Get(entries[15].guid)->state);
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entries[16].guid)->state);
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entries[17].guid)->state);
  EXPECT_EQ(Entry::State::PAUSED, model_->Get(entries[18].guid)->state);
  EXPECT_EQ(Entry::State::PAUSED, model_->Get(entries[19].guid)->state);
  EXPECT_NE(base::nullopt, driver_->Find(entries[15].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[16].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[17].guid));
  EXPECT_NE(base::nullopt, driver_->Find(entries[18].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[19].guid));

  // prog, comp, canc, int, __
  // Entry::State::COMPLETE.
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entries[20].guid)->state);
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entries[21].guid)->state);
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entries[22].guid)->state);
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entries[23].guid)->state);
  EXPECT_EQ(Entry::State::COMPLETE, model_->Get(entries[24].guid)->state);
  EXPECT_EQ(base::nullopt, driver_->Find(entries[20].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[21].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[22].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[23].guid));
  EXPECT_EQ(base::nullopt, driver_->Find(entries[24].guid));
}

TEST_F(DownloadServiceControllerImplTest, StartupRecoveryForUploadEntries) {
  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  std::vector<Entry> entries;
  std::vector<DriverEntry> driver_entries;

  auto add_entry = [&entries, &driver_entries](
                       Entry::State state,
                       base::Optional<DriverEntry::State> driver_state) {
    Entry entry = test::BuildBasicEntry(state);
    entry.has_upload_data = true;
    if (state == Entry::State::COMPLETE)
      entry.completion_time = base::Time::Now();

    entries.push_back(entry);
    if (driver_state.has_value())
      driver_entries.push_back(BuildDriverEntry(entry, driver_state.value()));
  };

  add_entry(Entry::State::ACTIVE, DriverEntry::State::IN_PROGRESS);
  add_entry(Entry::State::ACTIVE, DriverEntry::State::COMPLETE);
  add_entry(Entry::State::ACTIVE, DriverEntry::State::CANCELLED);
  add_entry(Entry::State::ACTIVE, DriverEntry::State::INTERRUPTED);
  add_entry(Entry::State::ACTIVE, base::nullopt);

  add_entry(Entry::State::PAUSED, DriverEntry::State::IN_PROGRESS);
  add_entry(Entry::State::PAUSED, DriverEntry::State::COMPLETE);
  add_entry(Entry::State::PAUSED, DriverEntry::State::CANCELLED);
  add_entry(Entry::State::PAUSED, DriverEntry::State::INTERRUPTED);
  add_entry(Entry::State::PAUSED, base::nullopt);

  add_entry(Entry::State::COMPLETE, DriverEntry::State::IN_PROGRESS);
  add_entry(Entry::State::COMPLETE, DriverEntry::State::COMPLETE);
  add_entry(Entry::State::COMPLETE, DriverEntry::State::CANCELLED);
  add_entry(Entry::State::COMPLETE, DriverEntry::State::INTERRUPTED);
  add_entry(Entry::State::COMPLETE, base::nullopt);

  // Set up the Controller.
  device_status_listener_->SetDeviceStatus(
      DeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));

  InitializeController();
  driver_->AddTestData(driver_entries);
  driver_->MakeReady();
  store_->AutomaticallyTriggerAllFutureCallbacks(true);
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);

  // Allow the initialization routines and persistent layers to do their thing.
  task_runner_->RunUntilIdle();

  auto verify_entry = [this](const std::string& guid, Entry::State state,
                             base::Optional<DriverEntry::State> driver_state) {
    EXPECT_EQ(state, model_->Get(guid)->state);
    auto driver_entry = driver_->Find(guid);
    EXPECT_EQ(driver_state.has_value(), driver_entry.has_value());
    if (driver_entry.has_value())
      EXPECT_EQ(driver_state, driver_entry->state);
  };

  // Validate Model and DownloadDriver states. Any IN_PROGRESS or INTERRUPTED
  // download should be moved to complete state for ACTIVE/PAUSED entries.

  // Entry::State::ACTIVE.
  verify_entry(entries[0].guid, Entry::State::COMPLETE, base::nullopt);
  verify_entry(entries[1].guid, Entry::State::COMPLETE, base::nullopt);
  verify_entry(entries[2].guid, Entry::State::COMPLETE, base::nullopt);
  verify_entry(entries[3].guid, Entry::State::COMPLETE, base::nullopt);
  verify_entry(entries[4].guid, Entry::State::ACTIVE,
               DriverEntry::State::IN_PROGRESS);

  // Entry::State::PAUSED.
  verify_entry(entries[5].guid, Entry::State::COMPLETE, base::nullopt);
  verify_entry(entries[6].guid, Entry::State::COMPLETE, base::nullopt);
  verify_entry(entries[7].guid, Entry::State::COMPLETE, base::nullopt);
  verify_entry(entries[8].guid, Entry::State::COMPLETE, base::nullopt);
  verify_entry(entries[9].guid, Entry::State::PAUSED, base::nullopt);

  // Entry::State::COMPLETE.
  verify_entry(entries[10].guid, Entry::State::COMPLETE, base::nullopt);
  verify_entry(entries[11].guid, Entry::State::COMPLETE, base::nullopt);
  verify_entry(entries[12].guid, Entry::State::COMPLETE, base::nullopt);
  verify_entry(entries[13].guid, Entry::State::COMPLETE, base::nullopt);
  verify_entry(entries[14].guid, Entry::State::COMPLETE, base::nullopt);
}

// Download driver will remove the download if failed to persist the response
// headers.
TEST_F(DownloadServiceControllerImplTest, StartupRecoveryNoResponseHeaders) {
  std::vector<Entry> entries;
  std::vector<DriverEntry> driver_entries;
  entries.push_back(test::BuildBasicEntry(Entry::State::ACTIVE));
  entries.back().response_headers = nullptr;
  entries.back().did_received_response = false;

  driver_entries.push_back(
      BuildDriverEntry(entries.back(), DriverEntry::State::IN_PROGRESS));

  device_status_listener_->SetDeviceStatus(
      DeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));

  InitializeController();
  driver_->AddTestData(driver_entries);
  driver_->MakeReady();
  store_->AutomaticallyTriggerAllFutureCallbacks(true);
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);

  // Verify that the driver entry will be removed without response headers.
  EXPECT_FALSE(driver_->Find(entries[0].guid).has_value());

  task_runner_->RunUntilIdle();

  // The download is retried.
  EXPECT_EQ(Entry::State::ACTIVE, model_->Get(entries[0].guid)->state);
  EXPECT_TRUE(model_->Get(entries[0].guid)->did_received_response);
  auto expected_driver_entry = driver_->Find(entries[0].guid);
  EXPECT_TRUE(expected_driver_entry.has_value());
  EXPECT_EQ(model_->Get(entries[0].guid)->url_chain,
            expected_driver_entry->url_chain);
  EXPECT_EQ(model_->Get(entries[0].guid)->response_headers->raw_headers(),
            expected_driver_entry->response_headers->raw_headers());
}

TEST_F(DownloadServiceControllerImplTest, ExistingExternalDownload) {
  Entry entry1 = test::BuildBasicEntry(Entry::State::ACTIVE);
  Entry entry2 = test::BuildBasicEntry(Entry::State::ACTIVE);
  Entry entry3 = test::BuildBasicEntry(Entry::State::ACTIVE);
  entry3.scheduling_params.priority = SchedulingParams::Priority::UI;

  // Simulate an existing download the service knows about and one it does not.
  DriverEntry dentry1 =
      BuildDriverEntry(entry2, DriverEntry::State::IN_PROGRESS);
  DriverEntry dentry2;
  dentry2.guid = base::GenerateGUID();
  dentry2.state = DriverEntry::State::IN_PROGRESS;

  std::vector<Entry> entries = {entry1, entry2, entry3};
  std::vector<DriverEntry> dentries = {dentry1, dentry2};

  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  // Set up the Controller.
  device_status_listener_->SetDeviceStatus(
      DeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));

  driver_->AddTestData(dentries);
  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);
  driver_->MakeReady();
  task_runner_->RunUntilIdle();

  EXPECT_EQ(Entry::State::ACTIVE, model_->Get(entry1.guid)->state);
  EXPECT_EQ(Entry::State::ACTIVE, model_->Get(entry2.guid)->state);
  EXPECT_EQ(Entry::State::ACTIVE, model_->Get(entry3.guid)->state);

  EXPECT_FALSE(driver_->Find(entry1.guid).has_value());

  EXPECT_TRUE(driver_->Find(entry2.guid).has_value());
  EXPECT_TRUE(driver_->Find(entry2.guid).value().paused);

  EXPECT_TRUE(driver_->Find(entry3.guid).has_value());
  EXPECT_FALSE(driver_->Find(entry3.guid).value().paused);

  // Simulate a successful external download.
  driver_->NotifyDownloadSucceeded(dentry2);
  task_runner_->RunUntilIdle();

  EXPECT_TRUE(driver_->Find(entry1.guid).has_value());
  EXPECT_FALSE(driver_->Find(entry1.guid).value().paused);
  EXPECT_FALSE(driver_->Find(entry2.guid).value().paused);
  EXPECT_FALSE(driver_->Find(entry3.guid).value().paused);
}

TEST_F(DownloadServiceControllerImplTest, NewExternalDownload) {
  Entry entry1 = test::BuildBasicEntry(Entry::State::ACTIVE);
  Entry entry2 = test::BuildBasicEntry(Entry::State::ACTIVE);
  entry2.scheduling_params.priority = SchedulingParams::Priority::UI;

  DriverEntry dentry1 =
      BuildDriverEntry(entry2, DriverEntry::State::IN_PROGRESS);

  std::vector<Entry> entries = {entry1, entry2};
  std::vector<DriverEntry> dentries = {dentry1};

  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  // Set up the Controller.
  device_status_listener_->SetDeviceStatus(
      DeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));

  driver_->AddTestData(dentries);
  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);
  driver_->MakeReady();
  task_runner_->RunUntilIdle();

  EXPECT_EQ(Entry::State::ACTIVE, model_->Get(entry1.guid)->state);
  EXPECT_EQ(Entry::State::ACTIVE, model_->Get(entry2.guid)->state);

  EXPECT_TRUE(driver_->Find(entry1.guid).has_value());
  EXPECT_FALSE(driver_->Find(entry1.guid).value().paused);
  EXPECT_TRUE(driver_->Find(entry2.guid).has_value());
  EXPECT_FALSE(driver_->Find(entry2.guid).value().paused);

  DriverEntry dentry2;
  dentry2.guid = base::GenerateGUID();
  dentry2.state = DriverEntry::State::IN_PROGRESS;

  // Simulate a newly created external download.
  driver_->Start(RequestParams(), dentry2.guid, dentry2.current_file_path,
                 nullptr, TRAFFIC_ANNOTATION_FOR_TESTS);

  EXPECT_TRUE(driver_->Find(entry1.guid).value().paused);
  EXPECT_FALSE(driver_->Find(entry2.guid).value().paused);

  // Simulate a paused external download.
  dentry2.paused = true;
  driver_->NotifyDownloadUpdate(dentry2);

  EXPECT_FALSE(driver_->Find(entry1.guid).value().paused);
  EXPECT_FALSE(driver_->Find(entry2.guid).value().paused);

  // Simulate a resumed external download.
  dentry2.paused = false;
  driver_->NotifyDownloadUpdate(dentry2);

  EXPECT_TRUE(driver_->Find(entry1.guid).value().paused);
  EXPECT_FALSE(driver_->Find(entry2.guid).value().paused);

  // Simulate a failed external download.
  dentry2.state = DriverEntry::State::INTERRUPTED;
  driver_->NotifyDownloadFailed(dentry2, FailureType::RECOVERABLE);

  EXPECT_FALSE(driver_->Find(entry1.guid).value().paused);
  EXPECT_FALSE(driver_->Find(entry2.guid).value().paused);

  // Rebuild the download so we can simulate more.
  dentry2.state = DriverEntry::State::IN_PROGRESS;
  driver_->Start(RequestParams(), dentry2.guid, dentry2.current_file_path,
                 nullptr, TRAFFIC_ANNOTATION_FOR_TESTS);

  EXPECT_TRUE(driver_->Find(entry1.guid).value().paused);
  EXPECT_FALSE(driver_->Find(entry2.guid).value().paused);

  // Simulate a successful external download.
  dentry2.state = DriverEntry::State::COMPLETE;
  driver_->NotifyDownloadSucceeded(dentry2);

  EXPECT_FALSE(driver_->Find(entry1.guid).value().paused);
  EXPECT_FALSE(driver_->Find(entry2.guid).value().paused);
}

TEST_F(DownloadServiceControllerImplTest, CancelTimeTest) {
  Entry entry1 = test::BuildBasicEntry();
  entry1.state = Entry::State::ACTIVE;
  entry1.create_time = base::Time::Now() - base::TimeDelta::FromSeconds(10);
  entry1.scheduling_params.cancel_time =
      base::Time::Now() - base::TimeDelta::FromSeconds(5);

  Entry entry2 = test::BuildBasicEntry();
  entry2.state = Entry::State::COMPLETE;
  entry2.create_time = base::Time::Now() - base::TimeDelta::FromSeconds(10);
  entry2.scheduling_params.cancel_time =
      base::Time::Now() - base::TimeDelta::FromSeconds(2);
  entry2.completion_time = base::Time::Now();
  std::vector<Entry> entries = {entry1, entry2};

  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);
  driver_->MakeReady();
  task_runner_->RunUntilIdle();

  // At startup, timed out entries should be killed.
  std::vector<Entry*> updated_entries = model_->PeekEntries();
  EXPECT_EQ(1u, updated_entries.size());
}

TEST_F(DownloadServiceControllerImplTest, RemoveCleanupEligibleDownloads) {
  config_->file_keep_alive_time = base::TimeDelta::FromMinutes(5);
  config_->max_file_keep_alive_time = base::TimeDelta::FromMinutes(50);

  Entry entry1 = test::BuildBasicEntry(Entry::State::ACTIVE);
  entry1.client = DownloadClient::TEST_2;

  Entry entry2 = test::BuildBasicEntry(Entry::State::COMPLETE);
  entry2.completion_time = base::Time::Now() - base::TimeDelta::FromMinutes(2);
  entry2.last_cleanup_check_time = entry2.completion_time;
  entry2.client = DownloadClient::TEST_2;

  Entry entry3 = test::BuildBasicEntry(Entry::State::COMPLETE);
  entry3.completion_time = base::Time::Now() - base::TimeDelta::FromMinutes(20);
  entry3.last_cleanup_check_time = entry3.completion_time;
  entry3.client = DownloadClient::TEST_2;

  // last_cleanup_check_time was recent and enough time hasn't passed.
  Entry entry4 = test::BuildBasicEntry(Entry::State::COMPLETE);
  entry4.completion_time = base::Time::Now() - base::TimeDelta::FromMinutes(20);
  entry4.last_cleanup_check_time =
      base::Time::Now() - base::TimeDelta::FromMinutes(2);
  entry4.client = DownloadClient::TEST_2;

  // Client doesn't want to delete.
  Entry entry5 = test::BuildBasicEntry(Entry::State::COMPLETE);
  entry5.completion_time = base::Time::Now() - base::TimeDelta::FromMinutes(45);
  entry5.last_cleanup_check_time =
      base::Time::Now() - base::TimeDelta::FromMinutes(20);
  entry5.client = DownloadClient::TEST;

  // Client doesn't want to delete, but entry has gotten too many life times.
  Entry entry6 = test::BuildBasicEntry(Entry::State::COMPLETE);
  entry6.completion_time = base::Time::Now() - base::TimeDelta::FromMinutes(80);
  entry6.last_cleanup_check_time =
      base::Time::Now() - base::TimeDelta::FromMinutes(20);
  entry6.client = DownloadClient::TEST;

  std::vector<Entry> entries = {entry1, entry2, entry3, entry4, entry5, entry6};

  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);
  driver_->MakeReady();
  task_runner_->RunUntilIdle();

  std::vector<Entry*> expected = {&entry1, &entry2, &entry4, &entry5};
  EXPECT_EQ(expected.size(), model_->PeekEntries().size());
  EXPECT_TRUE(
      test::CompareEntryListUsingGuidOnly(expected, model_->PeekEntries()));
}

// Ensures no more downloads are activated if the number of downloads exceeds
// the max running download configuration.
TEST_F(DownloadServiceControllerImplTest, ThrottlingConfigMaxRunning) {
  Entry entry1 = test::BuildBasicEntry(Entry::State::AVAILABLE);
  Entry entry2 = test::BuildBasicEntry(Entry::State::ACTIVE);
  std::vector<Entry> entries = {entry1, entry2};

  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  // Setup the Configuration.
  config_->max_concurrent_downloads = 1u;
  config_->max_running_downloads = 1u;

  // Setup the controller.
  device_status_listener_->SetDeviceStatus(
      DeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));
  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);
  store_->AutomaticallyTriggerAllFutureCallbacks(true);

  // Hit the max running configuration threshold, nothing should be called.
  EXPECT_CALL(*scheduler_, Next(_, _)).Times(0);
  EXPECT_CALL(*scheduler_, Reschedule(_)).Times(1);
  driver_->MakeReady();
  task_runner_->RunUntilIdle();

  EXPECT_EQ(Entry::State::AVAILABLE, model_->Get(entry1.guid)->state);
}

// Ensures max concurrent download configuration considers both active and
// paused downloads.
TEST_F(DownloadServiceControllerImplTest, ThrottlingConfigMaxConcurrent) {
  Entry entry1 = test::BuildBasicEntry(Entry::State::AVAILABLE);
  Entry entry2 = test::BuildBasicEntry(Entry::State::AVAILABLE);
  Entry entry3 = test::BuildBasicEntry(Entry::State::PAUSED);
  std::vector<Entry> entries = {entry1, entry2, entry3};

  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  // Setup the Configuration.
  config_->max_concurrent_downloads = 2u;
  config_->max_running_downloads = 1u;

  // Setup the controller.
  device_status_listener_->SetDeviceStatus(
      DeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));
  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);
  store_->AutomaticallyTriggerAllFutureCallbacks(true);

  // Can have one more download due to max concurrent configuration.
  testing::InSequence seq;
  EXPECT_EQ(Entry::State::AVAILABLE, model_->Get(entry1.guid)->state);
  EXPECT_CALL(*scheduler_, Next(_, _))
      .Times(1)
      .WillOnce(Return(model_->Get(entry1.guid)))
      .RetiresOnSaturation();
  // |scheduler_| will poll entry2 on next time, but it should not change the
  // state of entry2 due to max running download configuration.
  ON_CALL(*scheduler_, Next(_, _))
      .WillByDefault(Return(model_->Get(entry2.guid)));

  EXPECT_CALL(*scheduler_, Reschedule(_)).Times(1);
  driver_->MakeReady();
  task_runner_->RunUntilIdle();

  EXPECT_EQ(Entry::State::ACTIVE, model_->Get(entry1.guid)->state);
  EXPECT_EQ(Entry::State::AVAILABLE, model_->Get(entry2.guid)->state);
  EXPECT_EQ(Entry::State::PAUSED, model_->Get(entry3.guid)->state);
}

TEST_F(DownloadServiceControllerImplTest, DownloadTaskQueuesAfterFinish) {
  Entry entry1 = test::BuildBasicEntry(Entry::State::ACTIVE);
  Entry entry2 = test::BuildBasicEntry(Entry::State::AVAILABLE);
  Entry entry3 = test::BuildBasicEntry(Entry::State::AVAILABLE);
  entry3.scheduling_params.network_requirements =
      SchedulingParams::NetworkRequirements::UNMETERED;
  std::vector<Entry> entries = {entry1, entry2, entry3};

  DriverEntry dentry1 =
      BuildDriverEntry(entry1, DriverEntry::State::IN_PROGRESS);
  std::vector<DriverEntry> dentries = {dentry1};

  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  // Setup the Configuration.
  config_->max_concurrent_downloads = 1u;
  config_->max_running_downloads = 1u;

  // Setup the controller.
  device_status_listener_->SetDeviceStatus(
      DeviceStatus(BatteryStatus::CHARGING, NetworkStatus::METERED));
  driver_->AddTestData(dentries);
  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);
  store_->AutomaticallyTriggerAllFutureCallbacks(true);

  ON_CALL(*scheduler_, Next(_, _)).WillByDefault(Return(nullptr));

  // This will happen as the controller initializes and realizes it has more
  // work to do but isn't running a job.
  EXPECT_CALL(*scheduler_, Reschedule(_)).Times(1);

  // When the first download is done, it will attempt to clean up and schedule a
  // task.
  EXPECT_CALL(*task_scheduler_,
              ScheduleTask(DownloadTaskType::CLEANUP_TASK, _, _, _, _, _))
      .Times(2);

  driver_->MakeReady();
  task_runner_->RunUntilIdle();

  // Simulate a task start, which should limit our calls to Reschedule() because
  // we are in a task.
  controller_->OnStartScheduledTask(DownloadTaskType::DOWNLOAD_TASK,
                                    base::BindOnce(&NotifyTaskFinished));

  // Set up new expectations to start a new download.
  ON_CALL(*scheduler_, Next(_, _))
      .WillByDefault(Return(model_->Get(entry2.guid)));

  {
    // We should not reschedule if we still are doing work inside the job.
    EXPECT_CALL(*scheduler_, Reschedule(_)).Times(0);

    // Simulate a download success event, which will trigger the controller to
    // start a new download.
    base::Optional<DriverEntry> dentry1 = driver_->Find(entry1.guid);
    EXPECT_TRUE(dentry1.has_value());
    driver_->NotifyDownloadSucceeded(dentry1.value());
    task_runner_->RunUntilIdle();
  }

  ON_CALL(*scheduler_, Next(_, _)).WillByDefault(Return(nullptr));

  {
    EXPECT_CALL(*scheduler_, Reschedule(_)).Times(1);

    // Simulate a download success event, which will trigger the controller to
    // end it's task and schedule the task once (because the task is currently
    // running).
    base::Optional<DriverEntry> dentry2 = driver_->Find(entry2.guid);
    EXPECT_TRUE(dentry2.has_value());
    driver_->NotifyDownloadSucceeded(dentry2.value());
    task_runner_->RunUntilIdle();
  }
}

TEST_F(DownloadServiceControllerImplTest, CleanupTaskQueuesAfterFinish) {
  Entry entry1 = test::BuildBasicEntry(Entry::State::ACTIVE);
  Entry entry2 = test::BuildBasicEntry(Entry::State::ACTIVE);
  std::vector<Entry> entries = {entry1, entry2};

  DriverEntry dentry1 =
      BuildDriverEntry(entry1, DriverEntry::State::IN_PROGRESS);
  DriverEntry dentry2 =
      BuildDriverEntry(entry2, DriverEntry::State::IN_PROGRESS);
  std::vector<DriverEntry> dentries = {dentry1, dentry2};

  EXPECT_CALL(*client_, OnServiceInitialized(false, _)).Times(1);

  // Setup the Configuration.
  config_->max_concurrent_downloads = 2u;
  config_->max_running_downloads = 2u;

  // Setup the controller.
  device_status_listener_->SetDeviceStatus(
      DeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));
  driver_->AddTestData(dentries);
  InitializeController();
  store_->TriggerInit(true, std::make_unique<std::vector<Entry>>(entries));
  file_monitor_->TriggerInit(true);
  store_->AutomaticallyTriggerAllFutureCallbacks(true);
  driver_->MakeReady();

  // No cleanup tasks expected until we stop the job.
  EXPECT_CALL(*task_scheduler_,
              ScheduleTask(DownloadTaskType::CLEANUP_TASK, _, _, _, _, _))
      .Times(0);
  controller_->OnStartScheduledTask(DownloadTaskType::CLEANUP_TASK,
                                    base::BindOnce(&NotifyTaskFinished));

  // Trigger download succeed events, which should not schedule a cleanup until
  // the existing cleanup has finished.
  task_runner_->RunUntilIdle();
  driver_->NotifyDownloadSucceeded(driver_->Find(entry1.guid).value());
  task_runner_->RunUntilIdle();
  driver_->NotifyDownloadSucceeded(driver_->Find(entry2.guid).value());
  task_runner_->RunUntilIdle();

  // Now finish the job.  We expect a cleanup task to be scheduled.
  EXPECT_CALL(*task_scheduler_,
              ScheduleTask(DownloadTaskType::CLEANUP_TASK, _, _, _, _, _))
      .Times(1);
  controller_->OnStopScheduledTask(DownloadTaskType::CLEANUP_TASK);
}

}  // namespace download
