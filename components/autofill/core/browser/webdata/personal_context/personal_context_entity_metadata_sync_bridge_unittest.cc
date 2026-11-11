// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/personal_context/personal_context_entity_metadata_sync_bridge.h"

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/personal_context_metrics.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/browser/webdata/personal_context/personal_context_entity_metadata_sync_util.h"
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
using ::testing::Field;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::UnorderedElementsAre;

// The maximum number of suppressions and confirmations that the bridge keeps.
constexpr size_t kMaxSuppressions = 1000;
constexpr size_t kMaxConfirmations = 1000;

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

// Returns the hash under which `Suppress()` stores `entry`.
EntitySuppressionHash ToKey(const EntitySuppressionEntry& entry) {
  return GetEntitySuppressionHash(entry);
}

// TODO(crbug.com/501036619): Remove once legacy entries are no longer
// supported.
sync_pb::AutofillEntitySuppressionSpecifics CreateLegacyKeySpecifics(
    const std::string& guid,
    const EntitySuppressionEntry& entry,
    base::Time creation_time = base::Time::Now()) {
  sync_pb::AutofillEntitySuppressionSpecifics specifics;
  specifics.set_guid(guid);
  specifics.set_creation_time_windows_epoch_micros(
      creation_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  sync_pb::EntitySuppressionKey* key =
      specifics.mutable_entity_suppression_key();
  key->set_entity_type_name(std::string(entry.type.name_as_string()));
  for (const auto& [attr_type, hash] : entry.attribute_hashes) {
    sync_pb::EntitySuppressionKey::Attribute* attr = key->add_attributes();
    attr->set_name(std::string(attr_type.name_as_string()));
    attr->set_value_hash(hash.value().data(), hash.value().size());
  }
  return specifics;
}

std::unique_ptr<syncer::EntityChange> CreateAddChange(
    const std::string& guid,
    const EntitySuppressionEntry& entry) {
  return syncer::EntityChange::CreateAdd(
      guid, std::move(*CreateEntityDataFromEntitySuppressionHash(
                guid, ToKey(entry), base::Time::Now(), /*base_specifics=*/{})));
}

std::unique_ptr<syncer::EntityChange> CreateUpdateChange(
    const std::string& guid,
    const EntitySuppressionEntry& entry) {
  return syncer::EntityChange::CreateUpdate(
      guid, std::move(*CreateEntityDataFromEntitySuppressionHash(
                guid, ToKey(entry), base::Time::Now(), /*base_specifics=*/{})));
}

class MockBridgeObserver
    : public PersonalContextEntityMetadataSyncBridge::Observer {
 public:
  MOCK_METHOD(void, OnSuppressionsChanged, (), (override));
};

class PersonalContextEntityMetadataSyncBridgeTest : public testing::Test {
 public:
  PersonalContextEntityMetadataSyncBridgeTest()
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
    bridge_ = std::make_unique<PersonalContextEntityMetadataSyncBridge>(
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
    WriteEncryptedToStore(guid, CreateSpecificsFromEntitySuppressionHash(
                                    guid, ToKey(entry), creation_time,
                                    /*base_specifics=*/{})
                                    .SerializeAsString());
  }

  void WriteConfirmationToStore(const std::string& guid,
                                EntityConfirmationHash hash,
                                base::Time creation_time = base::Time::Now()) {
    WriteEncryptedToStore(guid, CreateSpecificsFromEntityConfirmationHash(
                                    guid, hash, creation_time,
                                    /*base_specifics=*/{})
                                    .SerializeAsString());
  }

  // Writes `count` confirmations with distinct hashes, random GUIDs and the
  // given `creation_time` to the store.
  void WriteConfirmationsWithRandomGuidToStore(size_t count,
                                               base::Time creation_time) {
    std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
        store_->CreateWriteBatch();
    for (size_t i = 0; i < count; ++i) {
      const std::string guid =
          base::Uuid::GenerateRandomV4().AsLowercaseString();
      const EntityConfirmationHash hash(static_cast<uint32_t>(i));
      std::string encrypted_blob;
      ASSERT_TRUE(encryptor().EncryptString(
          CreateSpecificsFromEntityConfirmationHash(guid, hash, creation_time,
                                                    /*base_specifics=*/{})
              .SerializeAsString(),
          &encrypted_blob));
      batch->WriteData(guid, encrypted_blob);
    }
    CommitToStore(std::move(batch));
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
          CreateSpecificsFromEntitySuppressionHash(guid, hash, creation_time,
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

  sync_pb::AutofillEntitySuppressionSpecifics DecryptRecord(
      const syncer::DataTypeStore::Record& record) {
    std::string plaintext;
    EXPECT_TRUE(encryptor().DecryptString(record.value, &plaintext));
    sync_pb::AutofillEntitySuppressionSpecifics specifics;
    EXPECT_TRUE(specifics.ParseFromString(plaintext));
    return specifics;
  }

  PersonalContextEntityMetadataSyncBridge& bridge() { return *bridge_; }
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
  std::unique_ptr<PersonalContextEntityMetadataSyncBridge> bridge_;
};

// Tests that when the encryptor has no usable key, the bridge reports an error
// instead of loading.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       EncryptionUnavailableReportsError) {
  EXPECT_CALL(mock_processor(), ModelReadyToSync).Times(0);
  EXPECT_CALL(mock_processor(),
              ReportError(Property(
                  &syncer::ModelError::type,
                  syncer::ModelError::Type::
                      kAutofillEntitySuppressionEncryptionUnavailable)));

  os_crypt_async::OSCryptAsync os_crypt_async_without_keys({});
  PersonalContextEntityMetadataSyncBridge bridge(
      mock_processor().CreateForwardingProcessor(),
      syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store().get()),
      &os_crypt_async_without_keys);
  task_environment().RunUntilIdle();

  EXPECT_FALSE(bridge.IsLoaded());
}

// Tests that pre-existing encrypted records in the store are loaded and
// decrypted upon bridge creation.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest, InitialLoadFromStore) {
  EntitySuppressionEntry entry = CreatePassportEntry();
  WriteToStore("guid1", entry);

  EXPECT_CALL(observer(), OnSuppressionsChanged());

  CreateBridge();

  EXPECT_TRUE(bridge().IsLoaded());
  EXPECT_EQ(GetFirstGuid(), "guid1");
  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(entry)));
}

