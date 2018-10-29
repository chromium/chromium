// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/async_directory_type_controller.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/sync/driver/async_directory_type_controller_mock.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/data_type_controller_mock.h"
#include "components/sync/driver/fake_sync_client.h"
#include "components/sync/driver/generic_change_processor_factory.h"
#include "components/sync/engine/model_safe_worker.h"
#include "components/sync/model/fake_syncable_service.h"
#include "components/sync/model/sync_change.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class SyncClient;

namespace {

using base::WaitableEvent;
using testing::_;
using testing::AtLeast;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;

const ModelType kType = AUTOFILL_PROFILE;

ACTION_P(WaitOnEvent, event) {
  event->Wait();
}

ACTION_P(SignalEvent, event) {
  event->Signal();
}

class SharedChangeProcessorMock : public SharedChangeProcessor {
 public:
  explicit SharedChangeProcessorMock(ModelType type)
      : SharedChangeProcessor(type) {}

  base::WeakPtr<SyncableService> Connect(
      SyncClient*,
      GenericChangeProcessorFactory*,
      UserShare*,
      std::unique_ptr<DataTypeErrorHandler>,
      const base::WeakPtr<SyncMergeResult>&) override {
    return std::move(connect_return_);
  }
  MOCK_METHOD0(Disconnect, bool());
  MOCK_METHOD2(ProcessSyncChanges,
               SyncError(const base::Location&, const SyncChangeList&));
  MOCK_CONST_METHOD2(GetAllSyncDataReturnError,
                     SyncError(ModelType, SyncDataList*));
  MOCK_METHOD0(GetSyncCount, int());
  MOCK_METHOD1(SyncModelHasUserCreatedNodes, bool(bool*));
  MOCK_METHOD0(CryptoReadyIfNecessary, bool());
  MOCK_CONST_METHOD1(GetDataTypeContext, bool(std::string*));
  MOCK_METHOD1(RecordAssociationTime, void(base::TimeDelta time));

  void SetConnectReturn(base::WeakPtr<SyncableService> service) {
    connect_return_ = service;
  }

 protected:
  ~SharedChangeProcessorMock() override { DCHECK(!connect_return_); }
  MOCK_METHOD2(OnUnrecoverableError,
               void(const base::Location&, const std::string&));

 private:
  base::WeakPtr<SyncableService> connect_return_;
  DISALLOW_COPY_AND_ASSIGN(SharedChangeProcessorMock);
};

class AsyncDirectoryTypeControllerFake : public AsyncDirectoryTypeController {
 public:
  AsyncDirectoryTypeControllerFake(
      SyncClient* sync_client,
      AsyncDirectoryTypeControllerMock* mock,
      SharedChangeProcessor* change_processor,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
      : AsyncDirectoryTypeController(kType,
                                     base::Closure(),
                                     sync_client,
                                     GROUP_DB,
                                     nullptr),
        blocked_(false),
        mock_(mock),
        change_processor_(change_processor),
        backend_task_runner_(backend_task_runner) {}
  ~AsyncDirectoryTypeControllerFake() override {}

  // Prevent tasks from being posted on the backend thread until
  // UnblockBackendTasks() is called.
  void BlockBackendTasks() { blocked_ = true; }

  // Post pending tasks on the backend thread and start allowing tasks
  // to be posted on the backend thread again.
  void UnblockBackendTasks() {
    blocked_ = false;
    for (std::vector<PendingTask>::const_iterator it = pending_tasks_.begin();
         it != pending_tasks_.end(); ++it) {
      PostTaskOnModelThread(it->from_here, it->task);
    }
    pending_tasks_.clear();
  }

  SharedChangeProcessor* CreateSharedChangeProcessor() override {
    return change_processor_.get();
  }

  std::unique_ptr<DataTypeErrorHandler> CreateErrorHandler() override {
    return AsyncDirectoryTypeController::CreateErrorHandler();
  }

 protected:
  bool PostTaskOnModelThread(const base::Location& from_here,
                             const base::Closure& task) override {
    if (blocked_) {
      pending_tasks_.push_back(PendingTask(from_here, task));
      return true;
    } else {
      return backend_task_runner_->PostTask(from_here, task);
    }
  }

  // We mock the following methods because their default implementations do
  // nothing, but we still want to make sure they're called appropriately.
  bool StartModels() override { return mock_->StartModels(); }
  void StopModels() override { mock_->StopModels(); }
  void RecordStartFailure(DataTypeController::ConfigureResult result) override {
    mock_->RecordStartFailure(result);
  }

 private:
  struct PendingTask {
    PendingTask(const base::Location& from_here, const base::Closure& task)
        : from_here(from_here), task(task) {}

    base::Location from_here;
    base::Closure task;
  };

  bool blocked_;
  std::vector<PendingTask> pending_tasks_;
  AsyncDirectoryTypeControllerMock* mock_;
  scoped_refptr<SharedChangeProcessor> change_processor_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(AsyncDirectoryTypeControllerFake);
};

class SyncAsyncDirectoryTypeControllerTest : public testing::Test,
                                             public FakeSyncClient {
 public:
  SyncAsyncDirectoryTypeControllerTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI),
        backend_thread_("dbthread") {}

