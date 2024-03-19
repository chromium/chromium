// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/android/auto_resumption_handler.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/download/network/network_status_listener_impl.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/download/public/task/mock_task_manager.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using network::mojom::ConnectionType;
using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::ReturnRefOfCopy;

namespace download {
namespace {

const char kNow[] = "1 Sep 2020 01:00:00 GMT";
const download::DownloadTaskType kUnmeteredDownloadsTaskType =
    download::DownloadTaskType::DOWNLOAD_AUTO_RESUMPTION_UNMETERED_TASK;
const download::DownloadTaskType kAnyNetworkDownloadsTaskType =
    download::DownloadTaskType::DOWNLOAD_AUTO_RESUMPTION_ANY_NETWORK_TASK;

base::Time GetNow() {
  base::Time now;
  bool success = base::Time::FromString(kNow, &now);
  EXPECT_TRUE(success);
  return now;
}

class AutoResumptionHandlerTest : public testing::Test {
 public:
  AutoResumptionHandlerTest()
      : task_runner_(new base::TestMockTimeTaskRunner),
        current_default_handle_(task_runner_) {}

  AutoResumptionHandlerTest(const AutoResumptionHandlerTest&) = delete;
  AutoResumptionHandlerTest& operator=(const AutoResumptionHandlerTest&) =
      delete;

  ~AutoResumptionHandlerTest() override = default;

 protected:
  void SetUp() override {
    auto network_listener = std::make_unique<NetworkStatusListenerImpl>(
        network::TestNetworkConnectionTracker::GetInstance());
    auto task_manager = std::make_unique<download::test::MockTaskManager>();
    task_manager_ = task_manager.get();
    auto config = std::make_unique<AutoResumptionHandler::Config>();
    config->auto_resumption_size_limit = 100;
    config->is_auto_resumption_enabled_in_native = true;
    clock_.SetNow(GetNow());

    auto_resumption_handler_ = std::make_unique<AutoResumptionHandler>(
        std::move(network_listener), std::move(task_manager), std::move(config),
        &clock_);

    std::vector<raw_ptr<DownloadItem, VectorExperimental>> empty_list;
    auto_resumption_handler_->SetResumableDownloads(empty_list);
    task_runner_->FastForwardUntilNoTasksRemain();
  }

  void TearDown() override {}

  void SetDownloadState(MockDownloadItem* download,
                        DownloadItem::DownloadState state,
                        bool paused,
                        bool allow_metered,
                        bool has_target_file_path = true) {
    ON_CALL(*download, GetGuid())
        .WillByDefault(ReturnRefOfCopy(
            base::Uuid::GenerateRandomV4().AsLowercaseString()));
    ON_CALL(*download, GetURL())
        .WillByDefault(ReturnRefOfCopy(GURL("http://example.com/foo")));
    ON_CALL(*download, GetState()).WillByDefault(Return(state));
    ON_CALL(*download, IsPaused()).WillByDefault(Return(paused));
    ON_CALL(*download, AllowMetered()).WillByDefault(Return(allow_metered));
    ON_CALL(*download, GetTargetFilePath())
        .WillByDefault(ReturnRefOfCopy(
            has_target_file_path ? base::FilePath(FILE_PATH_LITERAL("a.txt"))
                                 : base::FilePath()));
    auto last_reason =
        state == DownloadItem::INTERRUPTED
            ? download::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED
            : download::DOWNLOAD_INTERRUPT_REASON_NONE;
    ON_CALL(*download, GetLastReason()).WillByDefault(Return(last_reason));

    // Make sure the item won't be expired and ignored.
    ON_CALL(*download, GetStartTime())
        .WillByDefault(Return(GetNow() - base::Days(1)));
  }