// Tests that duplicate records for the same suppression in the store are
// deduplicated on load, keeping the lexicographically smaller GUID and deleting
// the loser from the store and Sync.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       InitialLoadFromStore_DeduplicatesMatchingEntriesWithDifferentGuids) {
  EntitySuppressionEntry entry = CreatePassportEntry();
  WriteToStore("guid1", entry);
  WriteToStore("guid2", entry);

  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(mock_processor(), Delete("guid2", _, _));
  EXPECT_CALL(observer(), OnSuppressionsChanged());

  CreateBridge();

  EXPECT_EQ(GetFirstGuid(), "guid1");
  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(entry)));
  EXPECT_THAT(ReadAllRecordsFromStore(),
              ElementsAre(Field(&syncer::DataTypeStore::Record::id, "guid1")));
}

// Tests that post-load cleanup does not write to the store or Sync if
// `ModelReadyToSync()` clears local state via `ApplyDisableSyncChanges()`.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       InitialLoadFromStore_SkipsCleanupWhenSyncDisabledDuringReadyToSync) {
  EntitySuppressionEntry entry = CreatePassportEntry();
  WriteToStore("guid1", entry);
  WriteToStore("guid2", entry);

  EXPECT_CALL(mock_processor(), ModelReadyToSync)
      .WillOnce([&](std::unique_ptr<syncer::MetadataBatch>) {
        ON_CALL(mock_processor(), IsTrackingMetadata())
            .WillByDefault(Return(false));
        bridge().ApplyDisableSyncChanges(bridge().CreateMetadataChangeList());
      });
  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(), Put).Times(0);

  CreateBridge();

  EXPECT_TRUE(bridge().GetSuppressions().empty());
  EXPECT_TRUE(ReadAllRecordsFromStore().empty());
}

// Tests that suppression entries written to the store are encrypted.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest, DataIsEncryptedInStore) {
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
  EXPECT_EQ(CreateEntitySuppressionHashFromSpecifics(specifics), ToKey(entry));
}

// Tests that locally suppressing an entry adds it to local suppressions,
// commits it to Sync, and notifies observers.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest, LocalSuppress) {
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
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       LocalSuppressAndUnsuppress) {
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
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       LocalUnsuppressNonExistingEntry) {
  CreateBridge();

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(0);
  EXPECT_FALSE(bridge().Unsuppress(CreatePassportEntry()));
}

// Tests that incremental remote additions update local suppressions and
// notify observers.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       ApplyIncrementalSyncChanges_Add) {
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
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       ApplyIncrementalSyncChanges_Delete) {
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
TEST_F(PersonalContextEntityMetadataSyncBridgeTest, ApplyDisableSyncChanges) {
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
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
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
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
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
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       Suppress_DuplicateEntryReturnsFalse) {
  CreateBridge();
  EntitySuppressionEntry entry = CreatePassportEntry();
  ASSERT_TRUE(bridge().Suppress(entry));

  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(0);
  EXPECT_FALSE(bridge().Suppress(entry));
}

// Tests that Suppress returns false without modifying local state or notifying
// observers when Sync is not tracking metadata.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
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
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
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
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
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
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
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
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
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
    PersonalContextEntityMetadataSyncBridgeTest,
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
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
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
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
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

// Tests that ClearEntries with a full time range deletes all entries from
// local state and store, commits deletions to Sync, and notifies observers
// once.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest, ClearEntries) {
  WriteToStore("guid1", CreatePassportEntry());
  WriteToStore("guid2", CreateDriversLicenseEntry());
  CreateBridge();
  ASSERT_EQ(bridge().GetSuppressions().size(), 2u);

  EXPECT_CALL(mock_processor(), Delete("guid1", _, _));
  EXPECT_CALL(mock_processor(), Delete("guid2", _, _));
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(1);
  EXPECT_TRUE(bridge().ClearEntries(base::Time(), base::Time::Max()));

  EXPECT_TRUE(bridge().GetSuppressions().empty());
  EXPECT_TRUE(ReadAllRecordsFromStore().empty());
}

// Tests that ClearEntries only deletes entries whose creation time falls
// within [delete_begin, delete_end].
TEST_F(PersonalContextEntityMetadataSyncBridgeTest, ClearEntries_TimeRange) {
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
  EXPECT_TRUE(bridge().ClearEntries(t1 + base::Seconds(50), base::Time::Max()));

  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(entry1)));
  EXPECT_EQ(ReadAllRecordsFromStore().size(), 1u);
}

// Tests that ClearEntries returns false and does not notify observers
// when there are no matching suppressions.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest, ClearEntries_Empty) {
  CreateBridge();
  ASSERT_TRUE(bridge().GetSuppressions().empty());

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(0);
  EXPECT_FALSE(bridge().ClearEntries(base::Time(), base::Time::Max()));
}

