// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_sync_bridge.h"

#include <array>
#include <memory>
#include <string>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_entry.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/personal_context_metrics.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/browser/webdata/personal_context/entity_suppression_sync_util.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/autofill_entity_suppression_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync/test/unknown_field_util.h"
#include "crypto/hash.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::base::test::EqualsProto;
using ::syncer::test::AddUnknownFieldToProto;
using ::syncer::test::HasUnknownField;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::ElementsAre;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::UnorderedElementsAre;

// The maximum number of suppressions that the bridge keeps.
constexpr size_t kMaxSuppressions = 1000;

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

// Returns the key under which `Suppress()` stores `entry`.
EntitySuppressionKey ToKey(const EntitySuppressionEntry& entry) {
  return GetEntitySuppressionHash(entry);
}

std::unique_ptr<syncer::EntityChange> CreateAddChange(
    const std::string& guid,
    const EntitySuppressionEntry& entry) {
  return syncer::EntityChange::CreateAdd(
      guid, std::move(*CreateEntityDataFromEntitySuppressionKey(
                guid, ToKey(entry), base::Time::Now(), /*base_specifics=*/{})));
}

std::unique_ptr<syncer::EntityChange> CreateUpdateChange(
    const std::string& guid,
    const EntitySuppressionEntry& entry) {
  return syncer::EntityChange::CreateUpdate(
      guid, std::move(*CreateEntityDataFromEntitySuppressionKey(
                guid, ToKey(entry), base::Time::Now(), /*base_specifics=*/{})));
}

class MockBridgeObserver : public EntitySuppressionSyncBridge::Observer {
 public:
  MOCK_METHOD(void, OnSuppressionsChanged, (), (override));
};

class EntitySuppressionSyncBridgeTest : public testing::Test {
 public:
  EntitySuppressionSyncBridgeTest()
      : os_crypt_async_(os_crypt_async::GetTestOSCryptAsyncForTesting(
            /*is_sync_for_unittests=*/true)),
        store_(syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    base::test::TestFuture<scoped_refptr<os_crypt_async::Encryptor>> future;
    os_crypt_async_->GetInstance(future.GetCallback());
    encryptor_ = future.Take();
  }

  void SetUp() override {
    ON_CALL(mock_processor_, IsTrackingMetadata()).WillByDefault(Return(true));
    ON_CALL(mock_processor_, GetPossiblyTrimmedRemoteSpecifics)
        .WillByDefault(ReturnRef(sync_pb::EntitySpecifics::default_instance()));
  }

  void CreateBridge() {
    bridge_ = std::make_unique<EntitySuppressionSyncBridge>(
        mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        os_crypt_async_.get());
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
                    const EntitySuppressionEntry& entry,
                    base::Time creation_time = base::Time::Now()) {
    WriteEncryptedToStore(guid, CreateSpecificsFromEntitySuppressionKey(
                                    guid, ToKey(entry), creation_time,
                                    /*base_specifics=*/{})
                                    .SerializeAsString());
  }

  // Writes `count` suppressions with distinct hashes, random GUIDs and the
  // given `creation_time` to the store.
  void WriteSuppressionsWithRandomGuidToStore(size_t count,
                                              base::Time creation_time) {
    std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
        store_->CreateWriteBatch();
    for (size_t i = 0; i < count; ++i) {
      const std::string guid =
          base::Uuid::GenerateRandomV4().AsLowercaseString();
      // Hashes of random entries could collide since they are truncated.
      const EntitySuppressionHash hash(static_cast<uint32_t>(i));
      std::string encrypted_blob;
      ASSERT_TRUE(encryptor().EncryptString(
          CreateSpecificsFromEntitySuppressionKey(guid, hash, creation_time,
                                                  /*base_specifics=*/{})
              .SerializeAsString(),
          &encrypted_blob));
      batch->WriteData(guid, encrypted_blob);
    }
    CommitToStore(std::move(batch));
  }

