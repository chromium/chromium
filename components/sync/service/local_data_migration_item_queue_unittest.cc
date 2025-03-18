// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/local_data_migration_item_queue.h"

#include "base/test/simple_test_clock.h"
#include "components/signin/public/base/consent_level.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/data_type_manager.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync/test/data_type_manager_mock.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class LocalDataMigrationItemQueueTest : public testing::Test {
 protected:
  LocalDataMigrationItemQueueTest()
      : local_data_migration_item_queue_(
            std::make_unique<LocalDataMigrationItemQueue>(sync_service(),
                                                          data_type_manager())),
        item_({{PASSWORDS, {"d0"}}}) {
    sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);

    sync_service()->GetUserSettings()->SetSelectedTypes(
        false, {UserSelectableType::kPasswords, UserSelectableType::kAutofill});
    ON_CALL(data_type_manager_, state())
        .WillByDefault(testing::Return(DataTypeManager::State::CONFIGURED));
  }

  TestSyncService* sync_service() { return &sync_service_; }
  DataTypeManagerMock* data_type_manager() { return &data_type_manager_; }

  std::map<syncer::DataType, std::vector<syncer::LocalDataItemModel::DataId>>
  item() {
    return item_;
  }

  std::vector<LocalDataItemModel::DataId> ids() { return item_[PASSWORDS]; }

  LocalDataMigrationItemQueue* queue() {
    return local_data_migration_item_queue_.get();
  }

 private:
  TestSyncService sync_service_;
  testing::NiceMock<DataTypeManagerMock> data_type_manager_;
  std::unique_ptr<LocalDataMigrationItemQueue> local_data_migration_item_queue_;
  std::map<syncer::DataType, std::vector<syncer::LocalDataItemModel::DataId>>
      item_;
};

TEST_F(LocalDataMigrationItemQueueTest, MoveWithSyncServiceActive) {
  EXPECT_CALL(*data_type_manager(), TriggerLocalDataMigrationForItems(item()));

  queue()->TriggerLocalDataMigrationForItemsWhenTypeBecomesActive(PASSWORDS,
                                                                  ids());
}

TEST_F(LocalDataMigrationItemQueueTest, MoveAfterSyncServiceActivates) {
  sync_service()->SetMaxTransportState(
      SyncService::TransportState::CONFIGURING);

  EXPECT_CALL(*data_type_manager(), TriggerLocalDataMigrationForItems(item()))
      .Times(0);
  queue()->TriggerLocalDataMigrationForItemsWhenTypeBecomesActive(PASSWORDS,
                                                                  ids());

  EXPECT_CALL(*data_type_manager(), TriggerLocalDataMigrationForItems(item()));
  sync_service()->SetMaxTransportState(SyncService::TransportState::ACTIVE);
  sync_service()->FireStateChanged();
  EXPECT_EQ(0u, queue()->GetItemsCountForTesting());
}

TEST_F(LocalDataMigrationItemQueueTest, MoveAfterDataTypeActivates) {
  sync_service()->SetFailedDataTypes({PASSWORDS});

  EXPECT_CALL(*data_type_manager(), TriggerLocalDataMigrationForItems(item()))
      .Times(0);
  queue()->TriggerLocalDataMigrationForItemsWhenTypeBecomesActive(PASSWORDS,
                                                                  ids());

  EXPECT_CALL(*data_type_manager(), TriggerLocalDataMigrationForItems(item()));
  sync_service()->SetFailedDataTypes({});
  sync_service()->FireStateChanged();
  EXPECT_EQ(0u, queue()->GetItemsCountForTesting());
}

TEST_F(LocalDataMigrationItemQueueTest, MoveAfterUserEnteredPassphrase) {
  const std::string kTestPassphrase = "TestPassphrase";
  sync_service()->GetUserSettings()->SetPassphraseRequired(kTestPassphrase);

  ASSERT_TRUE(sync_service()
                  ->GetUserSettings()
                  ->IsPassphraseRequiredForPreferredDataTypes());

  EXPECT_CALL(*data_type_manager(), TriggerLocalDataMigrationForItems(item()))
      .Times(0);
  queue()->TriggerLocalDataMigrationForItemsWhenTypeBecomesActive(PASSWORDS,
                                                                  ids());

  EXPECT_CALL(*data_type_manager(), TriggerLocalDataMigrationForItems(item()));

  sync_service()->GetUserSettings()->SetDecryptionPassphrase(kTestPassphrase);
  ASSERT_FALSE(sync_service()
                   ->GetUserSettings()
                   ->IsPassphraseRequiredForPreferredDataTypes());

  sync_service()->FireStateChanged();
  EXPECT_EQ(0u, queue()->GetItemsCountForTesting());
}

TEST_F(LocalDataMigrationItemQueueTest, MoveMultipleItems) {
  sync_service()->SetMaxTransportState(
      SyncService::TransportState::CONFIGURING);
  std::map<syncer::DataType, std::vector<syncer::LocalDataItemModel::DataId>>
      items{{PASSWORDS, {"d1", "d3"}}, {CONTACT_INFO, {"d2"}}};

  queue()->TriggerLocalDataMigrationForItemsWhenTypeBecomesActive(
      DataType::PASSWORDS, items[PASSWORDS]);
  queue()->TriggerLocalDataMigrationForItemsWhenTypeBecomesActive(
      DataType::CONTACT_INFO, items[CONTACT_INFO]);

  EXPECT_CALL(*data_type_manager(), TriggerLocalDataMigrationForItems(items));
  sync_service()->SetMaxTransportState(SyncService::TransportState::ACTIVE);
  sync_service()->FireStateChanged();
  EXPECT_EQ(0u, queue()->GetItemsCountForTesting());
}