// Tests that CountEntriesCreatedBetween only counts entries whose
// creation time falls within the inclusive range [begin, end].
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       CountEntriesCreatedBetween) {
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
      bridge().CountEntriesCreatedBetween(base::Time(), base::Time::Max()), 2u);
  EXPECT_EQ(bridge().CountEntriesCreatedBetween(base::Time(), base::Time()),
            2u);
  // Bounds are inclusive.
  EXPECT_EQ(bridge().CountEntriesCreatedBetween(t1, t1), 1u);
  EXPECT_EQ(bridge().CountEntriesCreatedBetween(t2, t2), 1u);
  EXPECT_EQ(bridge().CountEntriesCreatedBetween(t1, t2), 2u);
  EXPECT_EQ(bridge().CountEntriesCreatedBetween(t1 + base::Seconds(50),
                                                base::Time::Max()),
            1u);
  // Ranges not containing any creation time.
  EXPECT_EQ(bridge().CountEntriesCreatedBetween(base::Time::Min(),
                                                t1 - base::Seconds(1)),
            0u);
  EXPECT_EQ(bridge().CountEntriesCreatedBetween(t1 + base::Seconds(1),
                                                t2 - base::Seconds(1)),
            0u);
  EXPECT_EQ(bridge().CountEntriesCreatedBetween(t2 + base::Seconds(1),
                                                base::Time::Max()),
            0u);

  // After clearing, nothing is left.
  ASSERT_TRUE(bridge().ClearEntries(base::Time(), base::Time::Max()));
  EXPECT_EQ(
      bridge().CountEntriesCreatedBetween(base::Time(), base::Time::Max()), 0u);
}

// Tests that CountEntriesCreatedBetween returns zero when there are no
// suppressions.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       CountEntriesCreatedBetween_Empty) {
  CreateBridge();
  ASSERT_TRUE(bridge().GetSuppressions().empty());

  EXPECT_EQ(
      bridge().CountEntriesCreatedBetween(base::Time(), base::Time::Max()), 0u);
}

// Tests that suppressions above the limit, e.g. stored by builds without the
// limit, are deleted on load.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
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
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
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
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
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
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
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
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
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
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       Metrics_LoadWithoutDroppedRecords) {
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
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       Metrics_LoadWithDroppedRecords) {
  EntitySuppressionEntry valid_entry = CreatePassportEntry();
  WriteToStore("guid1", valid_entry);
  WriteRawToStore("guid2", "not encrypted");
  // 0xFF encodes field 31 with the invalid wire type 7.
  WriteEncryptedToStore("guid3", "\xFF");
  // Empty specifics lack the suppression key and are therefore invalid.
  WriteEncryptedToStore(
      "guid4",
      sync_pb::AutofillEntitySuppressionSpecifics().SerializeAsString());
  // Specifics whose `guid` does not match the storage record ID are invalid.
  WriteEncryptedToStore("guid5",
                        CreateSpecificsFromEntitySuppressionHash(
                            "other_guid", ToKey(CreateDriversLicenseEntry()),
                            base::Time::Now(), /*base_specifics=*/{})
                            .SerializeAsString());
  base::HistogramTester histogram_tester;

  CreateBridge();

  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(valid_entry)));
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.Suppression.Load.DroppedRecordCount", 4, 1);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.Ai.Suppression.Load.DropReason"),
      base::BucketsAre(
          base::Bucket(EntitySuppressionLoadDropReason::kDecryptionFailed, 1),
          base::Bucket(EntitySuppressionLoadDropReason::kParseFailed, 1),
          base::Bucket(EntitySuppressionLoadDropReason::kInvalidEntry, 2)));
}

// Tests that `TrimAllSupportedFieldsFromRemoteSpecifics()` correctly trims all
// supported fields from the specifics.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       TrimAllSupportedFieldsFromRemoteSpecifics) {
  CreateBridge();
  sync_pb::EntitySpecifics entity_specifics;
  *entity_specifics.mutable_autofill_entity_suppression() =
      CreateSpecificsFromEntitySuppressionHash(
          "guid1", ToKey(CreatePassportEntry()), base::Time::Now(),
          /*base_specifics=*/{});

  EXPECT_EQ(bridge()
                .TrimAllSupportedFieldsFromRemoteSpecifics(entity_specifics)
                .ByteSizeLong(),
            0u);
}

// Tests that supported fields and nested messages are successfully trimmed but
// that unsupported fields are preserved.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
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
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
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
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       LocalSuppress_PreservesUnknownFields) {
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

// Tests that a legacy record with full hashes (`entity_suppression_key`) in the
// store suppresses its entry, is migrated in-place to also populate
// `suppression.hash` and `entity_suppression_hash`, and preserves
// `entity_suppression_key` across `GetDataForCommit()`.
// TODO(crbug.com/501036619): Remove once legacy entries are no longer
// supported.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       LegacyKeyFromStore_IsMigratedOnLoad) {
  EntitySuppressionEntry entry = CreatePassportEntry();
  const uint32_t hash = GetEntitySuppressionHash(entry).value();
  const sync_pb::AutofillEntitySuppressionSpecifics legacy_specifics =
      CreateLegacyKeySpecifics("guid1", entry);
  WriteEncryptedToStore("guid1", legacy_specifics.SerializeAsString());

  sync_pb::AutofillEntitySuppressionSpecifics expected_specifics =
      legacy_specifics;
  expected_specifics.mutable_suppression()->set_hash(hash);
  expected_specifics.set_entity_suppression_hash(hash);

  EXPECT_CALL(mock_processor(), Put("guid1", _, _));
  EXPECT_CALL(observer(), OnSuppressionsChanged());
  CreateBridge();

  EXPECT_TRUE(bridge().IsSuppressed(entry));
  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(entry)));
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_FALSE(bridge().Suppress(entry));

  syncer::DataTypeStore::RecordList records = ReadAllRecordsFromStore();
  ASSERT_EQ(records.size(), 1u);
  EXPECT_THAT(DecryptRecord(records[0]), EqualsProto(expected_specifics));

  std::unique_ptr<syncer::DataBatch> batch =
      bridge().GetDataForCommit({"guid1"});
  ASSERT_TRUE(batch->HasNext());
  syncer::KeyAndData data = batch->Next();
  EXPECT_EQ(data.first, "guid1");
  EXPECT_THAT(data.second->specifics.autofill_entity_suppression(),
              EqualsProto(expected_specifics));
}

