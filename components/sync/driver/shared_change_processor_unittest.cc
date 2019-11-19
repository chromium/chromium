// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/shared_change_processor.h"

#include <cstddef>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/data_type_manager.h"
#include "components/sync/driver/generic_change_processor.h"
#include "components/sync/driver/generic_change_processor_factory.h"
#include "components/sync/driver/sync_client_mock.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync/model/data_type_error_handler_mock.h"
#include "components/sync/model/fake_syncable_service.h"
#include "components/sync/syncable/test_user_share.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::StrictMock;

class SyncSharedChangeProcessorTest : public testing::Test {
 public:
  SyncSharedChangeProcessorTest()
      : model_thread_("dbthread"), did_connect_(false) {}

  ~SyncSharedChangeProcessorTest() override {
    EXPECT_FALSE(db_syncable_service_);
  }

 protected:
  base::WeakPtr<SyncableService> GetSyncableServiceForType(ModelType type) {
    DCHECK(model_thread_.task_runner()->BelongsToCurrentThread());
    return db_syncable_service_->AsWeakPtr();
  }

  void SetUp() override {
    test_user_share_.SetUp();
    shared_change_processor_ = new SharedChangeProcessor(AUTOFILL);
    ON_CALL(sync_client_, GetSyncableServiceForType(AUTOFILL))
        .WillByDefault(testing::Invoke(
            this, &SyncSharedChangeProcessorTest::GetSyncableServiceForType));
    ASSERT_TRUE(model_thread_.Start());
    ASSERT_TRUE(model_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&SyncSharedChangeProcessorTest::SetUpDBSyncableService,
                       base::Unretained(this))));
  }

  void TearDown() override {
    EXPECT_TRUE(model_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SyncSharedChangeProcessorTest::TearDownDBSyncableService,
            base::Unretained(this))));
    // This must happen before the DB thread is stopped since
    // |shared_change_processor_| may post tasks to delete its members
    // on the correct thread.
    //
    // TODO(akalin): Write deterministic tests for the destruction of
    // |shared_change_processor_| on the UI and DB threads.
    shared_change_processor_ = nullptr;
    model_thread_.Stop();

    // Note: Stop() joins the threads, and that barrier prevents this read
    // from being moved (e.g by compiler optimization) in such a way that it
    // would race with the write in ConnectOnDBThread (because by this time,
    // everything that could have run on |model_thread_| has done so).
    ASSERT_TRUE(did_connect_);
    test_user_share_.TearDown();
  }

  // Connect |shared_change_processor_| on the DB thread.
  void Connect() {
    EXPECT_TRUE(model_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&SyncSharedChangeProcessorTest::ConnectOnDBThread,
                       base::Unretained(this), shared_change_processor_)));
  }

 private:
  // Used by SetUp().
  void SetUpDBSyncableService() {
    DCHECK(model_thread_.task_runner()->BelongsToCurrentThread());
    DCHECK(!db_syncable_service_);
    db_syncable_service_ = std::make_unique<FakeSyncableService>();
  }

  // Used by TearDown().
  void TearDownDBSyncableService() {
    DCHECK(model_thread_.task_runner()->BelongsToCurrentThread());
    DCHECK(db_syncable_service_);
    db_syncable_service_.reset();
  }

  // Used by Connect().  The SharedChangeProcessor is passed in
  // because we modify |shared_change_processor_| on the main thread
  // (in TearDown()).
  void ConnectOnDBThread(
      const scoped_refptr<SharedChangeProcessor>& shared_change_processor) {
    DCHECK(model_thread_.task_runner()->BelongsToCurrentThread());
    EXPECT_TRUE(shared_change_processor->Connect(
        &sync_client_, &processor_factory_, test_user_share_.user_share(),
        std::make_unique<DataTypeErrorHandlerMock>(),
        base::WeakPtr<SyncMergeResult>()));
    did_connect_ = true;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::Thread model_thread_;
  TestUserShare test_user_share_;

  scoped_refptr<SharedChangeProcessor> shared_change_processor_;

  GenericChangeProcessorFactory processor_factory_;
  bool did_connect_;

  // Used only on DB thread.
  std::unique_ptr<FakeSyncableService> db_syncable_service_;

  testing::NiceMock<SyncClientMock> sync_client_;
};

// Simply connect the shared change processor.  It should succeed, and
// nothing further should happen.
TEST_F(SyncSharedChangeProcessorTest, Basic) {
  Connect();
}

}  // namespace

}  // namespace syncer