  void SetNetworkConnectionType(ConnectionType connection_type) {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        connection_type);
  }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle current_default_handle_;
  raw_ptr<download::test::MockTaskManager, DanglingUntriaged> task_manager_;
  std::unique_ptr<AutoResumptionHandler> auto_resumption_handler_;
  base::SimpleTestClock clock_;
};

TEST_F(AutoResumptionHandlerTest, ScheduleTaskCalledOnDownloadStart) {
  auto item = std::make_unique<NiceMock<MockDownloadItem>>();

  EXPECT_CALL(*task_manager_, ScheduleTask(_, _)).Times(1);
  SetDownloadState(item.get(), DownloadItem::IN_PROGRESS, false, false);
  auto_resumption_handler_->OnDownloadStarted(item.get());
  task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(AutoResumptionHandlerTest, TaskFinishedCalledOnDownloadCompletion) {
  auto item = std::make_unique<NiceMock<MockDownloadItem>>();

  SetNetworkConnectionType(ConnectionType::CONNECTION_WIFI);
  task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_CALL(*task_manager_, ScheduleTask(_, _)).Times(1);
  SetDownloadState(item.get(), DownloadItem::IN_PROGRESS, false, false);
  auto_resumption_handler_->OnDownloadStarted(item.get());
  task_runner_->FastForwardUntilNoTasksRemain();

  // Complete the download.
  EXPECT_CALL(*task_manager_,
              NotifyTaskFinished(kAnyNetworkDownloadsTaskType, _))
      .Times(1);
  EXPECT_CALL(*task_manager_,
              NotifyTaskFinished(kUnmeteredDownloadsTaskType, _))
      .Times(1);
  EXPECT_CALL(*task_manager_, UnscheduleTask(kAnyNetworkDownloadsTaskType))
      .Times(1);
  EXPECT_CALL(*task_manager_, UnscheduleTask(kUnmeteredDownloadsTaskType))
      .Times(1);
  SetDownloadState(item.get(), DownloadItem::COMPLETE, false, false);
  auto_resumption_handler_->OnDownloadUpdated(item.get());
  task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(AutoResumptionHandlerTest, TaskFinishedCalledOnDownloadRemoved) {
  auto item = std::make_unique<NiceMock<MockDownloadItem>>();

  SetNetworkConnectionType(ConnectionType::CONNECTION_WIFI);
  task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_CALL(*task_manager_, ScheduleTask(_, _)).Times(1);
  SetDownloadState(item.get(), DownloadItem::IN_PROGRESS, false, false);
  auto_resumption_handler_->OnDownloadStarted(item.get());
  task_runner_->FastForwardUntilNoTasksRemain();

  // Remove the download.

  EXPECT_CALL(*task_manager_,
              NotifyTaskFinished(kAnyNetworkDownloadsTaskType, _))
      .Times(1);
  EXPECT_CALL(*task_manager_,
              NotifyTaskFinished(kUnmeteredDownloadsTaskType, _))
      .Times(1);
  SetDownloadState(item.get(), DownloadItem::COMPLETE, false, false);
  auto_resumption_handler_->OnDownloadRemoved(item.get());
  task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(AutoResumptionHandlerTest, MultipleDownloads) {
  // Start two downloads.
  auto item1 = std::make_unique<NiceMock<MockDownloadItem>>();
  auto item2 = std::make_unique<NiceMock<MockDownloadItem>>();
  SetDownloadState(item1.get(), DownloadItem::INTERRUPTED, false, false);
  SetDownloadState(item2.get(), DownloadItem::INTERRUPTED, false, false);

  SetNetworkConnectionType(ConnectionType::CONNECTION_WIFI);
  task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_CALL(*task_manager_, ScheduleTask(_, _)).Times(1);
  auto_resumption_handler_->OnDownloadStarted(item1.get());
  auto_resumption_handler_->OnDownloadStarted(item2.get());
  task_runner_->FastForwardUntilNoTasksRemain();

  // Finish item1. The task should still be running.
  EXPECT_CALL(*task_manager_, UnscheduleTask(kUnmeteredDownloadsTaskType))
      .Times(0);
  EXPECT_CALL(*task_manager_, UnscheduleTask(kAnyNetworkDownloadsTaskType))
      .Times(1);
  EXPECT_CALL(*task_manager_,
              NotifyTaskFinished(kUnmeteredDownloadsTaskType, _))
      .Times(0);
  EXPECT_CALL(*task_manager_,
              NotifyTaskFinished(kAnyNetworkDownloadsTaskType, _))
      .Times(1);
  SetDownloadState(item1.get(), DownloadItem::CANCELLED, false, false);
  auto_resumption_handler_->OnDownloadUpdated(item1.get());
  task_runner_->FastForwardUntilNoTasksRemain();

  // Finish item2. The task should now complete.
  EXPECT_CALL(*task_manager_, UnscheduleTask(kUnmeteredDownloadsTaskType))
      .Times(1);
  EXPECT_CALL(*task_manager_, UnscheduleTask(kAnyNetworkDownloadsTaskType))
      .Times(1);
  EXPECT_CALL(*task_manager_,
              NotifyTaskFinished(kUnmeteredDownloadsTaskType, _))
      .Times(1);
  EXPECT_CALL(*task_manager_,
              NotifyTaskFinished(kAnyNetworkDownloadsTaskType, _))
      .Times(1);

  SetDownloadState(item2.get(), DownloadItem::COMPLETE, false, false);
  auto_resumption_handler_->OnDownloadUpdated(item2.get());
  task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(AutoResumptionHandlerTest, DownloadResumesCorrectlyOnNetworkChange) {
  // Create two downloads: item1 (unmetered), item2 (metered).
  auto item1 = std::make_unique<NiceMock<MockDownloadItem>>();
  auto item2 = std::make_unique<NiceMock<MockDownloadItem>>();
  SetDownloadState(item1.get(), DownloadItem::INTERRUPTED, false, false);
  SetDownloadState(item2.get(), DownloadItem::INTERRUPTED, false, true);

  auto_resumption_handler_->OnDownloadStarted(item1.get());
  auto_resumption_handler_->OnDownloadStarted(item2.get());
  task_runner_->FastForwardUntilNoTasksRemain();

  // Start with disconnected network.
  SetNetworkConnectionType(ConnectionType::CONNECTION_NONE);
  task_runner_->FastForwardUntilNoTasksRemain();

  // Connect to Wifi.
  EXPECT_CALL(*item1.get(), Resume(_)).Times(1);
  EXPECT_CALL(*item2.get(), Resume(_)).Times(1);
  SetNetworkConnectionType(ConnectionType::CONNECTION_WIFI);
  task_runner_->FastForwardUntilNoTasksRemain();

  // Disconnect network again.
  EXPECT_CALL(*item1.get(), Resume(_)).Times(0);
  EXPECT_CALL(*item2.get(), Resume(_)).Times(0);
  SetNetworkConnectionType(ConnectionType::CONNECTION_NONE);
  task_runner_->FastForwardUntilNoTasksRemain();

  // Change network to metered.
  EXPECT_CALL(*item1.get(), Resume(_)).Times(0);
  EXPECT_CALL(*item2.get(), Resume(_)).Times(1);
  SetNetworkConnectionType(ConnectionType::CONNECTION_3G);
  task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(AutoResumptionHandlerTest, PausedDownloadsAreNotAutoResumed) {
  auto item = std::make_unique<NiceMock<MockDownloadItem>>();
  SetDownloadState(item.get(), DownloadItem::IN_PROGRESS, true, false);
  auto_resumption_handler_->OnDownloadStarted(item.get());

  SetNetworkConnectionType(ConnectionType::CONNECTION_NONE);
  task_runner_->FastForwardUntilNoTasksRemain();

  // Connect to Wifi.
  EXPECT_CALL(*item.get(), Resume(_)).Times(0);
  SetNetworkConnectionType(ConnectionType::CONNECTION_WIFI);
  task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(AutoResumptionHandlerTest,
       OnStartScheduledTaskWillResumeAllPendingDownloads) {
  SetNetworkConnectionType(ConnectionType::CONNECTION_WIFI);
  auto item = std::make_unique<NiceMock<MockDownloadItem>>();
  SetDownloadState(item.get(), DownloadItem::INTERRUPTED, false, false);
  auto_resumption_handler_->OnDownloadStarted(item.get());
  task_runner_->FastForwardUntilNoTasksRemain();

  // Start the task. It should resume all downloads.
  EXPECT_CALL(*item.get(), Resume(_)).Times(1);
  TaskFinishedCallback callback;
  auto_resumption_handler_->OnStartScheduledTask(
      DownloadTaskType::DOWNLOAD_AUTO_RESUMPTION_TASK, std::move(callback));
  task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(AutoResumptionHandlerTest, ExpiredDownloadNotAutoResumed) {
  SetNetworkConnectionType(ConnectionType::CONNECTION_WIFI);

  // Create a normal expired download.
  base::Time expired_start_time = GetNow() - base::Days(100);
  auto item0 = std::make_unique<NiceMock<MockDownloadItem>>();
  SetDownloadState(item0.get(), DownloadItem::INTERRUPTED, false, false);
  ON_CALL(*item0.get(), GetStartTime())
      .WillByDefault(Return(expired_start_time));

  auto_resumption_handler_->OnDownloadStarted(item0.get());
  task_runner_->FastForwardUntilNoTasksRemain();

  // Expired downoad |item0| won't be resumed.
  EXPECT_CALL(*item0.get(), Resume(_)).Times(0);

  TaskFinishedCallback callback;
  auto_resumption_handler_->OnStartScheduledTask(
      DownloadTaskType::DOWNLOAD_AUTO_RESUMPTION_TASK, std::move(callback));
  task_runner_->FastForwardUntilNoTasksRemain();
}

TEST_F(AutoResumptionHandlerTest, DownloadWithoutTargetPathNotAutoResumed) {
  SetNetworkConnectionType(ConnectionType::CONNECTION_WIFI);
  auto item = std::make_unique<NiceMock<MockDownloadItem>>();
  SetDownloadState(item.get(), DownloadItem::INTERRUPTED, false, false, false);
  auto_resumption_handler_->OnDownloadStarted(item.get());
  task_runner_->FastForwardUntilNoTasksRemain();

  EXPECT_CALL(*item.get(), Resume(_)).Times(0);
  auto_resumption_handler_->OnStartScheduledTask(
      DownloadTaskType::DOWNLOAD_AUTO_RESUMPTION_TASK, base::DoNothing());
  task_runner_->FastForwardUntilNoTasksRemain();
}

}  // namespace
}  // namespace download
