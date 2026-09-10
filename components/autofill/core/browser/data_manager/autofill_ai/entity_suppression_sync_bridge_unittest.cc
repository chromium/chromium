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

  EntitySuppressionSyncBridge& bridge() { return *bridge_; }
  NiceMock<MockBridgeObserver>& observer() { return observer_; }
  const os_crypt_async::Encryptor& encryptor() { return *encryptor_; }
  NiceMock<syncer::MockDataTypeLocalChangeProcessor>& mock_processor() {
    return mock_processor_;
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

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<const os_crypt_async::Encryptor> encryptor_;
  std::unique_ptr<syncer::DataTypeStore> store_;
  NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  NiceMock<MockBridgeObserver> observer_;
  std::unique_ptr<EntitySuppressionSyncBridge> bridge_;
};

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

  EXPECT_CALL(mock_processor(), Put);
  EXPECT_CALL(observer(), OnSuppressionsChanged());
  EXPECT_TRUE(bridge().Suppress(entry));

  EXPECT_THAT(bridge().GetSuppressions(), ElementsAre(entry));
}

// Tests that unsuppressing an entry deletes it from local state, commits
// the deletion to Sync, and notifies observers.
TEST_F(EntitySuppressionSyncBridgeTest, LocalSuppressAndUnsuppress) {
  CreateBridge();
  EntitySuppressionEntry entry = CreatePassportEntry();
  ASSERT_TRUE(bridge().Suppress(entry));
  ASSERT_THAT(bridge().GetSuppressions(), ElementsAre(entry));
  std::string guid = GetFirstGuid();

  EXPECT_CALL(mock_processor(), Delete(guid, _, _));
  EXPECT_CALL(observer(), OnSuppressionsChanged());
  EXPECT_TRUE(bridge().Unsuppress(entry));

  EXPECT_TRUE(bridge().GetSuppressions().empty());
}

// Tests that unsuppressing a non-existing entry returns `false` without
// calling Sync or notifying observers.
TEST_F(EntitySuppressionSyncBridgeTest, LocalUnsuppressNonExistingEntry) {
  CreateBridge();

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(observer(), OnSuppressionsChanged()).Times(0);
  EXPECT_FALSE(bridge().Unsuppress(CreatePassportEntry()));
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

}  // namespace

}  // namespace autofill