// Tests that unsuppressing an entry also deletes its legacy record.
// TODO(crbug.com/501036619): Remove once legacy entries are no longer
// supported.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       LegacyEntryFromStore_IsUnsuppressed) {
  EntitySuppressionEntry entry = CreatePassportEntry();
  WriteEncryptedToStore(
      "guid1", CreateLegacyKeySpecifics("guid1", entry).SerializeAsString());
  CreateBridge();
  ASSERT_TRUE(bridge().IsSuppressed(entry));

  EXPECT_CALL(mock_processor(), Delete("guid1", _, _));
  EXPECT_CALL(observer(), OnSuppressionsChanged());
  EXPECT_TRUE(bridge().Unsuppress(entry));

  EXPECT_FALSE(bridge().IsSuppressed(entry));
  EXPECT_TRUE(bridge().GetSuppressions().empty());
  EXPECT_TRUE(ReadAllRecordsFromStore().empty());
}

// Tests that a legacy record with full hashes (`entity_suppression_key`)
// received during `MergeFullSyncData` suppresses its entry and is persisted
// without triggering a sync re-upload.
// TODO(crbug.com/501036619): Remove once legacy entries are no longer
// supported.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       LegacyKeyFromMergeFullSyncData_IsSuppressedWithoutResync) {
  CreateBridge();
  EntitySuppressionEntry entry = CreatePassportEntry();
  const sync_pb::AutofillEntitySuppressionSpecifics legacy_specifics =
      CreateLegacyKeySpecifics("guid1", entry);

  syncer::EntityChangeList remote_changes;
  remote_changes.push_back(syncer::EntityChange::CreateAdd(
      "guid1", std::move(*CreateEntityDataFromSpecifics(legacy_specifics))));

  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(observer(), OnSuppressionsChanged());
  bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                             std::move(remote_changes));

  EXPECT_TRUE(bridge().IsSuppressed(entry));
  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(entry)));
  syncer::DataTypeStore::RecordList records = ReadAllRecordsFromStore();
  ASSERT_EQ(records.size(), 1u);
  EXPECT_THAT(DecryptRecord(records[0]), EqualsProto(legacy_specifics));
}

// Tests that a legacy record with full hashes (`entity_suppression_key`)
// received during `ApplyIncrementalSyncChanges` suppresses its entry and is
// persisted without triggering a sync re-upload.
// TODO(crbug.com/501036619): Remove once legacy entries are no longer
// supported.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       LegacyKeyFromApplyIncrementalSyncChanges_IsSuppressedWithoutResync) {
  CreateBridge();
  EntitySuppressionEntry entry = CreatePassportEntry();
  const sync_pb::AutofillEntitySuppressionSpecifics legacy_specifics =
      CreateLegacyKeySpecifics("guid1", entry);

  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      "guid1", std::move(*CreateEntityDataFromSpecifics(legacy_specifics))));

  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(observer(), OnSuppressionsChanged());
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(add_changes));

  EXPECT_TRUE(bridge().IsSuppressed(entry));
  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(ToKey(entry)));
  syncer::DataTypeStore::RecordList records = ReadAllRecordsFromStore();
  ASSERT_EQ(records.size(), 1u);
  EXPECT_THAT(DecryptRecord(records[0]), EqualsProto(legacy_specifics));
}

// Tests that when the store contains both a hash-only record ("guid_a") and a
// legacy `entity_suppression_key` record ("guid_b") for the same entry, loading
// deduplicates them by smaller GUID ("guid_a") and deletes "guid_b".
// TODO(crbug.com/501036619): Remove once legacy entries are no longer
// supported.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       LegacyKeyAndHashDuplicateInStore_DeduplicatesBySmallerGuid) {
  EntitySuppressionEntry entry = CreatePassportEntry();
  WriteToStore("guid_a", entry);
  WriteEncryptedToStore(
      "guid_b", CreateLegacyKeySpecifics("guid_b", entry).SerializeAsString());

  EXPECT_CALL(mock_processor(), Delete("guid_b", _, _));
  EXPECT_CALL(mock_processor(), Put).Times(0);
  CreateBridge();

  EXPECT_TRUE(bridge().IsSuppressed(entry));
  EXPECT_EQ(GetFirstGuid(), "guid_a");
  syncer::DataTypeStore::RecordList records = ReadAllRecordsFromStore();
  ASSERT_EQ(records.size(), 1u);
  EXPECT_EQ(records[0].id, "guid_a");
}

// Tests that in `MergeFullSyncData`, a local record with
// `entity_suppression_key` and a smaller GUID beats a remote hash-only
// duplicate and is uploaded to Sync with `entity_suppression_key`,
// `entity_suppression_hash`, and `suppression.hash` populated.
// TODO(crbug.com/501036619): Remove once legacy entries are no longer
// supported.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       MergeFullSyncData_LocalLegacyKeyUploadedWithSmallerGuid) {
  EntitySuppressionEntry entry = CreatePassportEntry();
  const uint32_t hash = GetEntitySuppressionHash(entry).value();
  const sync_pb::AutofillEntitySuppressionSpecifics legacy_specifics =
      CreateLegacyKeySpecifics("guid_a", entry);
  WriteEncryptedToStore("guid_a", legacy_specifics.SerializeAsString());
  CreateBridge();

  sync_pb::AutofillEntitySuppressionSpecifics expected_specifics =
      legacy_specifics;
  expected_specifics.mutable_suppression()->set_hash(hash);
  expected_specifics.set_entity_suppression_hash(hash);

  EXPECT_CALL(mock_processor(), Delete("guid_b", _, _));
  EXPECT_CALL(mock_processor(), Put("guid_a", _, _))
      .WillOnce([&](const std::string& storage_key,
                    std::unique_ptr<syncer::EntityData> entity_data,
                    syncer::MetadataChangeList* metadata_change_list) {
        EXPECT_THAT(entity_data->specifics.autofill_entity_suppression(),
                    EqualsProto(expected_specifics));
      });
  syncer::EntityChangeList remote_changes;
  remote_changes.push_back(CreateAddChange("guid_b", entry));
  bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                             std::move(remote_changes));

  EXPECT_EQ(GetFirstGuid(), "guid_a");
  syncer::DataTypeStore::RecordList records = ReadAllRecordsFromStore();
  ASSERT_EQ(records.size(), 1u);
  EXPECT_EQ(records[0].id, "guid_a");
  EXPECT_THAT(DecryptRecord(records[0]), EqualsProto(expected_specifics));
}