  void SetUp() override {
    backend_thread_.Start();
    change_processor_ = new SharedChangeProcessorMock(kType);
    // All of these are refcounted, so don't need to be released.
    dtc_mock_ =
        std::make_unique<StrictMock<AsyncDirectoryTypeControllerMock>>();
    non_ui_dtc_ = std::make_unique<AsyncDirectoryTypeControllerFake>(
        this, dtc_mock_.get(), change_processor_.get(),
        backend_thread_.task_runner());
  }

  void TearDown() override { backend_thread_.Stop(); }

  void WaitForDTC() {
    WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                       base::WaitableEvent::InitialState::NOT_SIGNALED);
    backend_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&SyncAsyncDirectoryTypeControllerTest::SignalDone,
                       &done));
    done.TimedWait(TestTimeouts::action_timeout());
    if (!done.IsSignaled()) {
      ADD_FAILURE() << "Timed out waiting for DB thread to finish.";
    }
    base::RunLoop().RunUntilIdle();
  }

  SyncService* GetSyncService() override {
    // Make sure this isn't called on backend_thread.
    EXPECT_FALSE(backend_thread_.task_runner()->BelongsToCurrentThread());
    return FakeSyncClient::GetSyncService();
  }

 protected:
  void SetStartExpectations() {
    EXPECT_CALL(*dtc_mock_, StartModels()).WillOnce(Return(true));
    EXPECT_CALL(model_load_callback_, Run(_, _));
  }

  void SetAssociateExpectations() {
    change_processor_->SetConnectReturn(syncable_service_.AsWeakPtr());
    EXPECT_CALL(*change_processor_, CryptoReadyIfNecessary())
        .WillOnce(Return(true));
    EXPECT_CALL(*change_processor_, SyncModelHasUserCreatedNodes(_))
        .WillOnce(DoAll(SetArgPointee<0>(true), Return(true)));
    EXPECT_CALL(*change_processor_, GetAllSyncDataReturnError(_, _))
        .WillOnce(Return(SyncError()));
    EXPECT_CALL(*change_processor_, GetSyncCount()).WillOnce(Return(0));
    EXPECT_CALL(*change_processor_, RecordAssociationTime(_));
  }

  void SetActivateExpectations(DataTypeController::ConfigureResult result) {
    EXPECT_CALL(start_callback_, Run(result, _, _));
  }

  void SetStopExpectations() {
    EXPECT_CALL(*dtc_mock_, StopModels());
    EXPECT_CALL(*change_processor_, Disconnect()).WillOnce(Return(true));
  }

  void SetStartFailExpectations(DataTypeController::ConfigureResult result) {
    EXPECT_CALL(*dtc_mock_, StopModels()).Times(AtLeast(1));
    EXPECT_CALL(*dtc_mock_, RecordStartFailure(result));
    EXPECT_CALL(start_callback_, Run(result, _, _));
  }

  void Start() {
    non_ui_dtc_->LoadModels(
        ConfigureContext(),
        base::Bind(&ModelLoadCallbackMock::Run,
                   base::Unretained(&model_load_callback_)));
    non_ui_dtc_->StartAssociating(base::Bind(
        &StartCallbackMock::Run, base::Unretained(&start_callback_)));
  }

  static void SignalDone(WaitableEvent* done) { done->Signal(); }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::Thread backend_thread_;

  StartCallbackMock start_callback_;
  ModelLoadCallbackMock model_load_callback_;
  // Must be destroyed after non_ui_dtc_.
  FakeSyncableService syncable_service_;
  std::unique_ptr<AsyncDirectoryTypeControllerFake> non_ui_dtc_;
  std::unique_ptr<AsyncDirectoryTypeControllerMock> dtc_mock_;
  scoped_refptr<SharedChangeProcessorMock> change_processor_;
  std::unique_ptr<SyncChangeProcessor> saved_change_processor_;
};

