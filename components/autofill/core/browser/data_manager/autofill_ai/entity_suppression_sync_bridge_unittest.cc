// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_sync_bridge.h"

#include <array>
#include <memory>
#include <string>
#include <utility>

#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_entry.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/webdata/personal_context/entity_suppression_sync_util.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/protocol/autofill_entity_suppression_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "crypto/hash.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

AttributeValueHash CreateTestHash(uint8_t fill_value) {
  std::array<uint8_t, crypto::hash::kSha256Size> hash;
  hash.fill(fill_value);
  return AttributeValueHash(hash);
}

EntitySuppressionEntry CreatePassportEntry() {
  return EntitySuppressionEntry{
      .type = EntityType(EntityTypeName::kPassport),
      .attribute_hashes = {{AttributeType(AttributeTypeName::kPassportNumber),
                            CreateTestHash(0x01)}},
  };
}

EntitySuppressionEntry CreateDriversLicenseEntry() {
  return EntitySuppressionEntry{
      .type = EntityType(EntityTypeName::kDriversLicense),
      .attribute_hashes = {{AttributeType(
                                AttributeTypeName::kDriversLicenseNumber),
                            CreateTestHash(0x02)}},
  };
}

std::unique_ptr<syncer::EntityChange> CreateAddChange(
    const std::string& guid,
    const EntitySuppressionEntry& entry) {
  return syncer::EntityChange::CreateAdd(
      guid,
      std::move(*CreateEntityDataFromEntitySuppressionEntry(guid, entry)));
}

std::unique_ptr<syncer::EntityChange> CreateUpdateChange(
    const std::string& guid,
    const EntitySuppressionEntry& entry) {
  return syncer::EntityChange::CreateUpdate(
      guid,
      std::move(*CreateEntityDataFromEntitySuppressionEntry(guid, entry)));
}

class MockBridgeObserver : public EntitySuppressionSyncBridge::Observer {
 public:
  MOCK_METHOD(void, OnSuppressionsChanged, (), (override));
};

class EntitySuppressionSyncBridgeTest : public testing::Test {
 public:
  EntitySuppressionSyncBridgeTest()
      : encryptor_(os_crypt_async::GetTestEncryptorForTesting()),
        store_(syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {}

  void SetUp() override {
    ON_CALL(mock_processor_, IsTrackingMetadata()).WillByDefault(Return(true));
  }

  void CreateBridge() {
    bridge_ = std::make_unique<EntitySuppressionSyncBridge>(
        mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        encryptor_);
    bridge_->AddObserver(&observer_);
    ASSERT_TRUE(base::test::RunUntil([&]() { return bridge_->IsLoaded(); }));
  }

  std::string GetFirstGuid() {
    std::unique_ptr<syncer::DataBatch> batch =
        bridge().GetAllDataForDebugging();
    EXPECT_TRUE(batch && batch->HasNext());
    return (batch && batch->HasNext()) ? batch->Next().first : "";
  }

  void WriteToStore(const std::string& guid,
                    const EntitySuppressionEntry& entry) {
    sync_pb::AutofillEntitySuppressionSpecifics specifics =
        CreateSpecificsFromEntitySuppressionEntry(guid, entry);
    std::string encrypted_blob;
    ASSERT_TRUE(encryptor().EncryptString(specifics.SerializeAsString(),
                                          &encrypted_blob));

    std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
        store_->CreateWriteBatch();
    batch->WriteData(guid, encrypted_blob);
    base::test::TestFuture<const std::optional<syncer::ModelError>&> future;
    store_->CommitWriteBatch(std::move(batch), future.GetCallback());
    EXPECT_FALSE(future.Get().has_value());
  }

  syncer::DataTypeStore::RecordList ReadAllRecordsFromStore() {
    base::test::TestFuture<const std::optional<syncer::ModelError>&,
                           std::unique_ptr<syncer::DataTypeStore::RecordList>>
        future;
    store_->ReadAllData(future.GetCallback());
    EXPECT_FALSE(future.Get<0>().has_value());
    auto records = std::get<1>(future.Take());
    return records ? std::move(*records) : syncer::DataTypeStore::RecordList();
  }

  EntitySuppressionSyncBridge& bridge() { return *bridge_; }
  NiceMock<MockBridgeObserver>& observer() { return observer_; }
  const os_crypt_async::Encryptor& encryptor() { return *encryptor_; }
  NiceMock<syncer::MockDataTypeLocalChangeProcessor>& mock_processor() {
    return mock_processor_;
  }
  std::unique_ptr<syncer::DataTypeStore>& store() { return store_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<const os_crypt_async::Encryptor> encryptor_;
  std::unique_ptr<syncer::DataTypeStore> store_;
  NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  NiceMock<MockBridgeObserver> observer_;
  std::unique_ptr<EntitySuppressionSyncBridge> bridge_;
};

// Tests that when the encryptor has no usable key, the bridge does not
// initialize the store or report ready to sync.
TEST_F(EntitySuppressionSyncBridgeTest, EncryptionUnavailableDoesNotLoad) {
  EXPECT_CALL(mock_processor(), ModelReadyToSync).Times(0);

  EntitySuppressionSyncBridge bridge(
      mock_processor().CreateForwardingProcessor(),
      syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store().get()),
      os_crypt_async::GetTestEncryptorWithoutKeysForTesting());

  EXPECT_FALSE(bridge.IsLoaded());
}

// Tests that pre-existing encrypted records in the store are loaded and
// decrypted upon bridge creation.
TEST_F(EntitySuppressionSyncBridgeTest, InitialLoadFromStore) {
  EntitySuppressionEntry entry = CreatePassportEntry();
  WriteToStore("guid1", entry);

  EXPECT_CALL(observer(), OnSuppressionsChanged());

  CreateBridge();

  EXPECT_TRUE(bridge().IsLoaded());
  EXPECT_EQ(GetFirstGuid(), "guid1");
  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(entry));
}