// Tests that a legacy record with only `entity_suppression_hash` in the store
// is migrated on load to also populate `suppression.hash` in the store and
// Sync while preserving unknown fields.
// TODO(crbug.com/501036619): Remove once legacy entries are no longer
// supported.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       LegacyHashFromStore_IsMigratedOnLoad) {
  EntitySuppressionEntry entry = CreatePassportEntry();
  const uint32_t hash = GetEntitySuppressionHash(entry).value();
  sync_pb::AutofillEntitySuppressionSpecifics legacy_specifics;
  legacy_specifics.set_guid("guid1");
  legacy_specifics.set_entity_suppression_hash(hash);
  AddUnknownFieldToProto(legacy_specifics, "unknown_field");
  WriteEncryptedToStore("guid1", legacy_specifics.SerializeAsString());

  sync_pb::AutofillEntitySuppressionSpecifics expected_specifics =
      legacy_specifics;
  expected_specifics.mutable_suppression()->set_hash(hash);

  EXPECT_CALL(mock_processor(), Put("guid1", _, _));
  EXPECT_CALL(observer(), OnSuppressionsChanged());
  CreateBridge();

  EXPECT_TRUE(bridge().IsSuppressed(entry));
  syncer::DataTypeStore::RecordList records = ReadAllRecordsFromStore();
  ASSERT_EQ(records.size(), 1u);
  const sync_pb::AutofillEntitySuppressionSpecifics stored_specifics =
      DecryptRecord(records[0]);
  EXPECT_THAT(stored_specifics, EqualsProto(expected_specifics));
  EXPECT_THAT(stored_specifics, HasUnknownField("unknown_field"));
}

// Tests that a legacy record with only `entity_suppression_hash` received
// during `MergeFullSyncData` suppresses its entry and is persisted without
// triggering a sync re-upload.
// TODO(crbug.com/501036619): Remove once legacy entries are no longer
// supported.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       LegacyHashFromMergeFullSyncData_IsSuppressedWithoutResync) {
  CreateBridge();
  EntitySuppressionEntry entry = CreatePassportEntry();
  const uint32_t hash = GetEntitySuppressionHash(entry).value();
  sync_pb::AutofillEntitySuppressionSpecifics legacy_specifics;
  legacy_specifics.set_guid("guid1");
  legacy_specifics.set_entity_suppression_hash(hash);

  syncer::EntityChangeList remote_changes;
  remote_changes.push_back(syncer::EntityChange::CreateAdd(
      "guid1", std::move(*CreateEntityDataFromSpecifics(legacy_specifics))));

  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(observer(), OnSuppressionsChanged());
  bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                             std::move(remote_changes));

  EXPECT_TRUE(bridge().IsSuppressed(entry));
  syncer::DataTypeStore::RecordList records = ReadAllRecordsFromStore();
  ASSERT_EQ(records.size(), 1u);
  EXPECT_THAT(DecryptRecord(records[0]), EqualsProto(legacy_specifics));
}

// Tests that a legacy record with only `entity_suppression_hash` received
// during `ApplyIncrementalSyncChanges` suppresses its entry and is persisted
// without triggering a sync re-upload.
// TODO(crbug.com/501036619): Remove once legacy entries are no longer
// supported.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       LegacyHashFromApplyIncrementalSyncChanges_IsSuppressedWithoutResync) {
  CreateBridge();
  EntitySuppressionEntry entry = CreatePassportEntry();
  const uint32_t hash = GetEntitySuppressionHash(entry).value();
  sync_pb::AutofillEntitySuppressionSpecifics legacy_specifics;
  legacy_specifics.set_guid("guid1");
  legacy_specifics.set_entity_suppression_hash(hash);

  syncer::EntityChangeList remote_changes;
  remote_changes.push_back(syncer::EntityChange::CreateAdd(
      "guid1", std::move(*CreateEntityDataFromSpecifics(legacy_specifics))));

  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(observer(), OnSuppressionsChanged());
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(remote_changes));

  EXPECT_TRUE(bridge().IsSuppressed(entry));
  syncer::DataTypeStore::RecordList records = ReadAllRecordsFromStore();
  ASSERT_EQ(records.size(), 1u);
  EXPECT_THAT(DecryptRecord(records[0]), EqualsProto(legacy_specifics));
}

// Tests that pre-existing encrypted confirmation records in the store are
// loaded and decrypted upon bridge creation.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       InitialLoadFromStore_Confirmation) {
  const EntityConfirmationHash hash(0x12345u);
  WriteConfirmationToStore("guid1", hash);

  CreateBridge();

  EXPECT_TRUE(bridge().IsLoaded());
  EXPECT_EQ(GetFirstGuid(), "guid1");
  EXPECT_TRUE(bridge().IsConfirmed(hash));
  EXPECT_THAT(bridge().GetConfirmations(), ElementsAre(hash));
}