  // Encrypts `plaintext` and writes it to the store under `guid`.
  void WriteEncryptedToStore(const std::string& guid,
                             const std::string& plaintext) {
    std::string encrypted_blob;
    ASSERT_TRUE(encryptor().EncryptString(plaintext, &encrypted_blob));
    WriteRawToStore(guid, encrypted_blob);
  }

  // Writes `value` to the store under `guid` without encrypting it.
  void WriteRawToStore(const std::string& guid, const std::string& value) {
    std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
        store_->CreateWriteBatch();
    batch->WriteData(guid, value);
    CommitToStore(std::move(batch));
  }

  void CommitToStore(std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch) {
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

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  // Writing many records makes leveldb schedule compactions on the thread pool,
  // so a `SingleThreadTaskEnvironment` isn't sufficient.
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
  scoped_refptr<os_crypt_async::Encryptor> encryptor_;
  std::unique_ptr<syncer::DataTypeStore> store_;
  NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  NiceMock<MockBridgeObserver> observer_;
  std::unique_ptr<EntitySuppressionSyncBridge> bridge_;
};

// Tests that when the encryptor has no usable key, the bridge reports an error
// instead of loading.
TEST_F(EntitySuppressionSyncBridgeTest, EncryptionUnavailableReportsError) {
  EXPECT_CALL(mock_processor(), ModelReadyToSync).Times(0);
  EXPECT_CALL(mock_processor(),
              ReportError(Property(
                  &syncer::ModelError::type,
                  syncer::ModelError::Type::
                      kAutofillEntitySuppressionEncryptionUnavailable)));

  os_crypt_async::OSCryptAsync os_crypt_async_without_keys({});
  EntitySuppressionSyncBridge bridge(
      mock_processor().CreateForwardingProcessor(),
      syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store().get()),
      &os_crypt_async_without_keys);
  task_environment().RunUntilIdle();

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
  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(entry)));
}

// Tests that suppression entries written to the store are encrypted.
TEST_F(EntitySuppressionSyncBridgeTest, DataIsEncryptedInStore) {
  CreateBridge();
  EntitySuppressionEntry entry = CreatePassportEntry();
  ASSERT_TRUE(bridge().Suppress(entry));

  syncer::DataTypeStore::RecordList records = ReadAllRecordsFromStore();
  ASSERT_EQ(records.size(), 1u);

  // Decrypting must recover the key of the suppression.
  std::string decrypted_plaintext;
  EXPECT_TRUE(
      encryptor().DecryptString(records[0].value, &decrypted_plaintext));
  EXPECT_NE(records[0].value, decrypted_plaintext);
  sync_pb::AutofillEntitySuppressionSpecifics specifics;
  ASSERT_TRUE(specifics.ParseFromString(decrypted_plaintext));
  EXPECT_EQ(CreateEntitySuppressionKeyFromSpecifics(specifics), ToKey(entry));
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

  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(entry)));
  EXPECT_TRUE(bridge().IsSuppressed(entry));
}

// Tests that unsuppressing an entry deletes it from local state, commits
// the deletion to Sync, and notifies observers.
TEST_F(EntitySuppressionSyncBridgeTest, LocalSuppressAndUnsuppress) {
  CreateBridge();
  EntitySuppressionEntry entry = CreatePassportEntry();
  ASSERT_TRUE(bridge().Suppress(entry));
  ASSERT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(entry)));
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

  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(entry)));
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
  ASSERT_THAT(bridge().GetSuppressions(),
              UnorderedElementsAre(ToKey(entry1), ToKey(entry2)));

  syncer::EntityChangeList delete_changes;
  delete_changes.push_back(
      syncer::EntityChange::CreateDelete("guid1", syncer::EntityData()));
  EXPECT_CALL(observer(), OnSuppressionsChanged());
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(delete_changes));

  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(entry2)));
}