// Tests that suppression entries written to the store are encrypted.
TEST_F(EntitySuppressionSyncBridgeTest, DataIsEncryptedInStore) {
  CreateBridge();
  EntitySuppressionEntry entry = CreatePassportEntry();
  ASSERT_TRUE(bridge().Suppress(entry));

  syncer::DataTypeStore::RecordList records = ReadAllRecordsFromStore();
  ASSERT_EQ(records.size(), 1u);

  // Decrypting must recover the original suppression entry.
  std::string decrypted_plaintext;
  EXPECT_TRUE(
      encryptor().DecryptString(records[0].value, &decrypted_plaintext));
  EXPECT_NE(records[0].value, decrypted_plaintext);
  sync_pb::AutofillEntitySuppressionSpecifics specifics;
  ASSERT_TRUE(specifics.ParseFromString(decrypted_plaintext));
  EXPECT_EQ(CreateEntitySuppressionEntryFromSpecifics(specifics), entry);
}

// Tests that locally suppressing an entry adds it to local suppressions,
// commits it to Sync, and notifies observers.
TEST_F(EntitySuppressionSyncBridgeTest, LocalSuppress) {
  CreateBridge();
  EntitySuppressionEntry entry = CreatePassportEntry();
  ASSERT_TRUE(bridge().GetSuppressions().empty());
  EXPECT_FALSE(bridge().IsSuppressed(entry));

  EXPECT_CALL(mock_processor(), Put);
  EXPECT_CALL(observer(), OnSuppressionsChanged());
  EXPECT_TRUE(bridge().Suppress(entry));

  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(entry));
  EXPECT_TRUE(bridge().IsSuppressed(entry));
}

// Tests that unsuppressing an entry deletes it from local state, commits
// the deletion to Sync, and notifies observers.
TEST_F(EntitySuppressionSyncBridgeTest, LocalSuppressAndUnsuppress) {
  CreateBridge();
  EntitySuppressionEntry entry = CreatePassportEntry();
  ASSERT_TRUE(bridge().Suppress(entry));
  ASSERT_THAT(bridge().GetSuppressions(), ElementsAre(entry));
  EXPECT_TRUE(bridge().IsSuppressed(entry));
  std::string guid = GetFirstGuid();

  EXPECT_CALL(mock_processor(), Delete(guid, _, _));
  EXPECT_CALL(observer(), OnSuppressionsChanged());
  EXPECT_TRUE(bridge().Unsuppress(entry));

  EXPECT_TRUE(bridge().GetSuppressions().empty());
  EXPECT_FALSE(bridge().IsSuppressed(entry));
}

