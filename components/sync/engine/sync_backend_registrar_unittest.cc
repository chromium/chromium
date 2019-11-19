// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/sync_backend_registrar.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "components/sync/engine/passive_model_worker.h"
#include "components/sync/engine/sequenced_model_worker.h"
#include "components/sync/model/change_processor_mock.h"
#include "components/sync/syncable/test_user_share.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;

class SyncBackendRegistrarTest : public testing::Test {
 public:
  SyncBackendRegistrarTest()
      : db_thread_("DBThreadForTest"),
        sync_thread_("SyncThreadForTest") {}

  void SetUp() override {
    db_thread_.StartAndWaitForTesting();
    sync_thread_.StartAndWaitForTesting();
    test_user_share_.SetUp();
    registrar_ = std::make_unique<SyncBackendRegistrar>(
        "test", base::Bind(&SyncBackendRegistrarTest::CreateModelWorkerForGroup,
                           base::Unretained(this)));
  }

  void TearDown() override {
    registrar_->RequestWorkerStopOnUIThread();
    test_user_share_.TearDown();
    sync_thread_.task_runner()->DeleteSoon(FROM_HERE, registrar_.release());
    sync_thread_.FlushForTesting();
  }

  void TriggerChanges(ModelType type) {
    registrar_->OnChangesApplied(type, 0, nullptr, ImmutableChangeRecordList());
    registrar_->OnChangesComplete(type);
  }

  void ExpectRoutingInfo(const ModelSafeRoutingInfo& expected_routing_info) {
    ModelSafeRoutingInfo actual_routing_info;
    registrar_->GetModelSafeRoutingInfo(&actual_routing_info);
    EXPECT_EQ(expected_routing_info, actual_routing_info);
  }

  void ExpectHasProcessorsForTypes(ModelTypeSet types) {
    for (int i = FIRST_REAL_MODEL_TYPE; i < ModelType::NUM_ENTRIES; ++i) {
      ModelType model_type = ModelTypeFromInt(i);
      EXPECT_EQ(types.Has(model_type),
                registrar_->IsTypeActivatedForTest(model_type));
    }
  }

  size_t GetWorkersSize() {
    std::vector<scoped_refptr<ModelSafeWorker>> workers;
    registrar_->GetWorkers(&workers);
    return workers.size();
  }

  SyncBackendRegistrar* registrar() { return registrar_.get(); }
  UserShare* user_share() { return test_user_share_.user_share(); }
  scoped_refptr<base::SequencedTaskRunner> db_task_runner() {
    return db_thread_.task_runner();
  }