// Tests that disabling sync clears all local suppressions and notifies
// observers.
TEST_F(EntitySuppressionSyncBridgeTest, ApplyDisableSyncChanges) {
  CreateBridge();
  EntitySuppressionEntry entry = CreatePassportEntry();
  EXPECT_TRUE(bridge().Suppress(entry));
  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(entry)));

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
  ASSERT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(local_entry)));

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
  ASSERT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(local_entry)));

  // Observer should be notified of the remote entry addition.
  EXPECT_CALL(observer(), OnSuppressionsChanged());
  EntitySuppressionEntry remote_entry = CreateDriversLicenseEntry();
  syncer::EntityChangeList remote_changes;
  remote_changes.push_back(CreateAddChange("guid2", remote_entry));
  bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                             std::move(remote_changes));

  EXPECT_THAT(bridge().GetSuppressions(),
              UnorderedElementsAre(ToKey(local_entry), ToKey(remote_entry)));
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
  ASSERT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(entry)));

  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(false));

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(0);
  EXPECT_FALSE(bridge().Unsuppress(entry));
  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(entry)));
}

// Tests that `MergeFullSyncData` deduplicates matching local and remote
// entries deterministically using lexicographical tie-breaking without
// redundant uploads.
TEST_F(EntitySuppressionSyncBridgeTest,
       MergeFullSyncData_DeduplicatesMatchingEntriesWithDifferentGuids) {
  EntitySuppressionEntry entry = CreatePassportEntry();
  WriteToStore("guid2", entry);
  CreateBridge();
  ASSERT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(entry)));

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
  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(entry)));
}

// Tests that when the local GUID is smaller than the remote duplicate GUID in
// `MergeFullSyncData`, the local GUID wins and is uploaded to Sync.
TEST_F(EntitySuppressionSyncBridgeTest,
       MergeFullSyncData_LocalWinsWhenSmallerGuid) {
  EntitySuppressionEntry entry = CreatePassportEntry();
  WriteToStore("guid1", entry);
  CreateBridge();
  ASSERT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(entry)));

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
  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(entry)));
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
  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(entry)));
}

// Tests that incremental remote additions matching an existing local entry
// deduplicate deterministically using smaller GUID.
TEST_F(
    EntitySuppressionSyncBridgeTest,
    ApplyIncrementalSyncChanges_DeduplicatesMatchingEntriesWithDifferentGuids) {
  EntitySuppressionEntry entry = CreatePassportEntry();
  WriteToStore("guid2", entry);
  CreateBridge();
  ASSERT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(entry)));

  // Incoming "guid1" < existing "guid2", so incoming wins. Existing "guid2"
  // is deleted from Sync and local store.
  EXPECT_CALL(mock_processor(), Delete("guid2", _, _));
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(0);
  syncer::EntityChangeList remote_changes;
  remote_changes.push_back(CreateAddChange("guid1", entry));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(remote_changes));

  EXPECT_EQ(GetFirstGuid(), "guid1");
  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(entry)));
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
  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(entry)));
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
  ASSERT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(entry)));

  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(0);
  syncer::EntityChangeList update_changes;
  update_changes.push_back(
      CreateUpdateChange("guid1", CreateDriversLicenseEntry()));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(update_changes));

  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(entry)));
  EXPECT_EQ(GetFirstGuid(), "guid1");
}

// Tests that ClearSuppressions with a full time range deletes all entries from
// local state and store, commits deletions to Sync, and notifies observers
// once.
TEST_F(EntitySuppressionSyncBridgeTest, ClearSuppressions) {
  WriteToStore("guid1", CreatePassportEntry());
  WriteToStore("guid2", CreateDriversLicenseEntry());
  CreateBridge();
  ASSERT_EQ(bridge().GetSuppressions().size(), 2u);

  EXPECT_CALL(mock_processor(), Delete("guid1", _, _));
  EXPECT_CALL(mock_processor(), Delete("guid2", _, _));
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(1);
  EXPECT_TRUE(bridge().ClearSuppressions(base::Time(), base::Time::Max()));

  EXPECT_TRUE(bridge().GetSuppressions().empty());
  EXPECT_TRUE(ReadAllRecordsFromStore().empty());
}