// Tests that unsuppressing a non-existing entry returns `false` without
// calling Sync or notifying observers.
TEST_F(EntitySuppressionSyncBridgeTest, LocalUnsuppressNonExistingEntry) {
  CreateBridge();

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(0);
  EXPECT_FALSE(bridge().Unsuppress(CreatePassportEntry()));
}

// Tests that incremental remote additions update local suppressions and
// notify observers.
TEST_F(EntitySuppressionSyncBridgeTest, ApplyIncrementalSyncChanges_Add) {
  CreateBridge();
  EntitySuppressionEntry entry = CreatePassportEntry();
  ASSERT_TRUE(bridge().GetSuppressions().empty());
  syncer::EntityChangeList add_changes;
  add_changes.push_back(CreateAddChange("guid1", entry));

  EXPECT_CALL(observer(), OnSuppressionsChanged());
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(add_changes));

  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(entry));
}

// Tests that incremental remote deletions remove suppressions locally and
// notify observers.
TEST_F(EntitySuppressionSyncBridgeTest, ApplyIncrementalSyncChanges_Delete) {
  CreateBridge();
  EntitySuppressionEntry entry1 = CreatePassportEntry();
  EntitySuppressionEntry entry2 = CreateDriversLicenseEntry();
  syncer::EntityChangeList add_changes;
  add_changes.push_back(CreateAddChange("guid1", entry1));
  add_changes.push_back(CreateAddChange("guid2", entry2));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(add_changes));
  ASSERT_THAT(bridge().GetSuppressions(), UnorderedElementsAre(entry1, entry2));

  syncer::EntityChangeList delete_changes;
  delete_changes.push_back(
      syncer::EntityChange::CreateDelete("guid1", syncer::EntityData()));
  EXPECT_CALL(observer(), OnSuppressionsChanged());
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(delete_changes));

  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(entry2));
}

// Tests that disabling sync clears all local suppressions and notifies
// observers.
TEST_F(EntitySuppressionSyncBridgeTest, ApplyDisableSyncChanges) {
  CreateBridge();
  EntitySuppressionEntry entry = CreatePassportEntry();
  EXPECT_TRUE(bridge().Suppress(entry));
  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(entry));

  EXPECT_CALL(observer(), OnSuppressionsChanged());
  bridge().ApplyDisableSyncChanges(bridge().CreateMetadataChangeList());

  EXPECT_TRUE(bridge().GetSuppressions().empty());
}

// Tests that local-only entries created prior to initial sync are uploaded
// to Sync during `MergeFullSyncData`.
TEST_F(EntitySuppressionSyncBridgeTest,
       MergeFullSyncData_UploadsLocalOnlyEntries) {
  EntitySuppressionEntry local_entry = CreatePassportEntry();
  WriteToStore("local_guid", local_entry);
  CreateBridge();
  ASSERT_THAT(bridge().GetSuppressions(), ElementsAre(local_entry));

  // When initial sync occurs (`MergeFullSyncData`), the pre-existing local
  // entry must be uploaded to Sync via `Put()`.
  EXPECT_CALL(mock_processor(), Put("local_guid", _, _));
  bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                             syncer::EntityChangeList());
}

// Tests that `MergeFullSyncData` combines distinct local and remote entries
// and notifies observers.
TEST_F(EntitySuppressionSyncBridgeTest,
       MergeFullSyncData_RemoteAndLocalEntries) {
  EntitySuppressionEntry local_entry = CreatePassportEntry();
  WriteToStore("local_guid", local_entry);
  CreateBridge();
  ASSERT_THAT(bridge().GetSuppressions(), ElementsAre(local_entry));

  // Observer should be notified of the remote entry addition.
  EXPECT_CALL(observer(), OnSuppressionsChanged());
  EntitySuppressionEntry remote_entry = CreateDriversLicenseEntry();
  syncer::EntityChangeList remote_changes;
  remote_changes.push_back(CreateAddChange("guid2", remote_entry));
  bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                             std::move(remote_changes));

  EXPECT_THAT(bridge().GetSuppressions(),
              UnorderedElementsAre(local_entry, remote_entry));
}

