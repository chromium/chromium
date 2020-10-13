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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class SyncBackendRegistrarTest : public testing::Test {
 public:
  SyncBackendRegistrarTest()
      : db_thread_("DBThreadForTest"),
        sync_thread_("SyncThreadForTest") {}

  void SetUp() override {
    db_thread_.StartAndWaitForTesting();
    sync_thread_.StartAndWaitForTesting();
    registrar_ = std::make_unique<SyncBackendRegistrar>(
        "test", base::BindRepeating(
                    &SyncBackendRegistrarTest::CreateModelWorkerForGroup,
                    base::Unretained(this)));
  }

  void TearDown() override {
    registrar_->RequestWorkerStopOnUIThread();
    sync_thread_.task_runner()->DeleteSoon(FROM_HERE, registrar_.release());
    sync_thread_.FlushForTesting();
  }

  void ExpectRoutingInfo(const ModelSafeRoutingInfo& expected_routing_info) {
    ModelSafeRoutingInfo actual_routing_info;
    registrar_->GetModelSafeRoutingInfo(&actual_routing_info);
    EXPECT_EQ(expected_routing_info, actual_routing_info);
  }

  size_t GetWorkersSize() {
    std::vector<scoped_refptr<ModelSafeWorker>> workers;
    registrar_->GetWorkers(&workers);
    return workers.size();
  }

  SyncBackendRegistrar* registrar() { return registrar_.get(); }
  scoped_refptr<base::SequencedTaskRunner> db_task_runner() {
    return db_thread_.task_runner();
  }

 private:
  scoped_refptr<ModelSafeWorker> CreateModelWorkerForGroup(
      ModelSafeGroup group) {
    switch (group) {
      case GROUP_PASSIVE:
        return new PassiveModelWorker();
      default:
        return nullptr;
    }
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::Thread db_thread_;
  base::Thread sync_thread_;

  std::unique_ptr<SyncBackendRegistrar> registrar_;
};

TEST_F(SyncBackendRegistrarTest, ConstructorEmpty) {
  registrar()->SetInitialTypes(ModelTypeSet());
  EXPECT_FALSE(registrar()->IsNigoriEnabled());
  EXPECT_EQ(1u, GetWorkersSize());
  ExpectRoutingInfo(ModelSafeRoutingInfo());
}

TEST_F(SyncBackendRegistrarTest, ConstructorNonEmpty) {
  registrar()->RegisterDataType(BOOKMARKS);
  registrar()->SetInitialTypes(ModelTypeSet(BOOKMARKS, NIGORI));
  EXPECT_TRUE(registrar()->IsNigoriEnabled());
  EXPECT_EQ(1u, GetWorkersSize());
  EXPECT_EQ(ModelTypeSet(NIGORI), registrar()->GetLastConfiguredTypes());
  // Bookmarks dropped because it is in ModelSafeGroup::GROUP_NON_BLOCKING.
  // Passwords dropped because of no password store.
  ExpectRoutingInfo({{NIGORI, GROUP_PASSIVE}});
}

TEST_F(SyncBackendRegistrarTest, ConstructorNonEmptyReversedInitialization) {
  registrar()->SetInitialTypes(ModelTypeSet(BOOKMARKS, NIGORI));
  registrar()->RegisterDataType(BOOKMARKS);
  EXPECT_TRUE(registrar()->IsNigoriEnabled());
  EXPECT_EQ(1u, GetWorkersSize());
  EXPECT_EQ(ModelTypeSet(NIGORI), registrar()->GetLastConfiguredTypes());
  // Bookmarks dropped because it is in ModelSafeGroup::GROUP_NON_BLOCKING.
  // Passwords dropped because of no password store.
  ExpectRoutingInfo({{NIGORI, GROUP_PASSIVE}});
}

TEST_F(SyncBackendRegistrarTest, ConfigureDataTypes) {
  registrar()->RegisterDataType(BOOKMARKS);
  registrar()->SetInitialTypes(ModelTypeSet());

  // Add.
  const ModelTypeSet types1(BOOKMARKS, NIGORI, AUTOFILL);
  EXPECT_EQ(types1, registrar()->ConfigureDataTypes(types1, ModelTypeSet()));
  ExpectRoutingInfo({{BOOKMARKS, GROUP_NON_BLOCKING},
                     {NIGORI, GROUP_PASSIVE},
                     {AUTOFILL, GROUP_PASSIVE}});
  EXPECT_EQ(types1, registrar()->GetLastConfiguredTypes());

  // Add and remove.
  const ModelTypeSet types2(PREFERENCES, THEMES);
  EXPECT_EQ(types2, registrar()->ConfigureDataTypes(types2, types1));

  ExpectRoutingInfo({{PREFERENCES, GROUP_PASSIVE}, {THEMES, GROUP_PASSIVE}});
  EXPECT_EQ(types2, registrar()->GetLastConfiguredTypes());

  // Remove.
  EXPECT_TRUE(registrar()->ConfigureDataTypes(ModelTypeSet(), types2).Empty());
  ExpectRoutingInfo(ModelSafeRoutingInfo());
  EXPECT_EQ(ModelTypeSet(), registrar()->GetLastConfiguredTypes());
}

// Tests that registration and configuration of sync data types is
// handled correctly in SyncBackendRegistrar.
TEST_F(SyncBackendRegistrarTest, ConfigureDataType) {
  registrar()->RegisterDataType(AUTOFILL);
  registrar()->RegisterDataType(BOOKMARKS);

  ExpectRoutingInfo(ModelSafeRoutingInfo());
  // Simulate that initial sync was already done for AUTOFILL.
  registrar()->AddRestoredDataType(AUTOFILL);
  // It should be added to routing info and set of configured types.
  EXPECT_EQ(ModelTypeSet(AUTOFILL), registrar()->GetLastConfiguredTypes());
  ExpectRoutingInfo({{AUTOFILL, GROUP_NON_BLOCKING}});

  // Configure two data types. Initial sync wasn't done for BOOKMARKS so
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