// Tests that ClearSuppressions only deletes entries whose creation time falls
// within [delete_begin, delete_end].
TEST_F(EntitySuppressionSyncBridgeTest, ClearSuppressions_TimeRange) {
  const base::Time t1 =
      base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(100));
  const base::Time t2 =
      base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(200));
  EntitySuppressionEntry entry1 = CreatePassportEntry();
  EntitySuppressionEntry entry2 = CreateDriversLicenseEntry();
  WriteToStore("guid1", entry1, t1);
  WriteToStore("guid2", entry2, t2);
  CreateBridge();
  ASSERT_EQ(bridge().GetSuppressions().size(), 2u);

  EXPECT_CALL(mock_processor(), Delete("guid1", _, _)).Times(0);
  EXPECT_CALL(mock_processor(), Delete("guid2", _, _));
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(1);
  EXPECT_TRUE(
      bridge().ClearSuppressions(t1 + base::Seconds(50), base::Time::Max()));

  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(entry1)));
  EXPECT_EQ(ReadAllRecordsFromStore().size(), 1u);
}

// Tests that ClearSuppressions returns false and does not notify observers
// when there are no matching suppressions.
TEST_F(EntitySuppressionSyncBridgeTest, ClearSuppressions_Empty) {
  CreateBridge();
  ASSERT_TRUE(bridge().GetSuppressions().empty());

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(0);
  EXPECT_FALSE(bridge().ClearSuppressions(base::Time(), base::Time::Max()));
}

// Tests that CountSuppressionsCreatedBetween only counts entries whose
// creation time falls within the inclusive range [begin, end].
TEST_F(EntitySuppressionSyncBridgeTest, CountSuppressionsCreatedBetween) {
  const base::Time t1 =
      base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(100));
  const base::Time t2 =
      base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(200));
  WriteToStore("guid1", CreatePassportEntry(), t1);
  WriteToStore("guid2", CreateDriversLicenseEntry(), t2);
  CreateBridge();
  ASSERT_EQ(bridge().GetSuppressions().size(), 2u);

  // Null bounds cover everything.
  EXPECT_EQ(
      bridge().CountSuppressionsCreatedBetween(base::Time(), base::Time::Max()),
      2u);
  EXPECT_EQ(
      bridge().CountSuppressionsCreatedBetween(base::Time(), base::Time()), 2u);
  // Bounds are inclusive.
  EXPECT_EQ(bridge().CountSuppressionsCreatedBetween(t1, t1), 1u);
  EXPECT_EQ(bridge().CountSuppressionsCreatedBetween(t2, t2), 1u);
  EXPECT_EQ(bridge().CountSuppressionsCreatedBetween(t1, t2), 2u);
  EXPECT_EQ(bridge().CountSuppressionsCreatedBetween(t1 + base::Seconds(50),
                                                     base::Time::Max()),
            1u);
  // Ranges not containing any creation time.
  EXPECT_EQ(bridge().CountSuppressionsCreatedBetween(base::Time::Min(),
                                                     t1 - base::Seconds(1)),
            0u);
  EXPECT_EQ(bridge().CountSuppressionsCreatedBetween(t1 + base::Seconds(1),
                                                     t2 - base::Seconds(1)),
            0u);
  EXPECT_EQ(bridge().CountSuppressionsCreatedBetween(t2 + base::Seconds(1),
                                                     base::Time::Max()),
            0u);

  // After clearing, nothing is left.
  ASSERT_TRUE(bridge().ClearSuppressions(base::Time(), base::Time::Max()));
  EXPECT_EQ(
      bridge().CountSuppressionsCreatedBetween(base::Time(), base::Time::Max()),
      0u);
}

// Tests that CountSuppressionsCreatedBetween returns zero when there are no
// suppressions.
TEST_F(EntitySuppressionSyncBridgeTest, CountSuppressionsCreatedBetween_Empty) {
  CreateBridge();
  ASSERT_TRUE(bridge().GetSuppressions().empty());

  EXPECT_EQ(
      bridge().CountSuppressionsCreatedBetween(base::Time(), base::Time::Max()),
      0u);
}