TEST_F(LocalDataMigrationItemQueueTest, MoveItemsOfOnlyActiveDataType) {
  sync_service()->SetFailedDataTypes({PASSWORDS, CONTACT_INFO});

  std::map<syncer::DataType, std::vector<syncer::LocalDataItemModel::DataId>>
      address_item{{CONTACT_INFO, {"d1"}}};

  queue()->TriggerLocalDataMigrationForItemsWhenTypeBecomesActive(
      DataType::PASSWORDS, ids());
  queue()->TriggerLocalDataMigrationForItemsWhenTypeBecomesActive(
      DataType::CONTACT_INFO, address_item[CONTACT_INFO]);

  EXPECT_CALL(*data_type_manager(), TriggerLocalDataMigrationForItems(item()))
      .Times(0);
  EXPECT_CALL(*data_type_manager(),
              TriggerLocalDataMigrationForItems(address_item));
  sync_service()->SetFailedDataTypes({PASSWORDS});

  ASSERT_EQ(2u, queue()->GetItemsCountForTesting());
  sync_service()->FireStateChanged();
  EXPECT_EQ(1u, queue()->GetItemsCountForTesting());
}

TEST_F(LocalDataMigrationItemQueueTest, DoNotMoveNonPreferredType) {
  // If the type is not in `GetPreferredDataTypes()` when the data migration is
  // triggered, the data will never be moved, even if the type is added later.
  sync_service()->GetUserSettings()->SetSelectedType(
      UserSelectableType::kPasswords, false);

  queue()->TriggerLocalDataMigrationForItemsWhenTypeBecomesActive(PASSWORDS,
                                                                  ids());

  EXPECT_CALL(*data_type_manager(), TriggerLocalDataMigrationForItems(item()))
      .Times(0);
  sync_service()->GetUserSettings()->SetSelectedType(
      UserSelectableType::kPasswords, true);
  sync_service()->FireStateChanged();
  EXPECT_EQ(0u, queue()->GetItemsCountForTesting());
}

TEST_F(LocalDataMigrationItemQueueTest, DoNotMoveAfterTimeLimitExceeded) {
  sync_service()->SetMaxTransportState(
      SyncService::TransportState::CONFIGURING);

  base::SimpleTestClock clock;
  queue()->SetClockForTesting(&clock);
  queue()->TriggerLocalDataMigrationForItemsWhenTypeBecomesActive(PASSWORDS,
                                                                  ids());

  clock.Advance(base::Hours(1));

  EXPECT_CALL(*data_type_manager(), TriggerLocalDataMigrationForItems(item()))
      .Times(0);
  sync_service()->SetMaxTransportState(SyncService::TransportState::ACTIVE);

  ASSERT_EQ(1u, queue()->GetItemsCountForTesting());
  sync_service()->FireStateChanged();
  EXPECT_EQ(0u, queue()->GetItemsCountForTesting());
}
TEST_F(LocalDataMigrationItemQueueTest, DoNotMoveAfterSyncServiceDisabled) {
  sync_service()->SetMaxTransportState(
      SyncService::TransportState::CONFIGURING);

  queue()->TriggerLocalDataMigrationForItemsWhenTypeBecomesActive(PASSWORDS,
                                                                  ids());

  // Disable sync service.
  sync_service()->SetSignedOut();

  EXPECT_CALL(*data_type_manager(), TriggerLocalDataMigrationForItems(item()))
      .Times(0);
  sync_service()->SetMaxTransportState(SyncService::TransportState::ACTIVE);

  ASSERT_EQ(1u, queue()->GetItemsCountForTesting());
  sync_service()->FireStateChanged();
  EXPECT_EQ(0u, queue()->GetItemsCountForTesting());
}

TEST_F(LocalDataMigrationItemQueueTest, DoNotMoveAfterSyncServicePaused) {
  sync_service()->SetMaxTransportState(
      SyncService::TransportState::CONFIGURING);

  queue()->TriggerLocalDataMigrationForItemsWhenTypeBecomesActive(PASSWORDS,
                                                                  ids());

  // Pause sync service.
  sync_service()->SetPersistentAuthError();

  EXPECT_CALL(*data_type_manager(), TriggerLocalDataMigrationForItems(item()))
      .Times(0);
  sync_service()->SetMaxTransportState(SyncService::TransportState::ACTIVE);

  ASSERT_EQ(1u, queue()->GetItemsCountForTesting());
  sync_service()->FireStateChanged();
  EXPECT_EQ(0u, queue()->GetItemsCountForTesting());
}

TEST_F(LocalDataMigrationItemQueueTest, DoNotMoveAfterConsentToSync) {
  sync_service()->SetMaxTransportState(
      SyncService::TransportState::CONFIGURING);

  queue()->TriggerLocalDataMigrationForItemsWhenTypeBecomesActive(PASSWORDS,
                                                                  ids());
  sync_service()->SetSignedIn(signin::ConsentLevel::kSync);

  EXPECT_CALL(*data_type_manager(), TriggerLocalDataMigrationForItems(item()))
      .Times(0);
  sync_service()->SetMaxTransportState(SyncService::TransportState::ACTIVE);

  ASSERT_EQ(1u, queue()->GetItemsCountForTesting());
  sync_service()->FireStateChanged();
  EXPECT_EQ(0u, queue()->GetItemsCountForTesting());
}

}  // namespace syncer
