// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_data_type_controller.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/history/core/browser/history_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/data_type_controller_mock.h"
#include "components/sync/driver/fake_sync_client.h"
#include "components/sync/driver/fake_sync_service.h"
#include "components/sync/driver/model_associator_mock.h"
#include "components/sync/driver/sync_api_component_factory_mock.h"
#include "components/sync/model/change_processor_mock.h"
#include "components/sync/model/sync_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using bookmarks::BookmarkModel;
using sync_bookmarks::BookmarkDataTypeController;
using syncer::ChangeProcessorMock;
using syncer::DataTypeController;
using syncer::ModelAssociatorMock;
using syncer::ModelLoadCallbackMock;
using syncer::StartCallbackMock;
using testing::_;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::NiceMock;
using testing::Return;
using testing::SetArgPointee;

namespace {

class HistoryMock : public history::HistoryService {
 public:
  HistoryMock() : history::HistoryService() {}
  MOCK_METHOD0(BackendLoaded, bool(void));

  ~HistoryMock() override {}
};

}  // namespace

class SyncBookmarkDataTypeControllerTest : public testing::Test,
                                           public syncer::FakeSyncClient {
 public:
  SyncBookmarkDataTypeControllerTest() {}

  // FakeSyncClient overrides.
  BookmarkModel* GetBookmarkModel() override { return bookmark_model_.get(); }
  history::HistoryService* GetHistoryService() override {
    return history_service_.get();
  }
  syncer::SyncService* GetSyncService() override { return &service_; }
  syncer::SyncApiComponentFactory* GetSyncApiComponentFactory() override {
    return &components_factory_;
  }

  void SetUp() override {
    model_associator_deleter_ =
        std::make_unique<NiceMock<ModelAssociatorMock>>();
    change_processor_deleter_ =
        std::make_unique<NiceMock<ChangeProcessorMock>>();
    model_associator_ = model_associator_deleter_.get();
    change_processor_ = change_processor_deleter_.get();
    history_service_ = std::make_unique<HistoryMock>();
    bookmark_dtc_ =
        std::make_unique<BookmarkDataTypeController>(base::DoNothing(), this);

    ON_CALL(components_factory_, CreateBookmarkSyncComponents(_))
        .WillByDefault(testing::InvokeWithoutArgs([=]() {
          syncer::SyncApiComponentFactory::SyncComponents components;
          components.model_associator = std::move(model_associator_deleter_);
          components.change_processor = std::move(change_processor_deleter_);
          return components;
        }));
  }

 protected:
  enum BookmarkLoadPolicy {
    DONT_LOAD_MODEL,
    LOAD_MODEL,
  };

  void CreateBookmarkModel(BookmarkLoadPolicy bookmark_load_policy) {
    bookmark_model_ = std::make_unique<BookmarkModel>(
        std::make_unique<bookmarks::TestBookmarkClient>());
    if (bookmark_load_policy == LOAD_MODEL) {
      TestingPrefServiceSimple prefs;
      bookmark_model_->Load(&prefs, base::FilePath(),
                            base::SequencedTaskRunnerHandle::Get(),
                            base::SequencedTaskRunnerHandle::Get());
      bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_.get());
    }
  }

  void SetStartExpectations() {
    EXPECT_CALL(*history_service_.get(), BackendLoaded())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(model_load_callback_, Run(_, _));
  }

  void SetAssociateExpectations() {
    EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
        WillRepeatedly(Return(true));
    EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_))
        .WillRepeatedly(DoAll(SetArgPointee<0>(true), Return(true)));
    EXPECT_CALL(*model_associator_, AssociateModels(_, _)).
        WillRepeatedly(Return(syncer::SyncError()));
  }

  void SetStopExpectations() {
    EXPECT_CALL(*model_associator_, DisassociateModels()).
                WillOnce(Return(syncer::SyncError()));
  }

  void Start() {
    bookmark_dtc_->LoadModels(
        syncer::ConfigureContext(),
        base::Bind(&ModelLoadCallbackMock::Run,
                   base::Unretained(&model_load_callback_)));
    bookmark_dtc_->StartAssociating(
        base::Bind(&StartCallbackMock::Run,
                   base::Unretained(&start_callback_)));
    base::RunLoop().RunUntilIdle();
  }

  void NotifyHistoryServiceLoaded() {
    history_service_->NotifyHistoryServiceLoaded();
  }

  base::MessageLoop message_loop_;
  testing::NiceMock<syncer::SyncApiComponentFactoryMock> components_factory_;
  std::unique_ptr<BookmarkModel> bookmark_model_;
  std::unique_ptr<HistoryMock> history_service_;
  std::unique_ptr<BookmarkDataTypeController> bookmark_dtc_;
  syncer::FakeSyncService service_;
  ModelAssociatorMock* model_associator_;
  ChangeProcessorMock* change_processor_;
  std::unique_ptr<ModelAssociatorMock> model_associator_deleter_;
  std::unique_ptr<ChangeProcessorMock> change_processor_deleter_;
  StartCallbackMock start_callback_;
  ModelLoadCallbackMock model_load_callback_;
};

TEST_F(SyncBookmarkDataTypeControllerTest, StartDependentsReady) {
  CreateBookmarkModel(LOAD_MODEL);
  SetStartExpectations();
  SetAssociateExpectations();

  EXPECT_EQ(DataTypeController::NOT_RUNNING, bookmark_dtc_->state());

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _, _));
  Start();
  EXPECT_EQ(DataTypeController::RUNNING, bookmark_dtc_->state());
}