// Tests that suppressions above the limit, e.g. stored by builds without the
// limit, are deleted on load.
TEST_F(EntitySuppressionSyncBridgeTest,
       Load_DeletesOldestSuppressionsAboveLimit) {
  EntitySuppressionEntry oldest_entry = CreatePassportEntry();
  EntitySuppressionEntry second_oldest_entry = CreateDriversLicenseEntry();
  WriteToStore("oldest_guid", oldest_entry,
               /*creation_time=*/base::Time::UnixEpoch());
  WriteToStore("second_oldest_guid", second_oldest_entry,
               /*creation_time=*/test::kJanuary2017);
  WriteSuppressionsWithRandomGuidToStore(
      /*count=*/kMaxSuppressions,
      /*creation_time=*/test::kJune2017);

  EXPECT_CALL(mock_processor(), Delete("oldest_guid", _, _));
  EXPECT_CALL(mock_processor(), Delete("second_oldest_guid", _, _));
  CreateBridge();

  EXPECT_EQ(bridge().GetSuppressions().size(), kMaxSuppressions);
  EXPECT_FALSE(bridge().IsSuppressed(oldest_entry));
  EXPECT_FALSE(bridge().IsSuppressed(second_oldest_entry));
  EXPECT_EQ(ReadAllRecordsFromStore().size(), kMaxSuppressions);
}

// Tests that suppressing an entry at the limit deletes the oldest suppression
// from local storage and Sync.
TEST_F(EntitySuppressionSyncBridgeTest,
       Suppress_DeletesOldestSuppressionAtLimit) {
  EntitySuppressionEntry oldest_entry = CreatePassportEntry();
  WriteToStore("oldest_guid", oldest_entry,
               /*creation_time=*/base::Time::UnixEpoch());
  WriteSuppressionsWithRandomGuidToStore(
      /*count=*/kMaxSuppressions - 1,
      /*creation_time=*/test::kJune2017);
  CreateBridge();

  EntitySuppressionEntry new_entry = CreateDriversLicenseEntry();
  EXPECT_CALL(mock_processor(), Delete("oldest_guid", _, _));
  EXPECT_TRUE(bridge().Suppress(new_entry));

  EXPECT_EQ(bridge().GetSuppressions().size(), kMaxSuppressions);
  EXPECT_FALSE(bridge().IsSuppressed(oldest_entry));
  EXPECT_TRUE(bridge().IsSuppressed(new_entry));
}

// Tests that `MergeFullSyncData` deletes the oldest suppressions above the
// limit and doesn't upload deleted local-only suppressions.
TEST_F(EntitySuppressionSyncBridgeTest,
       MergeFullSyncData_DeletesOldestSuppressionsAboveLimit) {
  EntitySuppressionEntry oldest_entry = CreatePassportEntry();
  WriteToStore("oldest_guid", oldest_entry,
               /*creation_time=*/base::Time::UnixEpoch());
  WriteSuppressionsWithRandomGuidToStore(
      /*count=*/kMaxSuppressions - 1,
      /*creation_time=*/test::kJune2017);
  CreateBridge();
  EntitySuppressionEntry remote_entry = CreateDriversLicenseEntry();
  syncer::EntityChangeList remote_changes;
  remote_changes.push_back(CreateAddChange("remote_guid", remote_entry));

  EXPECT_CALL(mock_processor(), Put("oldest_guid", _, _)).Times(0);
  // The remaining local-only suppressions are uploaded.
  EXPECT_CALL(mock_processor(), Put(Ne("oldest_guid"), _, _))
      .Times(AnyNumber());
  bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                             std::move(remote_changes));

  EXPECT_EQ(bridge().GetSuppressions().size(), kMaxSuppressions);
  EXPECT_FALSE(bridge().IsSuppressed(oldest_entry));
  EXPECT_TRUE(bridge().IsSuppressed(remote_entry));
}