// Tests that locally confirming a hash adds it to local confirmations, writes
// an encrypted record to the store, and commits it to Sync without notifying
// suppression observers.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest, LocalConfirm) {
  CreateBridge();
  const EntityConfirmationHash hash(0x12345u);
  ASSERT_TRUE(bridge().GetConfirmations().empty());
  EXPECT_FALSE(bridge().IsConfirmed(hash));

  EXPECT_CALL(mock_processor(), Put);
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(0);
  EXPECT_TRUE(bridge().Confirm(hash));

  EXPECT_THAT(bridge().GetConfirmations(), ElementsAre(hash));
  EXPECT_TRUE(bridge().IsConfirmed(hash));

  syncer::DataTypeStore::RecordList records = ReadAllRecordsFromStore();
  ASSERT_EQ(records.size(), 1u);
  std::string decrypted_plaintext;
  EXPECT_TRUE(
      encryptor().DecryptString(records[0].value, &decrypted_plaintext));
  EXPECT_NE(records[0].value, decrypted_plaintext);
  EXPECT_EQ(
      CreateEntityConfirmationHashFromSpecifics(DecryptRecord(records[0])),
      hash);
}

// Tests that attempting to confirm an already confirmed hash returns `false`
// and performs no operations.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       Confirm_DuplicateHashReturnsFalse) {
  CreateBridge();
  const EntityConfirmationHash hash(0x12345u);
  ASSERT_TRUE(bridge().Confirm(hash));

  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_FALSE(bridge().Confirm(hash));
}

// Tests that Confirm returns false without modifying local state when Sync is
// not tracking metadata.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       Confirm_ReturnsFalseWhenSyncNotTrackingMetadata) {
  CreateBridge();
  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(false));

  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_FALSE(bridge().Confirm(EntityConfirmationHash(0x12345u)));
  EXPECT_TRUE(bridge().GetConfirmations().empty());
}

// Tests that incremental remote confirmation additions and deletions update
// local confirmations without notifying suppression observers.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       ApplyIncrementalSyncChanges_ConfirmationAddAndDelete) {
  CreateBridge();
  const EntityConfirmationHash hash1(0x12345u);
  const EntityConfirmationHash hash2(0x67890u);
  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      "guid1", std::move(*CreateEntityDataFromEntityConfirmationHash(
                   "guid1", hash1, base::Time::Now(), /*base_specifics=*/{}))));
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      "guid2", std::move(*CreateEntityDataFromEntityConfirmationHash(
                   "guid2", hash2, base::Time::Now(), /*base_specifics=*/{}))));

  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(0);
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(add_changes));

  EXPECT_THAT(bridge().GetConfirmations(), UnorderedElementsAre(hash1, hash2));

  syncer::EntityChangeList delete_changes;
  delete_changes.push_back(
      syncer::EntityChange::CreateDelete("guid1", syncer::EntityData()));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(delete_changes));

  EXPECT_THAT(bridge().GetConfirmations(), ElementsAre(hash2));
}

// Tests that `MergeFullSyncData` merges local and remote confirmations and
// uploads local-only confirmations to Sync.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       MergeFullSyncData_Confirmation) {
  const EntityConfirmationHash local_hash(0x12345u);
  const EntityConfirmationHash remote_hash(0x67890u);
  WriteConfirmationToStore("local_guid", local_hash);
  CreateBridge();
  ASSERT_THAT(bridge().GetConfirmations(), ElementsAre(local_hash));

  EXPECT_CALL(mock_processor(), Put("local_guid", _, _));
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(0);
  syncer::EntityChangeList remote_changes;
  remote_changes.push_back(syncer::EntityChange::CreateAdd(
      "remote_guid", std::move(*CreateEntityDataFromEntityConfirmationHash(
                         "remote_guid", remote_hash, base::Time::Now(),
                         /*base_specifics=*/{}))));
  bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                             std::move(remote_changes));

  EXPECT_THAT(bridge().GetConfirmations(),
              UnorderedElementsAre(local_hash, remote_hash));
}

// Tests that ClearEntries and CountEntriesCreatedBetween also cover
// confirmation records.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       ClearAndCountIncludeConfirmations) {
  const base::Time t1 =
      base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(100));
  const base::Time t2 =
      base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(200));
  WriteToStore("guid1", CreatePassportEntry(), t1);
  WriteConfirmationToStore("guid2", EntityConfirmationHash(0x12345u), t2);
  CreateBridge();

  EXPECT_EQ(
      bridge().CountEntriesCreatedBetween(base::Time(), base::Time::Max()), 2u);
  EXPECT_EQ(bridge().CountEntriesCreatedBetween(t2, t2), 1u);

  // Clearing only the confirmation record at `t2` should not notify suppression
  // observers.
  EXPECT_CALL(mock_processor(), Delete("guid2", _, _));
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(0);
  EXPECT_TRUE(bridge().ClearEntries(t1 + base::Seconds(50), base::Time::Max()));
  EXPECT_TRUE(bridge().GetConfirmations().empty());
  EXPECT_EQ(bridge().GetSuppressions().size(), 1u);
}

// Tests that confirming an entry at the limit deletes the oldest confirmation
// from local storage and Sync without affecting suppressions.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       Confirm_DeletesOldestConfirmationAtLimit) {
  const EntityConfirmationHash oldest_hash(0xABCDEu);
  EntitySuppressionEntry old_suppression = CreatePassportEntry();
  WriteToStore("suppression_guid", old_suppression,
               /*creation_time=*/base::Time::UnixEpoch());
  WriteConfirmationToStore("oldest_confirm_guid", oldest_hash,
                           /*creation_time=*/base::Time::UnixEpoch());
  WriteConfirmationsWithRandomGuidToStore(
      /*count=*/kMaxConfirmations - 1,
      /*creation_time=*/test::kJune2017);
  CreateBridge();

  const EntityConfirmationHash new_hash(0xFFFFFu);
  EXPECT_CALL(mock_processor(), Delete("oldest_confirm_guid", _, _));
  EXPECT_TRUE(bridge().Confirm(new_hash));

  EXPECT_EQ(bridge().GetConfirmations().size(), kMaxConfirmations);
  EXPECT_FALSE(bridge().IsConfirmed(oldest_hash));
  EXPECT_TRUE(bridge().IsConfirmed(new_hash));
  EXPECT_TRUE(bridge().IsSuppressed(old_suppression));
}