 private:
  scoped_refptr<ModelSafeWorker> CreateModelWorkerForGroup(
      ModelSafeGroup group) {
    switch (group) {
      case GROUP_UI:
        return new SequencedModelWorker(
            task_environment_.GetMainThreadTaskRunner(), group);
      case GROUP_PASSIVE:
        return new PassiveModelWorker();
      default:
        return nullptr;
    }
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::Thread db_thread_;
  base::Thread sync_thread_;

  TestUserShare test_user_share_;
  std::unique_ptr<SyncBackendRegistrar> registrar_;
};

TEST_F(SyncBackendRegistrarTest, ConstructorEmpty) {
  registrar()->SetInitialTypes(ModelTypeSet());
  EXPECT_FALSE(registrar()->IsNigoriEnabled());
  EXPECT_EQ(2u, GetWorkersSize());
  ExpectRoutingInfo(ModelSafeRoutingInfo());
  ExpectHasProcessorsForTypes(ModelTypeSet());
}

TEST_F(SyncBackendRegistrarTest, ConstructorNonEmpty) {
  registrar()->RegisterNonBlockingType(BOOKMARKS);
  registrar()->SetInitialTypes(ModelTypeSet(BOOKMARKS, NIGORI, PASSWORDS));
  EXPECT_TRUE(registrar()->IsNigoriEnabled());
  EXPECT_EQ(2u, GetWorkersSize());
  EXPECT_EQ(ModelTypeSet(NIGORI), registrar()->GetLastConfiguredTypes());
  // Bookmarks dropped because it is nonblocking.
  // Passwords dropped because of no password store.
  ExpectRoutingInfo({{NIGORI, GROUP_PASSIVE}});
  ExpectHasProcessorsForTypes(ModelTypeSet());
}

TEST_F(SyncBackendRegistrarTest, ConstructorNonEmptyReversedInitialization) {
  // The blocking types get to set initial types before NonBlocking types here.
  registrar()->SetInitialTypes(ModelTypeSet(BOOKMARKS, NIGORI, PASSWORDS));
  registrar()->RegisterNonBlockingType(BOOKMARKS);
  EXPECT_TRUE(registrar()->IsNigoriEnabled());
  EXPECT_EQ(2u, GetWorkersSize());
  EXPECT_EQ(ModelTypeSet(NIGORI), registrar()->GetLastConfiguredTypes());
  // Bookmarks dropped because it is nonblocking.
  // Passwords dropped because of no password store.
  ExpectRoutingInfo({{NIGORI, GROUP_PASSIVE}});
  ExpectHasProcessorsForTypes(ModelTypeSet());
}

TEST_F(SyncBackendRegistrarTest, ConfigureDataTypes) {
  registrar()->RegisterNonBlockingType(BOOKMARKS);
  registrar()->SetInitialTypes(ModelTypeSet());

  // Add.
  const ModelTypeSet types1(BOOKMARKS, NIGORI, AUTOFILL);
  EXPECT_EQ(types1, registrar()->ConfigureDataTypes(types1, ModelTypeSet()));
  ExpectRoutingInfo({{BOOKMARKS, GROUP_NON_BLOCKING},
                     {NIGORI, GROUP_PASSIVE},
                     {AUTOFILL, GROUP_PASSIVE}});
  ExpectHasProcessorsForTypes(ModelTypeSet());
  EXPECT_EQ(types1, registrar()->GetLastConfiguredTypes());

  // Add and remove.
  const ModelTypeSet types2(PREFERENCES, THEMES);
  EXPECT_EQ(types2, registrar()->ConfigureDataTypes(types2, types1));

  ExpectRoutingInfo({{PREFERENCES, GROUP_PASSIVE}, {THEMES, GROUP_PASSIVE}});
  ExpectHasProcessorsForTypes(ModelTypeSet());
  EXPECT_EQ(types2, registrar()->GetLastConfiguredTypes());

  // Remove.
  EXPECT_TRUE(registrar()->ConfigureDataTypes(ModelTypeSet(), types2).Empty());
  ExpectRoutingInfo(ModelSafeRoutingInfo());
  ExpectHasProcessorsForTypes(ModelTypeSet());
  EXPECT_EQ(ModelTypeSet(), registrar()->GetLastConfiguredTypes());
}

TEST_F(SyncBackendRegistrarTest, ActivateDeactivateUIDataType) {
  InSequence in_sequence;
  registrar()->SetInitialTypes(ModelTypeSet());

  // Should do nothing.
  TriggerChanges(BOOKMARKS);

  StrictMock<ChangeProcessorMock> change_processor_mock;
  EXPECT_CALL(change_processor_mock, StartImpl());
  EXPECT_CALL(change_processor_mock, IsRunning()).WillRepeatedly(Return(true));
  EXPECT_CALL(change_processor_mock, ApplyChangesFromSyncModel(nullptr, _, _));
  EXPECT_CALL(change_processor_mock, IsRunning()).WillRepeatedly(Return(true));
  EXPECT_CALL(change_processor_mock, CommitChangesFromSyncModel());
  EXPECT_CALL(change_processor_mock, IsRunning()).WillRepeatedly(Return(false));

  const ModelTypeSet types(BOOKMARKS);
  EXPECT_EQ(types, registrar()->ConfigureDataTypes(types, ModelTypeSet()));
  registrar()->ActivateDataType(BOOKMARKS, GROUP_UI, &change_processor_mock,
                                user_share());
  ExpectRoutingInfo({{BOOKMARKS, GROUP_UI}});
  ExpectHasProcessorsForTypes(types);

  TriggerChanges(BOOKMARKS);

  registrar()->DeactivateDataType(BOOKMARKS);
  ExpectRoutingInfo(ModelSafeRoutingInfo());
  ExpectHasProcessorsForTypes(ModelTypeSet());

  // Should do nothing.
  TriggerChanges(BOOKMARKS);
}

// Tests that registration and configuration of non-blocking data types is
// handled correctly in SyncBackendRegistrar.
TEST_F(SyncBackendRegistrarTest, ConfigureNonBlockingDataType) {
  registrar()->RegisterNonBlockingType(AUTOFILL);
  registrar()->RegisterNonBlockingType(BOOKMARKS);

  ExpectRoutingInfo(ModelSafeRoutingInfo());
  // Simulate that initial sync was already done for AUTOFILL.
  registrar()->AddRestoredNonBlockingType(AUTOFILL);
  // It should be added to routing info and set of configured types.
  EXPECT_EQ(ModelTypeSet(AUTOFILL), registrar()->GetLastConfiguredTypes());
  ExpectRoutingInfo({{AUTOFILL, GROUP_NON_BLOCKING}});

  // Configure two non-blocking types. Initial sync wasn't done for BOOKMARKS so
  // it should be included in types to be downloaded.
  ModelTypeSet types_to_add(AUTOFILL, BOOKMARKS);
  ModelTypeSet newly_added_types =
      registrar()->ConfigureDataTypes(types_to_add, ModelTypeSet());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS), newly_added_types);
  EXPECT_EQ(types_to_add, registrar()->GetLastConfiguredTypes());
  ExpectRoutingInfo(
      {{AUTOFILL, GROUP_NON_BLOCKING}, {BOOKMARKS, GROUP_NON_BLOCKING}});
}

}  // namespace

}  // namespace syncer