// Tests that remote additions above the limit, e.g. from clients without the
// limit, delete the oldest suppressions from local storage and Sync.
TEST_F(EntitySuppressionSyncBridgeTest,
       ApplyIncrementalSyncChanges_DeletesOldestSuppressionsAboveLimit) {
  EntitySuppressionEntry oldest_entry = CreatePassportEntry();
  WriteToStore("oldest_guid", oldest_entry,
               /*creation_time=*/base::Time::UnixEpoch());
  WriteSuppressionsWithRandomGuidToStore(
      /*count=*/kMaxSuppressions - 1,
      /*creation_time=*/test::kJune2017);
  CreateBridge();
  EntitySuppressionEntry remote_entry = CreateDriversLicenseEntry();
  syncer::EntityChangeList remote_changes;
  remote_changes.push_back(CreateAddChange("remote_guid", remote_entry));

  EXPECT_CALL(mock_processor(), Delete("oldest_guid", _, _));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(remote_changes));

  EXPECT_EQ(bridge().GetSuppressions().size(), kMaxSuppressions);
  EXPECT_FALSE(bridge().IsSuppressed(oldest_entry));
  EXPECT_TRUE(bridge().IsSuppressed(remote_entry));
}

// Tests that of suppressions with equal creation times, the one with the larger
// GUID is deleted, so that all clients delete the same suppressions.
TEST_F(EntitySuppressionSyncBridgeTest,
       DeletesLargerGuidFirstForEqualCreationTimes) {
  EntitySuppressionEntry entry_a = CreatePassportEntry();
  EntitySuppressionEntry entry_b = CreateDriversLicenseEntry();
  WriteToStore("guid_a", entry_a, /*creation_time=*/base::Time::UnixEpoch());
  WriteToStore("guid_b", entry_b, /*creation_time=*/base::Time::UnixEpoch());
  WriteSuppressionsWithRandomGuidToStore(
      /*count=*/kMaxSuppressions - 1,
      /*creation_time=*/test::kJune2017);

  EXPECT_CALL(mock_processor(), Delete("guid_b", _, _));
  CreateBridge();

  EXPECT_TRUE(bridge().IsSuppressed(entry_a));
  EXPECT_FALSE(bridge().IsSuppressed(entry_b));
}

// Tests that a load without corrupt records logs zero dropped records.
TEST_F(EntitySuppressionSyncBridgeTest, Metrics_LoadWithoutDroppedRecords) {
  WriteToStore("guid1", CreatePassportEntry());
  base::HistogramTester histogram_tester;

  CreateBridge();

  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.Suppression.Load.DroppedRecordCount", 0, 1);
  histogram_tester.ExpectTotalCount("Autofill.Ai.Suppression.Load.DropReason",
                                    0);
}

// Tests that records which cannot be decrypted, parsed or converted are dropped
// on load and that the reasons and the dropped count are logged.
TEST_F(EntitySuppressionSyncBridgeTest, Metrics_LoadWithDroppedRecords) {
  EntitySuppressionEntry valid_entry = CreatePassportEntry();
  WriteToStore("guid1", valid_entry);
  WriteRawToStore("guid2", "not encrypted");
  // 0xFF encodes field 31 with the invalid wire type 7.
  WriteEncryptedToStore("guid3", "\xFF");
  // Empty specifics lack the suppression key and are therefore invalid.
  WriteEncryptedToStore(
      "guid4",
      sync_pb::AutofillEntitySuppressionSpecifics().SerializeAsString());
  base::HistogramTester histogram_tester;

  CreateBridge();

  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(valid_entry)));
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.Suppression.Load.DroppedRecordCount", 3, 1);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.Ai.Suppression.Load.DropReason"),
      base::BucketsAre(
          base::Bucket(EntitySuppressionLoadDropReason::kDecryptionFailed, 1),
          base::Bucket(EntitySuppressionLoadDropReason::kParseFailed, 1),
          base::Bucket(EntitySuppressionLoadDropReason::kInvalidEntry, 1)));
}