// Tests that attempting to suppress an already suppressed entry returns
// `false` and performs no operations.
TEST_F(EntitySuppressionSyncBridgeTest, Suppress_DuplicateEntryReturnsFalse) {
  CreateBridge();
  EntitySuppressionEntry entry = CreatePassportEntry();
  ASSERT_TRUE(bridge().Suppress(entry));

  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(0);
  EXPECT_FALSE(bridge().Suppress(entry));
}

// Tests that Suppress returns false without modifying local state or notifying
// observers when Sync is not tracking metadata.
TEST_F(EntitySuppressionSyncBridgeTest,
       Suppress_ReturnsFalseWhenSyncNotTrackingMetadata) {
  CreateBridge();
  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(false));

  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(0);
  EXPECT_FALSE(bridge().Suppress(CreatePassportEntry()));
  EXPECT_TRUE(bridge().GetSuppressions().empty());
}

// Tests that Unsuppress returns false without deleting local state or notifying
// observers when Sync is not tracking metadata.
TEST_F(EntitySuppressionSyncBridgeTest,
       Unsuppress_ReturnsFalseWhenSyncNotTrackingMetadata) {
  CreateBridge();
  EntitySuppressionEntry entry = CreatePassportEntry();
  ASSERT_TRUE(bridge().Suppress(entry));
  ASSERT_THAT(bridge().GetSuppressions(), ElementsAre(entry));

  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(false));

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(0);
  EXPECT_FALSE(bridge().Unsuppress(entry));
  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(entry));
}

// Tests that `MergeFullSyncData` deduplicates matching local and remote
// entries deterministically using lexicographical tie-breaking without
// redundant uploads.
TEST_F(EntitySuppressionSyncBridgeTest,
       MergeFullSyncData_DeduplicatesMatchingEntriesWithDifferentGuids) {
  EntitySuppressionEntry entry = CreatePassportEntry();
  WriteToStore("guid2", entry);
  CreateBridge();
  ASSERT_THAT(bridge().GetSuppressions(), ElementsAre(entry));

  // When remote GUID ("guid1") is smaller than local GUID ("guid2"), remote
  // wins. Local duplicate is deleted from Sync; remote is not re-uploaded, and
  // suppression observers are not notified since the entity already exists.
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(mock_processor(), Delete("guid2", _, _));
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(0);
  syncer::EntityChangeList remote_changes;
  remote_changes.push_back(CreateAddChange("guid1", entry));
  bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                             std::move(remote_changes));

  EXPECT_EQ(GetFirstGuid(), "guid1");
  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(entry));
}

// Tests that when the local GUID is smaller than the remote duplicate GUID in
// `MergeFullSyncData`, the local GUID wins and is uploaded to Sync.
TEST_F(EntitySuppressionSyncBridgeTest,
       MergeFullSyncData_LocalWinsWhenSmallerGuid) {
  EntitySuppressionEntry entry = CreatePassportEntry();
  WriteToStore("guid1", entry);
  CreateBridge();
  ASSERT_THAT(bridge().GetSuppressions(), ElementsAre(entry));

  // When local GUID ("guid1") is smaller than remote GUID ("guid2"), local
  // wins. Remote duplicate is deleted from Sync, and the winning local entry is
  // uploaded to Sync.
  EXPECT_CALL(mock_processor(), Delete("guid2", _, _));
  EXPECT_CALL(mock_processor(), Put("guid1", _, _));
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(0);
  syncer::EntityChangeList remote_changes;
  remote_changes.push_back(CreateAddChange("guid2", entry));
  bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                             std::move(remote_changes));

  EXPECT_EQ(GetFirstGuid(), "guid1");
  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(entry));
}

// Tests that duplicate entries within the remote initial sync payload are
// deduplicated deterministically.
TEST_F(EntitySuppressionSyncBridgeTest,
       MergeFullSyncData_RemotePayloadContainsDuplicates) {
  CreateBridge();
  EntitySuppressionEntry entry = CreatePassportEntry();
  syncer::EntityChangeList remote_changes;
  remote_changes.push_back(CreateAddChange("guid1", entry));
  remote_changes.push_back(CreateAddChange("guid2", entry));

  EXPECT_CALL(mock_processor(), Delete("guid2", _, _));
  EXPECT_CALL(observer(), OnSuppressionsChanged());
  bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                             std::move(remote_changes));

  EXPECT_EQ(GetFirstGuid(), "guid1");
  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(entry));
}

