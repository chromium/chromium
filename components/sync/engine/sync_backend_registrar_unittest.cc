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
    registrar_ = std::make_unique<SyncBackendRegistrar>("test");
  }

  void TearDown() override {
    sync_thread_.task_runner()->DeleteSoon(FROM_HERE, registrar_.release());
    sync_thread_.FlushForTesting();
  }

  void ExpectRoutingInfo(ModelTypeSet expected_routing_info_types) {
    EXPECT_EQ(expected_routing_info_types,
              registrar_->GetTypesWithRoutingInfo());
  }

  SyncBackendRegistrar* registrar() { return registrar_.get(); }
  scoped_refptr<base::SequencedTaskRunner> db_task_runner() {
    return db_thread_.task_runner();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::Thread db_thread_;
  base::Thread sync_thread_;
  std::unique_ptr<SyncBackendRegistrar> registrar_;
};

TEST_F(SyncBackendRegistrarTest, ConstructorEmpty) {
  registrar()->SetInitialTypes(ModelTypeSet());
  EXPECT_FALSE(registrar()->IsNigoriEnabled());
  ExpectRoutingInfo(ModelTypeSet());
}

TEST_F(SyncBackendRegistrarTest, ConstructorNonEmpty) {
  registrar()->RegisterDataType(BOOKMARKS);
  registrar()->SetInitialTypes(ModelTypeSet(BOOKMARKS, NIGORI));
  EXPECT_TRUE(registrar()->IsNigoriEnabled());
  EXPECT_EQ(ModelTypeSet(NIGORI), registrar()->GetLastConfiguredTypes());
  // Bookmarks dropped because it's nonblocking.
  // Passwords dropped because of no password store.
  ExpectRoutingInfo({NIGORI});
}

TEST_F(SyncBackendRegistrarTest, ConstructorNonEmptyReversedInitialization) {
  registrar()->SetInitialTypes(ModelTypeSet(BOOKMARKS, NIGORI));
  registrar()->RegisterDataType(BOOKMARKS);
  EXPECT_TRUE(registrar()->IsNigoriEnabled());
  EXPECT_EQ(ModelTypeSet(NIGORI), registrar()->GetLastConfiguredTypes());
  // Bookmarks dropped because it's nonblocking.
  // Passwords dropped because of no password store.
  ExpectRoutingInfo({NIGORI});
}

TEST_F(SyncBackendRegistrarTest, ConfigureDataTypes) {
  registrar()->RegisterDataType(BOOKMARKS);
  registrar()->SetInitialTypes(ModelTypeSet());

  // Add.
  const ModelTypeSet types1(BOOKMARKS, NIGORI, AUTOFILL);
  EXPECT_EQ(types1, registrar()->ConfigureDataTypes(types1, ModelTypeSet()));
  ExpectRoutingInfo({BOOKMARKS, NIGORI, AUTOFILL});
  EXPECT_EQ(types1, registrar()->GetLastConfiguredTypes());

  // Add and remove.
  const ModelTypeSet types2(PREFERENCES, THEMES);
  EXPECT_EQ(types2, registrar()->ConfigureDataTypes(types2, types1));

  ExpectRoutingInfo({PREFERENCES, THEMES});
  EXPECT_EQ(types2, registrar()->GetLastConfiguredTypes());

  // Remove.
  EXPECT_TRUE(registrar()->ConfigureDataTypes(ModelTypeSet(), types2).Empty());
  ExpectRoutingInfo(ModelTypeSet());
  EXPECT_EQ(ModelTypeSet(), registrar()->GetLastConfiguredTypes());
}

// Tests that registration and configuration of sync data types is
// handled correctly in SyncBackendRegistrar.
TEST_F(SyncBackendRegistrarTest, ConfigureDataType) {
  registrar()->RegisterDataType(AUTOFILL);
  registrar()->RegisterDataType(BOOKMARKS);

  ExpectRoutingInfo(ModelTypeSet());
  // Simulate that initial sync was already done for AUTOFILL.
  registrar()->AddRestoredDataType(AUTOFILL);
  // It should be added to routing info and set of configured types.
  EXPECT_EQ(ModelTypeSet(AUTOFILL), registrar()->GetLastConfiguredTypes());
  ExpectRoutingInfo({AUTOFILL});

  // Configure two data types. Initial sync wasn't done for BOOKMARKS so
  // it should be included in types to be downloaded.
  ModelTypeSet types_to_add(AUTOFILL, BOOKMARKS);
  ModelTypeSet newly_added_types =
      registrar()->ConfigureDataTypes(types_to_add, ModelTypeSet());
  EXPECT_EQ(ModelTypeSet(BOOKMARKS), newly_added_types);
  EXPECT_EQ(types_to_add, registrar()->GetLastConfiguredTypes());
  ExpectRoutingInfo({AUTOFILL, BOOKMARKS});
}

}  // namespace

}  // namespace syncer