// Tests that `TrimAllSupportedFieldsFromRemoteSpecifics()` correctly trims all
// supported fields from the specifics.
TEST_F(EntitySuppressionSyncBridgeTest,
       TrimAllSupportedFieldsFromRemoteSpecifics) {
  CreateBridge();
  sync_pb::EntitySpecifics entity_specifics;
  *entity_specifics.mutable_autofill_entity_suppression() =
      CreateSpecificsFromEntitySuppressionKey(
          "guid1", ToKey(CreatePassportEntry()), base::Time::Now(),
          /*base_specifics=*/{});
  entity_specifics.mutable_autofill_entity_suppression()->set_schema_version(1);

  EXPECT_EQ(bridge()
                .TrimAllSupportedFieldsFromRemoteSpecifics(entity_specifics)
                .ByteSizeLong(),
            0u);
}

// Tests that supported fields and nested messages are successfully trimmed but
// that unsupported fields are preserved.
TEST_F(EntitySuppressionSyncBridgeTest,
       TrimAllSupportedFieldsFromRemoteSpecifics_PreserveUnsupportedFields) {
  CreateBridge();
  sync_pb::EntitySpecifics trimmed_entity_specifics;
  sync_pb::AutofillEntitySuppressionSpecifics*
      suppression_specifics_with_only_unknown_fields =
          trimmed_entity_specifics.mutable_autofill_entity_suppression();

  // Set an unsupported field in the top-level message and in the nested key.
  AddUnknownFieldToProto(*suppression_specifics_with_only_unknown_fields,
                         "unsupported_fields");
  AddUnknownFieldToProto(*suppression_specifics_with_only_unknown_fields
                              ->mutable_entity_suppression_key(),
                         "unsupported_key_fields");

  // Create a copy and set supported values in the top-level and nested
  // messages.
  sync_pb::EntitySpecifics entity_specifics = trimmed_entity_specifics;
  sync_pb::AutofillEntitySuppressionSpecifics* specifics =
      entity_specifics.mutable_autofill_entity_suppression();
  specifics->set_guid("guid1");
  specifics->set_schema_version(1);
  specifics->set_creation_time_windows_epoch_micros(12345);
  specifics->mutable_entity_suppression_key()->set_entity_type_name("passport");
  sync_pb::EntitySuppressionKey::Attribute* attr =
      specifics->mutable_entity_suppression_key()->add_attributes();
  attr->set_name("passport_number");
  attr->set_value_hash("hash");

  EXPECT_THAT(
      bridge().TrimAllSupportedFieldsFromRemoteSpecifics(entity_specifics),
      EqualsProto(trimmed_entity_specifics));
}

// Tests that `GetDataForCommit()` includes unknown fields from the server.
TEST_F(EntitySuppressionSyncBridgeTest,
       GetDataForCommit_PreservesUnknownFields) {
  WriteToStore("guid1", CreatePassportEntry());
  CreateBridge();

  sync_pb::EntitySpecifics base_specifics;
  AddUnknownFieldToProto(*base_specifics.mutable_autofill_entity_suppression(),
                         "unknown_field");
  EXPECT_CALL(mock_processor(), GetPossiblyTrimmedRemoteSpecifics("guid1"))
      .WillOnce(ReturnRef(base_specifics));

  std::unique_ptr<syncer::DataBatch> batch =
      bridge().GetDataForCommit({"guid1"});

  ASSERT_TRUE(batch);
  ASSERT_TRUE(batch->HasNext());
  const syncer::KeyAndData& data_pair = batch->Next();
  EXPECT_EQ(data_pair.first, "guid1");
  EXPECT_THAT(data_pair.second->specifics.autofill_entity_suppression(),
              HasUnknownField("unknown_field"));
}