// Tests that incremental remote additions matching an existing local entry
// deduplicate deterministically using smaller GUID.
TEST_F(
    EntitySuppressionSyncBridgeTest,
    ApplyIncrementalSyncChanges_DeduplicatesMatchingEntriesWithDifferentGuids) {
  EntitySuppressionEntry entry = CreatePassportEntry();
  WriteToStore("guid2", entry);
  CreateBridge();
  ASSERT_THAT(bridge().GetSuppressions(), ElementsAre(entry));

  // Incoming "guid1" < existing "guid2", so incoming wins. Existing "guid2"
  // is deleted from Sync and local store.
  EXPECT_CALL(mock_processor(), Delete("guid2", _, _));
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(0);
  syncer::EntityChangeList remote_changes;
  remote_changes.push_back(CreateAddChange("guid1", entry));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(remote_changes));

  EXPECT_EQ(GetFirstGuid(), "guid1");
  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(entry));
}

// Tests that duplicate entries within an incremental update cause
// deletion of the larger duplicate in Sync.
TEST_F(EntitySuppressionSyncBridgeTest,
       ApplyIncrementalSyncChanges_RemotePayloadContainsDuplicates) {
  CreateBridge();
  EntitySuppressionEntry entry = CreatePassportEntry();
  syncer::EntityChangeList remote_changes;
  remote_changes.push_back(CreateAddChange("guid1", entry));
  remote_changes.push_back(CreateAddChange("guid2", entry));

  // "guid1" < "guid2", so "guid1" wins and "guid2" is deleted from sync.
  EXPECT_CALL(mock_processor(), Delete("guid2", _, _));
  EXPECT_CALL(observer(), OnSuppressionsChanged());
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(remote_changes));

  EXPECT_EQ(GetFirstGuid(), "guid1");
  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(entry));
}

// Tests that ACTION_UPDATE changes are ignored since entity suppressions are
// immutable.
TEST_F(EntitySuppressionSyncBridgeTest,
       ApplyIncrementalSyncChanges_IgnoresUpdates) {
  CreateBridge();
  EntitySuppressionEntry entry = CreatePassportEntry();
  syncer::EntityChangeList remote_changes;
  remote_changes.push_back(CreateAddChange("guid1", entry));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(remote_changes));
  ASSERT_THAT(bridge().GetSuppressions(), ElementsAre(entry));

  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(0);
  syncer::EntityChangeList update_changes;
  update_changes.push_back(
      CreateUpdateChange("guid1", CreateDriversLicenseEntry()));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(update_changes));

  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(entry));
  EXPECT_EQ(GetFirstGuid(), "guid1");
}

// Tests that ClearAllSuppressions deletes all entries from local state and
// store, commits deletions to Sync, and notifies observers once.
TEST_F(EntitySuppressionSyncBridgeTest, ClearAllSuppressions) {
  WriteToStore("guid1", CreatePassportEntry());
  WriteToStore("guid2", CreateDriversLicenseEntry());
  CreateBridge();
  ASSERT_EQ(bridge().GetSuppressions().size(), 2u);

  EXPECT_CALL(mock_processor(), Delete("guid1", _, _));
  EXPECT_CALL(mock_processor(), Delete("guid2", _, _));
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(1);
  EXPECT_TRUE(bridge().ClearAllSuppressions());

  EXPECT_TRUE(bridge().GetSuppressions().empty());
  EXPECT_TRUE(ReadAllRecordsFromStore().empty());
}

// Tests that ClearAllSuppressions returns false and does not notify observers
// when there are no suppressions.
TEST_F(EntitySuppressionSyncBridgeTest, ClearAllSuppressions_Empty) {
  CreateBridge();
  ASSERT_TRUE(bridge().GetSuppressions().empty());

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(0);
  EXPECT_FALSE(bridge().ClearAllSuppressions());
}

}  // namespace

}  // namespace autofill
