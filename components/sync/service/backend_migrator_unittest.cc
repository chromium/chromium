// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/backend_migrator.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/sync/test/data_type_manager_mock.h"
#include "components/sync/test/data_type_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;

namespace syncer {

class SyncBackendMigratorTest : public testing::Test {
 public:
  SyncBackendMigratorTest() = default;
  ~SyncBackendMigratorTest() override = default;

  void SetUp() override {
    Mock::VerifyAndClear(manager());
    preferred_types_.Put(BOOKMARKS);
    preferred_types_.Put(PREFERENCES);
    preferred_types_.Put(AUTOFILL);

    migrator_ = std::make_unique<BackendMigrator>(
        "Profile0", manager(), reconfigure_callback()->Get(),
        migration_done_callback()->Get());
    SetUnsyncedTypes(DataTypeSet());
  }

  void TearDown() override { migrator_.reset(); }

  // Marks all types in |unsynced_types| as unsynced  and all other
  // types as synced.
  void SetUnsyncedTypes(DataTypeSet unsynced_types) {
    ON_CALL(manager_, GetStoppedDataTypesExcludingNigori())
        .WillByDefault(Return(unsynced_types));
  }

  void SendConfigureDone(DataTypeManager::ConfigureStatus status,
                         DataTypeSet requested_types) {
    if (status == DataTypeManager::OK) {
      DataTypeManager::ConfigureResult result(status, requested_types);
      migrator_->OnConfigureDone(result);
    } else {
      DataTypeManager::ConfigureResult result(status, requested_types);
      migrator_->OnConfigureDone(result);
    }
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  DataTypeManagerMock* manager() { return &manager_; }
  DataTypeSet preferred_types() { return preferred_types_; }
  base::MockCallback<base::RepeatingClosure>* reconfigure_callback() {
    return &reconfigure_callback_;
  }
  base::MockCallback<base::RepeatingClosure>* migration_done_callback() {
    return &migration_done_callback_;
  }
  BackendMigrator* migrator() { return migrator_.get(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  DataTypeSet preferred_types_;
  NiceMock<DataTypeManagerMock> manager_;
  NiceMock<base::MockCallback<base::RepeatingClosure>> reconfigure_callback_;
  NiceMock<base::MockCallback<base::RepeatingClosure>> migration_done_callback_;
  std::unique_ptr<BackendMigrator> migrator_;
};

class MockMigrationObserver : public MigrationObserver {
 public:
  ~MockMigrationObserver() override = default;

  MOCK_METHOD(void, OnMigrationStateChange, ());
};

// Test that in the normal case a migration does transition through each state
// and wind up back in IDLE.
TEST_F(SyncBackendMigratorTest, Sanity) {
  EXPECT_CALL(*migration_done_callback(), Run()).Times(0);

  MockMigrationObserver migration_observer;
  migrator()->AddMigrationObserver(&migration_observer);
  EXPECT_CALL(migration_observer, OnMigrationStateChange()).Times(4);

  DataTypeSet to_migrate, difference;
  to_migrate.Put(PREFERENCES);
  difference.Put(AUTOFILL);
  difference.Put(BOOKMARKS);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*manager(), PurgeForMigration);
  EXPECT_CALL(*reconfigure_callback(), Run());

  migrator()->MigrateTypes(to_migrate);
  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());

  SetUnsyncedTypes(to_migrate);
  SendConfigureDone(DataTypeManager::OK, difference);
  EXPECT_EQ(BackendMigrator::REENABLING_TYPES, migrator()->state());

  EXPECT_CALL(*migration_done_callback(), Run());
  SetUnsyncedTypes(DataTypeSet());
  SendConfigureDone(DataTypeManager::OK, preferred_types());
  EXPECT_EQ(BackendMigrator::IDLE, migrator()->state());

  migrator()->RemoveMigrationObserver(&migration_observer);
}

// Test that in the normal case with Nigori a migration transitions through
// each state and wind up back in IDLE.
TEST_F(SyncBackendMigratorTest, MigrateNigori) {
  EXPECT_CALL(*migration_done_callback(), Run()).Times(0);

  DataTypeSet to_migrate, difference;
  to_migrate.Put(NIGORI);
  difference.Put(AUTOFILL);
  difference.Put(BOOKMARKS);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));

  EXPECT_CALL(*manager(), PurgeForMigration);

  migrator()->MigrateTypes(to_migrate);
  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());

  EXPECT_CALL(*reconfigure_callback(), Run());
  SetUnsyncedTypes(to_migrate);
  SendConfigureDone(DataTypeManager::OK, difference);
  EXPECT_EQ(BackendMigrator::REENABLING_TYPES, migrator()->state());

  EXPECT_CALL(*migration_done_callback(), Run());
  SetUnsyncedTypes(DataTypeSet());
  SendConfigureDone(DataTypeManager::OK, preferred_types());
  EXPECT_EQ(BackendMigrator::IDLE, migrator()->state());
}