TEST_F(SyncBookmarkDataTypeControllerTest, StartBookmarkModelNotReady) {
  CreateBookmarkModel(DONT_LOAD_MODEL);
  SetStartExpectations();
  SetAssociateExpectations();

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _, _));
  bookmark_dtc_->LoadModels(
      syncer::ConfigureContext(),
      base::Bind(&ModelLoadCallbackMock::Run,
                 base::Unretained(&model_load_callback_)));
  EXPECT_EQ(DataTypeController::MODEL_STARTING, bookmark_dtc_->state());

  TestingPrefServiceSimple prefs;
  bookmark_model_->Load(&prefs, base::FilePath(),
                        base::SequencedTaskRunnerHandle::Get(),
                        base::SequencedTaskRunnerHandle::Get());
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_.get());
  EXPECT_EQ(DataTypeController::MODEL_LOADED, bookmark_dtc_->state());

  bookmark_dtc_->StartAssociating(
      base::Bind(&StartCallbackMock::Run,
                 base::Unretained(&start_callback_)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(DataTypeController::RUNNING, bookmark_dtc_->state());
}

TEST_F(SyncBookmarkDataTypeControllerTest, StartHistoryServiceNotReady) {
  CreateBookmarkModel(LOAD_MODEL);
  SetStartExpectations();
  EXPECT_CALL(*history_service_.get(), BackendLoaded())
      .WillRepeatedly(Return(false));

  bookmark_dtc_->LoadModels(
      syncer::ConfigureContext(),
      base::Bind(&ModelLoadCallbackMock::Run,
                 base::Unretained(&model_load_callback_)));

  EXPECT_EQ(DataTypeController::MODEL_STARTING, bookmark_dtc_->state());
  testing::Mock::VerifyAndClearExpectations(history_service_.get());
  EXPECT_CALL(*history_service_.get(), BackendLoaded())
      .WillRepeatedly(Return(true));

  // Send the notification that the history service has finished loading the db.
  NotifyHistoryServiceLoaded();
  EXPECT_EQ(DataTypeController::MODEL_LOADED, bookmark_dtc_->state());
}

TEST_F(SyncBookmarkDataTypeControllerTest, StartFirstRun) {
  CreateBookmarkModel(LOAD_MODEL);
  SetStartExpectations();
  SetAssociateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(false), Return(true)));
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK_FIRST_RUN, _, _));
  Start();
}

TEST_F(SyncBookmarkDataTypeControllerTest, StartBusy) {
  CreateBookmarkModel(LOAD_MODEL);
  EXPECT_CALL(*history_service_.get(), BackendLoaded())
      .WillRepeatedly(Return(false));

  EXPECT_CALL(model_load_callback_, Run(_, _));
  bookmark_dtc_->LoadModels(
      syncer::ConfigureContext(),
      base::Bind(&ModelLoadCallbackMock::Run,
                 base::Unretained(&model_load_callback_)));
  bookmark_dtc_->LoadModels(
      syncer::ConfigureContext(),
      base::Bind(&ModelLoadCallbackMock::Run,
                 base::Unretained(&model_load_callback_)));
}

TEST_F(SyncBookmarkDataTypeControllerTest, StartOk) {
  CreateBookmarkModel(LOAD_MODEL);
  SetStartExpectations();
  SetAssociateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(true), Return(true)));

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _, _));
  Start();
}

TEST_F(SyncBookmarkDataTypeControllerTest, StartAssociationFailed) {
  CreateBookmarkModel(LOAD_MODEL);
  SetStartExpectations();
  // Set up association to fail.
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(true), Return(true)));
  EXPECT_CALL(*model_associator_, AssociateModels(_, _)).
      WillRepeatedly(Return(syncer::SyncError(FROM_HERE,
                                              syncer::SyncError::DATATYPE_ERROR,
                                              "error",
                                              syncer::BOOKMARKS)));

  EXPECT_CALL(start_callback_,
              Run(DataTypeController::ASSOCIATION_FAILED, _, _));
  Start();
  EXPECT_EQ(DataTypeController::FAILED, bookmark_dtc_->state());
}

TEST_F(SyncBookmarkDataTypeControllerTest,
       StartAssociationTriggersUnrecoverableError) {
  CreateBookmarkModel(LOAD_MODEL);
  SetStartExpectations();
  // Set up association to fail with an unrecoverable error.
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(false), Return(false)));
  EXPECT_CALL(start_callback_,
              Run(DataTypeController::UNRECOVERABLE_ERROR, _, _));
  Start();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, bookmark_dtc_->state());
}

TEST_F(SyncBookmarkDataTypeControllerTest, StartAborted) {
  CreateBookmarkModel(LOAD_MODEL);
  EXPECT_CALL(*history_service_.get(), BackendLoaded())
      .WillRepeatedly(Return(false));

  bookmark_dtc_->LoadModels(
      syncer::ConfigureContext(),
      base::Bind(&ModelLoadCallbackMock::Run,
                 base::Unretained(&model_load_callback_)));

  bookmark_dtc_->Stop(syncer::STOP_SYNC);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, bookmark_dtc_->state());
}

TEST_F(SyncBookmarkDataTypeControllerTest, Stop) {
  CreateBookmarkModel(LOAD_MODEL);
  SetStartExpectations();
  SetAssociateExpectations();
  SetStopExpectations();

  EXPECT_EQ(DataTypeController::NOT_RUNNING, bookmark_dtc_->state());

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _, _));
  Start();
  EXPECT_EQ(DataTypeController::RUNNING, bookmark_dtc_->state());
  bookmark_dtc_->Stop(syncer::STOP_SYNC);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, bookmark_dtc_->state());
}