// Tests that confirmations above the limit are deleted on initial load.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       Load_DeletesOldestConfirmationsAboveLimit) {
  const EntityConfirmationHash oldest_hash(0xABCDEu);
  const EntityConfirmationHash second_oldest_hash(0xBCDEFu);
  WriteConfirmationToStore("oldest_guid", oldest_hash,
                           /*creation_time=*/base::Time::UnixEpoch());
  WriteConfirmationToStore("second_oldest_guid", second_oldest_hash,
                           /*creation_time=*/test::kJanuary2017);
  WriteConfirmationsWithRandomGuidToStore(
      /*count=*/kMaxConfirmations,
      /*creation_time=*/test::kJune2017);

  EXPECT_CALL(mock_processor(), Delete("oldest_guid", _, _));
  EXPECT_CALL(mock_processor(), Delete("second_oldest_guid", _, _));
  CreateBridge();

  EXPECT_EQ(bridge().GetConfirmations().size(), kMaxConfirmations);
  EXPECT_FALSE(bridge().IsConfirmed(oldest_hash));
  EXPECT_FALSE(bridge().IsConfirmed(second_oldest_hash));
  EXPECT_EQ(ReadAllRecordsFromStore().size(), kMaxConfirmations);
}

// Tests that `MergeFullSyncData` deletes the oldest confirmations above the
// limit without uploading evicted local-only confirmations or notifying
// suppression observers.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       MergeFullSyncData_DeletesOldestConfirmationsAboveLimit) {
  const EntityConfirmationHash oldest_hash(0xABCDEu);
  const EntityConfirmationHash remote_hash(0xFFFFFu);
  WriteConfirmationToStore("oldest_guid", oldest_hash,
                           /*creation_time=*/base::Time::UnixEpoch());
  WriteConfirmationsWithRandomGuidToStore(
      /*count=*/kMaxConfirmations - 1,
      /*creation_time=*/test::kJune2017);
  CreateBridge();

  syncer::EntityChangeList remote_changes;
  remote_changes.push_back(syncer::EntityChange::CreateAdd(
      "remote_guid", std::move(*CreateEntityDataFromEntityConfirmationHash(
                         "remote_guid", remote_hash, test::kJune2017,
                         /*base_specifics=*/{}))));

  EXPECT_CALL(mock_processor(), Put("oldest_guid", _, _)).Times(0);
  // The remaining `kMaxConfirmations - 1` local-only confirmations are
  // uploaded.
  EXPECT_CALL(mock_processor(), Put(Ne("oldest_guid"), _, _))
      .Times(kMaxConfirmations - 1);
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(0);
  bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                             std::move(remote_changes));

  EXPECT_EQ(bridge().GetConfirmations().size(), kMaxConfirmations);
  EXPECT_FALSE(bridge().IsConfirmed(oldest_hash));
  EXPECT_TRUE(bridge().IsConfirmed(remote_hash));
}

// Tests that incremental remote confirmation additions above the limit delete
// the oldest confirmations without notifying suppression observers.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       ApplyIncrementalSyncChanges_DeletesOldestConfirmationsAboveLimit) {
  const EntityConfirmationHash oldest_hash(0xABCDEu);
  const EntityConfirmationHash remote_hash(0xFFFFFu);
  WriteConfirmationToStore("oldest_guid", oldest_hash,
                           /*creation_time=*/base::Time::UnixEpoch());
  WriteConfirmationsWithRandomGuidToStore(
      /*count=*/kMaxConfirmations - 1,
      /*creation_time=*/test::kJune2017);
  CreateBridge();

  syncer::EntityChangeList remote_changes;
  remote_changes.push_back(syncer::EntityChange::CreateAdd(
      "remote_guid", std::move(*CreateEntityDataFromEntityConfirmationHash(
                         "remote_guid", remote_hash, test::kJune2017,
                         /*base_specifics=*/{}))));

  EXPECT_CALL(mock_processor(), Delete("oldest_guid", _, _));
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(0);
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(remote_changes));

  EXPECT_EQ(bridge().GetConfirmations().size(), kMaxConfirmations);
  EXPECT_FALSE(bridge().IsConfirmed(oldest_hash));
  EXPECT_TRUE(bridge().IsConfirmed(remote_hash));
}

// Tests that duplicate confirmation records in the store are deduplicated on
// load, keeping the smaller GUID.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       InitialLoadFromStore_DeduplicatesMatchingConfirmations) {
  const EntityConfirmationHash hash(0x12345u);
  WriteConfirmationToStore("guid1", hash);
  WriteConfirmationToStore("guid2", hash);

  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(mock_processor(), Delete("guid2", _, _));

  CreateBridge();

  EXPECT_EQ(GetFirstGuid(), "guid1");
  EXPECT_THAT(bridge().GetConfirmations(), ElementsAre(hash));
  EXPECT_THAT(ReadAllRecordsFromStore(),
              ElementsAre(Field(&syncer::DataTypeStore::Record::id, "guid1")));
}