TEST_F(SyncAsyncDirectoryTypeControllerTest, StartOk) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations(DataTypeController::OK);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  Start();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::RUNNING, non_ui_dtc_->state());
}

TEST_F(SyncAsyncDirectoryTypeControllerTest, StartFirstRun) {
  SetStartExpectations();
  change_processor_->SetConnectReturn(syncable_service_.AsWeakPtr());
  EXPECT_CALL(*change_processor_, CryptoReadyIfNecessary())
      .WillOnce(Return(true));
  EXPECT_CALL(*change_processor_, SyncModelHasUserCreatedNodes(_))
      .WillOnce(DoAll(SetArgPointee<0>(false), Return(true)));
  EXPECT_CALL(*change_processor_, GetAllSyncDataReturnError(_, _))
      .WillOnce(Return(SyncError()));
  EXPECT_CALL(*change_processor_, RecordAssociationTime(_));
  SetActivateExpectations(DataTypeController::OK_FIRST_RUN);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  Start();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::RUNNING, non_ui_dtc_->state());
}

// Start the DTC and have StartModels() return false.  Then, stop the
// DTC without finishing model startup.  It should stop cleanly.
TEST_F(SyncAsyncDirectoryTypeControllerTest, AbortDuringStartModels) {
  EXPECT_CALL(*dtc_mock_, StartModels()).WillOnce(Return(false));
  EXPECT_CALL(*dtc_mock_, StopModels());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  non_ui_dtc_->LoadModels(ConfigureContext(),
                          base::Bind(&ModelLoadCallbackMock::Run,
                                     base::Unretained(&model_load_callback_)));
  WaitForDTC();
  EXPECT_EQ(DataTypeController::MODEL_STARTING, non_ui_dtc_->state());
  non_ui_dtc_->Stop(STOP_SYNC);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
}

// Start the DTC and have MergeDataAndStartSyncing() return an error.
// The DTC should become disabled, and the DTC should still stop
// cleanly.
TEST_F(SyncAsyncDirectoryTypeControllerTest, StartAssociationFailed) {
  SetStartExpectations();
  change_processor_->SetConnectReturn(syncable_service_.AsWeakPtr());
  EXPECT_CALL(*change_processor_, CryptoReadyIfNecessary())
      .WillOnce(Return(true));
  EXPECT_CALL(*change_processor_, SyncModelHasUserCreatedNodes(_))
      .WillOnce(DoAll(SetArgPointee<0>(true), Return(true)));
  EXPECT_CALL(*change_processor_, GetAllSyncDataReturnError(_, _))
      .WillOnce(Return(SyncError()));
  EXPECT_CALL(*change_processor_, RecordAssociationTime(_));
  SetStartFailExpectations(DataTypeController::ASSOCIATION_FAILED);
  // Set up association to fail with an association failed error.
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  syncable_service_.set_merge_data_and_start_syncing_error(SyncError(
      FROM_HERE, SyncError::DATATYPE_ERROR, "Sync Error", non_ui_dtc_->type()));
  Start();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::FAILED, non_ui_dtc_->state());
  non_ui_dtc_->Stop(STOP_SYNC);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
}

TEST_F(SyncAsyncDirectoryTypeControllerTest,
       StartAssociationTriggersUnrecoverableError) {
  SetStartExpectations();
  SetStartFailExpectations(DataTypeController::UNRECOVERABLE_ERROR);
  // Set up association to fail with an unrecoverable error.
  change_processor_->SetConnectReturn(syncable_service_.AsWeakPtr());
  EXPECT_CALL(*change_processor_, CryptoReadyIfNecessary())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*change_processor_, SyncModelHasUserCreatedNodes(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(false), Return(false)));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  Start();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
}