// Test that the migrator waits for the data type manager to be idle before
// starting a migration.
TEST_F(SyncBackendMigratorTest, WaitToStart) {
  DataTypeSet to_migrate;
  to_migrate.Put(PREFERENCES);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURING));
  EXPECT_CALL(*reconfigure_callback(), Run()).Times(0);
  migrator()->MigrateTypes(to_migrate);
  EXPECT_EQ(BackendMigrator::WAITING_TO_START, migrator()->state());

  Mock::VerifyAndClearExpectations(manager());
  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*manager(), PurgeForMigration);
  SetUnsyncedTypes(DataTypeSet());
  SendConfigureDone(DataTypeManager::OK, DataTypeSet());

  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());
}

// Test that the migrator can cope with a migration request while a migration
// is in progress.
TEST_F(SyncBackendMigratorTest, RestartMigration) {
  DataTypeSet to_migrate1, to_migrate2, to_migrate_union, bookmarks;
  to_migrate1.Put(PREFERENCES);
  to_migrate2.Put(AUTOFILL);
  to_migrate_union.Put(PREFERENCES);
  to_migrate_union.Put(AUTOFILL);
  bookmarks.Put(BOOKMARKS);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*manager(), PurgeForMigration).Times(2);
  migrator()->MigrateTypes(to_migrate1);

  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());
  migrator()->MigrateTypes(to_migrate2);

  const DataTypeSet difference1 = Difference(preferred_types(), to_migrate1);

  Mock::VerifyAndClearExpectations(manager());
  EXPECT_CALL(*manager(), PurgeForMigration);
  EXPECT_CALL(*reconfigure_callback(), Run());
  SetUnsyncedTypes(to_migrate1);
  SendConfigureDone(DataTypeManager::OK, difference1);
  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());

  SetUnsyncedTypes(to_migrate_union);
  SendConfigureDone(DataTypeManager::OK, bookmarks);
  EXPECT_EQ(BackendMigrator::REENABLING_TYPES, migrator()->state());
}

// Test that an external invocation of Configure(...) during a migration results
// in a migration reattempt.
TEST_F(SyncBackendMigratorTest, InterruptedWhileDisablingTypes) {
  DataTypeSet to_migrate;
  DataTypeSet difference;
  to_migrate.Put(PREFERENCES);
  difference.Put(AUTOFILL);
  difference.Put(BOOKMARKS);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*manager(), PurgeForMigration(HasDataTypes(to_migrate)));
  migrator()->MigrateTypes(to_migrate);
  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());

  Mock::VerifyAndClearExpectations(manager());
  EXPECT_CALL(*manager(), PurgeForMigration(HasDataTypes(to_migrate)));
  SetUnsyncedTypes(DataTypeSet());
  SendConfigureDone(DataTypeManager::OK, preferred_types());

  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());
}

// Test that spurious OnConfigureDone events don't confuse the
// migrator while it's waiting for disabled types to have been purged
// from the sync db.
TEST_F(SyncBackendMigratorTest, WaitingForPurge) {
  DataTypeSet to_migrate, difference;
  to_migrate.Put(PREFERENCES);
  to_migrate.Put(AUTOFILL);
  difference.Put(BOOKMARKS);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*manager(), PurgeForMigration);
  EXPECT_CALL(*reconfigure_callback(), Run());

  migrator()->MigrateTypes(to_migrate);
  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());

  SendConfigureDone(DataTypeManager::OK, difference);
  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());

  DataTypeSet prefs;
  prefs.Put(PREFERENCES);
  SetUnsyncedTypes(prefs);
  SendConfigureDone(DataTypeManager::OK, difference);
  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());

  SetUnsyncedTypes(to_migrate);
  SendConfigureDone(DataTypeManager::OK, difference);
  EXPECT_EQ(BackendMigrator::REENABLING_TYPES, migrator()->state());
}

TEST_F(SyncBackendMigratorTest, ConfigureFailure) {
  DataTypeSet to_migrate;
  to_migrate.Put(PREFERENCES);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*manager(), PurgeForMigration);
  migrator()->MigrateTypes(to_migrate);
  SetUnsyncedTypes(DataTypeSet());
  SendConfigureDone(DataTypeManager::ABORTED, DataTypeSet());
  EXPECT_EQ(BackendMigrator::IDLE, migrator()->state());
}

}  // namespace syncer