// Tests that `MergeFullSyncData` and `ApplyIncrementalSyncChanges` deduplicate
// matching confirmation hashes deterministically using the smaller GUID.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       RemoteSync_DeduplicatesMatchingConfirmationsWithDifferentGuids) {
  const EntityConfirmationHash hash(0x12345u);
  WriteConfirmationToStore("guid2", hash);
  CreateBridge();
  ASSERT_THAT(bridge().GetConfirmations(), ElementsAre(hash));

  // In `MergeFullSyncData`, smaller remote "guid1" beats local "guid2".
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(mock_processor(), Delete("guid2", _, _));
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(0);
  syncer::EntityChangeList merge_changes;
  merge_changes.push_back(syncer::EntityChange::CreateAdd(
      "guid1", std::move(*CreateEntityDataFromEntityConfirmationHash(
                   "guid1", hash, base::Time::Now(), /*base_specifics=*/{}))));
  bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                             std::move(merge_changes));
  EXPECT_EQ(GetFirstGuid(), "guid1");

  // In `ApplyIncrementalSyncChanges`, larger remote "guid3" loses to "guid1".
  EXPECT_CALL(mock_processor(), Delete("guid3", _, _));
  syncer::EntityChangeList incremental_changes;
  incremental_changes.push_back(syncer::EntityChange::CreateAdd(
      "guid3", std::move(*CreateEntityDataFromEntityConfirmationHash(
                   "guid3", hash, base::Time::Now(), /*base_specifics=*/{}))));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(incremental_changes));
  EXPECT_EQ(GetFirstGuid(), "guid1");
  EXPECT_THAT(bridge().GetConfirmations(), ElementsAre(hash));
  EXPECT_THAT(ReadAllRecordsFromStore(),
              ElementsAre(Field(&syncer::DataTypeStore::Record::id, "guid1")));
}

// Tests that `GetDataForCommit` and `GetAllDataForDebugging` return
// confirmation entries and preserve cached unknown fields.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       GetDataForCommitAndDebugging_Confirmation) {
  const EntityConfirmationHash hash(0x12345u);
  WriteToStore("suppression_guid", CreatePassportEntry());
  WriteConfirmationToStore("confirm_guid", hash);
  CreateBridge();

  sync_pb::EntitySpecifics base_specifics;
  AddUnknownFieldToProto(*base_specifics.mutable_autofill_entity_suppression(),
                         "unknown_field");
  ON_CALL(mock_processor(), GetPossiblyTrimmedRemoteSpecifics("confirm_guid"))
      .WillByDefault(ReturnRef(base_specifics));

  std::unique_ptr<syncer::DataBatch> commit_batch =
      bridge().GetDataForCommit({"confirm_guid"});
  ASSERT_TRUE(commit_batch);
  ASSERT_TRUE(commit_batch->HasNext());
  const syncer::KeyAndData commit_item = commit_batch->Next();
  EXPECT_EQ(commit_item.first, "confirm_guid");
  EXPECT_EQ(CreateEntityConfirmationHashFromSpecifics(
                commit_item.second->specifics.autofill_entity_suppression()),
            hash);
  EXPECT_THAT(commit_item.second->specifics.autofill_entity_suppression(),
              HasUnknownField("unknown_field"));
  EXPECT_FALSE(commit_batch->HasNext());

  std::unique_ptr<syncer::DataBatch> debug_batch =
      bridge().GetAllDataForDebugging();
  ASSERT_TRUE(debug_batch);
  std::vector<std::string> keys;
  while (debug_batch->HasNext()) {
    keys.push_back(debug_batch->Next().first);
  }
  EXPECT_THAT(keys, UnorderedElementsAre("suppression_guid", "confirm_guid"));
}

// Tests that `ApplyDisableSyncChanges` wipes confirmations and local storage
// without notifying suppression observers when no suppressions exist.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       ApplyDisableSyncChanges_OnlyConfirmations) {
  CreateBridge();
  ASSERT_TRUE(bridge().Confirm(EntityConfirmationHash(0x12345u)));
  ASSERT_FALSE(bridge().GetConfirmations().empty());

  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(0);
  bridge().ApplyDisableSyncChanges(bridge().CreateMetadataChangeList());

  EXPECT_TRUE(bridge().GetConfirmations().empty());
  EXPECT_TRUE(ReadAllRecordsFromStore().empty());
}

// Tests `IsEntityDataValid` for valid, invalid, and empty confirmation
// specifics, and verifies that invalid remote specifics are ignored gracefully.
TEST_F(PersonalContextEntityMetadataSyncBridgeTest,
       IsEntityDataValidAndGracefulInvalidRemoteSpecifics_Confirmation) {
  CreateBridge();

  std::unique_ptr<syncer::EntityData> valid_confirmation =
      CreateEntityDataFromEntityConfirmationHash(
          "guid1", EntityConfirmationHash(0x12345u), base::Time::Now(),
          /*base_specifics=*/{});
  EXPECT_TRUE(bridge().IsEntityDataValid(*valid_confirmation));

  sync_pb::AutofillEntitySuppressionSpecifics missing_guid_specifics =
      valid_confirmation->specifics.autofill_entity_suppression();
  missing_guid_specifics.clear_guid();
  EXPECT_FALSE(bridge().IsEntityDataValid(
      *CreateEntityDataFromSpecifics(missing_guid_specifics)));

  sync_pb::AutofillEntitySuppressionSpecifics empty_confirmation_specifics;
  empty_confirmation_specifics.set_guid("guid1");
  empty_confirmation_specifics.mutable_confirmation();
  std::unique_ptr<syncer::EntityData> invalid_confirmation =
      CreateEntityDataFromSpecifics(empty_confirmation_specifics);
  EXPECT_FALSE(bridge().IsEntityDataValid(*invalid_confirmation));

  syncer::EntityData empty_entity_data;
  EXPECT_FALSE(bridge().IsEntityDataValid(empty_entity_data));

  // Invalid remote specifics passed to `MergeFullSyncData` or
  // `ApplyIncrementalSyncChanges` are ignored without crashing.
  syncer::EntityChangeList invalid_changes;
  invalid_changes.push_back(syncer::EntityChange::CreateAdd(
      "guid1", std::move(*invalid_confirmation)));
  bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                       std::move(invalid_changes));
  EXPECT_TRUE(bridge().GetConfirmations().empty());
  EXPECT_TRUE(ReadAllRecordsFromStore().empty());
}

}  // namespace

}  // namespace autofill