TEST_F(SyncAsyncDirectoryTypeControllerTest, StartAssociationCryptoNotReady) {
  SetStartExpectations();
  SetStartFailExpectations(DataTypeController::NEEDS_CRYPTO);
  // Set up association to fail with a NEEDS_CRYPTO error.
  change_processor_->SetConnectReturn(syncable_service_.AsWeakPtr());
  EXPECT_CALL(*change_processor_, CryptoReadyIfNecessary())
      .WillRepeatedly(Return(false));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  Start();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
}

// Trigger a Stop() call when we check if the model associator has user created
// nodes.
TEST_F(SyncAsyncDirectoryTypeControllerTest, AbortDuringAssociation) {
  WaitableEvent wait_for_db_thread_pause(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent pause_db_thread(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  SetStartExpectations();
  change_processor_->SetConnectReturn(syncable_service_.AsWeakPtr());
  EXPECT_CALL(*change_processor_, CryptoReadyIfNecessary())
      .WillOnce(Return(true));
  EXPECT_CALL(*change_processor_, SyncModelHasUserCreatedNodes(_))
      .WillOnce(DoAll(SignalEvent(&wait_for_db_thread_pause),
                      WaitOnEvent(&pause_db_thread), SetArgPointee<0>(true),
                      Return(true)));
  EXPECT_CALL(*change_processor_, GetAllSyncDataReturnError(_, _))
      .WillOnce(Return(SyncError(FROM_HERE, SyncError::DATATYPE_ERROR,
                                 "Disconnected.", kType)));
  EXPECT_CALL(*dtc_mock_, StopModels());
  EXPECT_CALL(*change_processor_, Disconnect())
      .WillOnce(DoAll(SignalEvent(&pause_db_thread), Return(true)));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  Start();
  wait_for_db_thread_pause.Wait();
  non_ui_dtc_->Stop(STOP_SYNC);
  WaitForDTC();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
}

// Start the DTC while the backend tasks are blocked. Then stop the DTC before
// the backend tasks get a chance to run.
TEST_F(SyncAsyncDirectoryTypeControllerTest, StartAfterSyncShutdown) {
  non_ui_dtc_->BlockBackendTasks();

  SetStartExpectations();
  // We don't expect StopSyncing to be called because local_service_ will never
  // have been set.
  SetStopExpectations();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  Start();
  non_ui_dtc_->Stop(STOP_SYNC);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  Mock::VerifyAndClearExpectations(change_processor_.get());
  Mock::VerifyAndClearExpectations(dtc_mock_.get());

  non_ui_dtc_->UnblockBackendTasks();
  WaitForDTC();
}

TEST_F(SyncAsyncDirectoryTypeControllerTest, Stop) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations(DataTypeController::OK);
  SetStopExpectations();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  Start();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::RUNNING, non_ui_dtc_->state());
  non_ui_dtc_->Stop(STOP_SYNC);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
}

// Start the DTC then block its backend tasks.  While its backend
// tasks are blocked, stop and start it again, then unblock its
// backend tasks.  The (delayed) running of the backend tasks from the
// stop after the restart shouldn't cause any problems.
TEST_F(SyncAsyncDirectoryTypeControllerTest, StopStart) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations(DataTypeController::OK);
  SetStopExpectations();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  Start();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::RUNNING, non_ui_dtc_->state());

  non_ui_dtc_->BlockBackendTasks();
  non_ui_dtc_->Stop(STOP_SYNC);
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations(DataTypeController::OK);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  Start();
  non_ui_dtc_->UnblockBackendTasks();

  WaitForDTC();
  EXPECT_EQ(DataTypeController::RUNNING, non_ui_dtc_->state());
}

TEST_F(SyncAsyncDirectoryTypeControllerTest, OnUnrecoverableError) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations(DataTypeController::OK);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  Start();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::RUNNING, non_ui_dtc_->state());

  testing::Mock::VerifyAndClearExpectations(&start_callback_);
  EXPECT_CALL(model_load_callback_, Run(_, _));
  SyncError error(FROM_HERE, SyncError::DATATYPE_ERROR, "error",
                  non_ui_dtc_->type());
  backend_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DataTypeErrorHandler::OnUnrecoverableError,
                                non_ui_dtc_->CreateErrorHandler(), error));
  WaitForDTC();
}

}  // namespace

}  // namespace syncer