// Tests that `Suppress()` includes unknown fields from the server when
// calling `Put()`.
TEST_F(EntitySuppressionSyncBridgeTest, LocalSuppress_PreservesUnknownFields) {
  CreateBridge();

  sync_pb::EntitySpecifics base_specifics;
  AddUnknownFieldToProto(*base_specifics.mutable_autofill_entity_suppression(),
                         "unknown_field");
  EXPECT_CALL(mock_processor(), GetPossiblyTrimmedRemoteSpecifics)
      .WillOnce(ReturnRef(base_specifics));

  EXPECT_CALL(mock_processor(), Put)
      .WillOnce([](const std::string& storage_key,
                   std::unique_ptr<syncer::EntityData> entity_data,
                   syncer::MetadataChangeList* metadata_change_list) {
        EXPECT_THAT(entity_data->specifics.autofill_entity_suppression(),
                    HasUnknownField("unknown_field"));
      });

  EXPECT_TRUE(bridge().Suppress(CreatePassportEntry()));
}

// Tests that a legacy record with full hashes from the store suppresses its
// entry and is committed unchanged.
// TODO(crbug.com/501036619): Remove once legacy entries are no longer
// supported.
TEST_F(EntitySuppressionSyncBridgeTest,
       LegacyEntryFromStore_IsSuppressedAndCommittedUnchanged) {
  EntitySuppressionEntry entry = CreatePassportEntry();
  const sync_pb::AutofillEntitySuppressionSpecifics specifics =
      CreateSpecificsFromEntitySuppressionKey(
          "guid1", LegacyEntitySuppressionEntry(entry), base::Time::Now(),
          /*base_specifics=*/{});
  WriteEncryptedToStore("guid1", specifics.SerializeAsString());
  CreateBridge();

  EXPECT_TRUE(bridge().IsSuppressed(entry));
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_FALSE(bridge().Suppress(entry));

  std::unique_ptr<syncer::DataBatch> batch =
      bridge().GetDataForCommit({"guid1"});
  ASSERT_TRUE(batch->HasNext());
  syncer::KeyAndData data = batch->Next();
  EXPECT_EQ(data.first, "guid1");
  EXPECT_THAT(data.second->specifics.autofill_entity_suppression(),
              EqualsProto(specifics));
}

// Tests that unsuppressing an entry also deletes its legacy record.
// TODO(crbug.com/501036619): Remove once legacy entries are no longer
// supported.
TEST_F(EntitySuppressionSyncBridgeTest, LegacyEntryFromStore_IsUnsuppressed) {
  EntitySuppressionEntry entry = CreatePassportEntry();
  WriteEncryptedToStore("guid1",
                        CreateSpecificsFromEntitySuppressionKey(
                            "guid1", LegacyEntitySuppressionEntry(entry),
                            base::Time::Now(), /*base_specifics=*/{})
                            .SerializeAsString());
  CreateBridge();
  ASSERT_TRUE(bridge().IsSuppressed(entry));

  EXPECT_CALL(mock_processor(), Delete("guid1", _, _));
  EXPECT_CALL(observer(), OnSuppressionsChanged());
  EXPECT_TRUE(bridge().Unsuppress(entry));

  EXPECT_FALSE(bridge().IsSuppressed(entry));
  EXPECT_TRUE(bridge().GetSuppressions().empty());
  EXPECT_TRUE(ReadAllRecordsFromStore().empty());
}

// Tests that a legacy record with full hashes from Sync suppresses its entry.
// TODO(crbug.com/501036619): Remove once legacy entries are no longer
// supported.
TEST_F(EntitySuppressionSyncBridgeTest, LegacyEntryFromSync_IsSuppressed) {
  CreateBridge();
  EntitySuppressionEntry entry = CreatePassportEntry();
  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      "guid1", std::move(*CreateEntityDataFromEntitySuppressionKey(
                   "guid1", LegacyEntitySuppressionEntry(entry),
                   base::Time::Now(), /*base_specifics=*/{}))));

  EXPECT_CALL(observer(), OnSuppressionsChanged());
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(add_changes));

  EXPECT_TRUE(bridge().IsSuppressed(entry));
  EXPECT_THAT(
      bridge().GetSuppressions(),
      ElementsAre(EntitySuppressionKey(LegacyEntitySuppressionEntry(entry))));
}

}  // namespace

}  // namespace autofill
