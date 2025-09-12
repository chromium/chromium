// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/valuables/valuable_sync_bridge.h"

#include <memory>
#include <string_view>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_sync_util.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_sync_metadata_table.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/valuables/valuables_sync_test_utils.h"
#include "components/autofill/core/browser/webdata/valuables/valuables_sync_util.h"
#include "components/autofill/core/browser/webdata/valuables/valuables_table.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/protocol/autofill_valuable_specifics.pb.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/webdata/common/web_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using base::test::EqualsProto;
using testing::_;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Return;
using testing::UnorderedElementsAre;

constexpr char kId1[] = "1";
constexpr char kId2[] = "2";
constexpr char kInvalidId[] = "";

std::vector<LoyaltyCard> ExtractLoyaltyCardsFromDataBatch(
    std::unique_ptr<syncer::DataBatch> batch) {
  std::vector<LoyaltyCard> loyalty_cards;
  while (batch->HasNext()) {
    const syncer::KeyAndData& data_pair = batch->Next();
    loyalty_cards.push_back(CreateAutofillLoyaltyCardFromSpecifics(
        data_pair.second->specifics.autofill_valuable()));
  }
  return loyalty_cards;
}

std::vector<EntityInstance> ExtractEntitiesFromDataBatch(
    std::unique_ptr<syncer::DataBatch> batch) {
  std::vector<EntityInstance> entities;
  while (batch->HasNext()) {
    const syncer::KeyAndData& data_pair = batch->Next();
    entities.push_back(*CreateEntityInstanceFromSpecifics(
        data_pair.second->specifics.autofill_valuable()));
  }
  return entities;
}

std::unique_ptr<syncer::EntityData> CreateEntityDataFromLoyaltyCardSpecifics(
    const sync_pb::AutofillValuableSpecifics& card_specifics) {
  std::unique_ptr<syncer::EntityData> entity_data =
      std::make_unique<syncer::EntityData>();
  entity_data->name = card_specifics.id();
  sync_pb::AutofillValuableSpecifics* specifics =
      entity_data->specifics.mutable_autofill_valuable();
  specifics->CopyFrom(card_specifics);

  return entity_data;
}

EntityInstance GetLocalVehicleEntityInstance(
    test::VehicleOptions options = {}) {
  return test::GetVehicleEntityInstance(
      {.record_type = EntityInstance::RecordType::kLocal});
}

EntityInstance GetServerVehicleEntityInstance(
    test::VehicleOptions options = {}) {
  options.nickname = "";
  options.date_modified = {};
  options.record_type = EntityInstance::RecordType::kServerWallet;
  return test::GetVehicleEntityInstance(options);
}

EntityInstance GetServerFlightEntityInstance(
    test::FlightReservationOptions options = {}) {
  options.nickname = "";
  options.date_modified = {};
  options.record_type = EntityInstance::RecordType::kServerWallet;
  return test::GetFlightReservationEntityInstance(options);
}

}  // namespace

class ValuableSyncBridgeTest : public testing::Test {
 public:
  // Creates the `bridge()` and mocks its `ValuablesTable`.
  void SetUp() override {
    feature_list_.InitWithFeatures({syncer::kSyncMoveValuablesToProfileDb,
                                    syncer::kSyncWalletFlightReservations,
                                    syncer::kSyncWalletVehicleRegistrations},
                                   /*disabled_features=*/{});
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&valuables_table_);
    db_.AddTable(&sync_metadata_table_);
    db_.AddTable(&entity_table_);
    db_.Init(temp_dir_.GetPath().AppendASCII("SyncTestWebDatabase"),
             &encryptor_);
    ON_CALL(backend_, GetDatabase()).WillByDefault(Return(&db_));

    bridge_ = std::make_unique<ValuableSyncBridge>(
        mock_processor_.CreateForwardingProcessor(), &backend_);
  }

  // Tells the processor to starts syncing with pre-existing `loyalty_cards`.
  // Triggers the `bridge()`'s `MergeFullSyncData()`.
  // Returns true if syncing started successfully.
  bool SyncLoyaltyCards(const std::vector<LoyaltyCard>& loyalty_cards) {
    ON_CALL(mock_processor(), IsTrackingMetadata).WillByDefault(Return(true));
    syncer::EntityChangeList entity_data;
    for (const LoyaltyCard& card : loyalty_cards) {
      entity_data.push_back(syncer::EntityChange::CreateAdd(
          card.id().value(), CardToEntityData(card)));
    }
    // `MergeFullSyncData()` returns an error if it fails.
    return !bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                                       std::move(entity_data));
  }

  // Tells the processor to starts syncing with pre-existing `entities`.
  // Triggers the `bridge()`'s `MergeFullSyncData()`.
  // Returns true if syncing started successfully.
  bool SyncEntityInstances(const std::vector<EntityInstance>& entities) {
    ON_CALL(mock_processor(), IsTrackingMetadata).WillByDefault(Return(true));
    syncer::EntityChangeList entity_data;
    for (const EntityInstance& entity : entities) {
      entity_data.push_back(syncer::EntityChange::CreateAdd(
          entity.guid().value(), EntityInstanceToEntityData(entity)));
    }
    // `MergeFullSyncData()` returns an error if it fails.
    return !bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                                       std::move(entity_data));
  }

  void AddLoyaltyCards(const std::vector<LoyaltyCard>& loyalty_cards) {
    valuables_table_.SetLoyaltyCards(loyalty_cards);
  }

  void AddEntities(const std::vector<EntityInstance>& entities) {
    for (const EntityInstance& entitity : entities) {
      entity_table_.AddOrUpdateEntityInstance(entitity);
    }
  }

  std::vector<LoyaltyCard> GetAllLoyaltyCardsFromTable() {
    return valuables_table_.GetLoyaltyCards();
  }

  std::vector<EntityInstance> GetAllEntityInstancesFromTable() {
    return entity_table_.GetEntityInstances();
  }

  syncer::EntityData CardToEntityData(const LoyaltyCard& card) {
    return std::move(*CreateEntityDataFromLoyaltyCard(card));
  }

  syncer::EntityData EntityInstanceToEntityData(const EntityInstance& entity) {
    return std::move(*CreateEntityDataFromEntityInstance(entity));
  }

  MockAutofillWebDataBackend& backend() { return backend_; }

  syncer::MockDataTypeLocalChangeProcessor& mock_processor() {
    return mock_processor_;
  }

  ValuableSyncBridge& bridge() { return *bridge_; }

 private:
  base::ScopedTempDir temp_dir_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  testing::NiceMock<MockAutofillWebDataBackend> backend_;
  const os_crypt_async::Encryptor encryptor_ =
      os_crypt_async::GetTestEncryptorForTesting();
  ValuablesTable valuables_table_;
  AutofillSyncMetadataTable sync_metadata_table_;
  EntityTable entity_table_;
  WebDatabase db_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ValuableSyncBridge> bridge_;
};

// Tests that a failure in the database initialization reports an error and
// doesn't cause a crash.
// Regression test for crbug.com/1421663.
TEST_F(ValuableSyncBridgeTest, InitializationFailure) {
  // The database will be null if it failed to initialize.
  ON_CALL(backend(), GetDatabase()).WillByDefault(Return(nullptr));
  EXPECT_CALL(mock_processor(), ReportError);
  // The `bridge()` was already initialized during `SetUp()`. Recreate it.
  ValuableSyncBridge(mock_processor().CreateForwardingProcessor(), &backend());
}

TEST_F(ValuableSyncBridgeTest, IsEntityDataValid) {
  // Valid case.
  std::unique_ptr<syncer::EntityData> entity =
      CreateEntityDataFromLoyaltyCard(TestLoyaltyCard(kId1));
  EXPECT_TRUE(bridge().IsEntityDataValid(*entity));
  // Invalid case.
  entity->specifics.mutable_autofill_valuable()->set_id(kInvalidId);
  EXPECT_FALSE(bridge().IsEntityDataValid(*entity));
}

TEST_F(ValuableSyncBridgeTest, IsLoyaltyCardEntityDataValid) {
  sync_pb::AutofillValuableSpecifics specifics = TestLoyaltyCardSpecifics(kId1);
  EXPECT_TRUE(bridge().IsEntityDataValid(
      *CreateEntityDataFromLoyaltyCardSpecifics(specifics)));

  specifics.mutable_loyalty_card()->clear_program_logo();
  EXPECT_TRUE(bridge().IsEntityDataValid(
      *CreateEntityDataFromLoyaltyCardSpecifics(specifics)));
  EXPECT_TRUE(
      bridge().IsEntityDataValid(*CreateEntityDataFromLoyaltyCardSpecifics(
          TestLoyaltyCardSpecifics(kId1, /*program_logo=*/""))));
}

TEST_F(ValuableSyncBridgeTest, IsLoyaltyCardEntityDataInvalid) {
  // Invalid id.
  EXPECT_FALSE(
      bridge().IsEntityDataValid(*CreateEntityDataFromLoyaltyCardSpecifics(
          TestLoyaltyCardSpecifics(kInvalidId))));

  // Invalid program logo.
  EXPECT_FALSE(
      bridge().IsEntityDataValid(*CreateEntityDataFromLoyaltyCardSpecifics(
          TestLoyaltyCardSpecifics(kId1, /*program_logo=*/"logo.png"))));

  // Invalid number.
  EXPECT_FALSE(bridge().IsEntityDataValid(
      *CreateEntityDataFromLoyaltyCardSpecifics(TestLoyaltyCardSpecifics(
          kId1, /*program_logo=*/"http://foobar.com/logo.png",
          /*number=*/""))));

  // Invalid merchant name.
  sync_pb::AutofillValuableSpecifics empty_merchant_name_specifics =
      TestLoyaltyCardSpecifics(kId1);
  empty_merchant_name_specifics.mutable_loyalty_card()->clear_merchant_name();
  EXPECT_FALSE(
      bridge().IsEntityDataValid(*CreateEntityDataFromLoyaltyCardSpecifics(
          empty_merchant_name_specifics)));
}

TEST_F(ValuableSyncBridgeTest, GetStorageKey) {
  std::unique_ptr<syncer::EntityData> entity =
      CreateEntityDataFromLoyaltyCard(TestLoyaltyCard(kId1));
  ASSERT_TRUE(bridge().IsEntityDataValid(*entity));
  EXPECT_EQ(kId1, bridge().GetStorageKey(*entity));
}

TEST_F(ValuableSyncBridgeTest, GetClientTag) {
  std::unique_ptr<syncer::EntityData> entity =
      CreateEntityDataFromLoyaltyCard(TestLoyaltyCard(kId1));
  ASSERT_TRUE(bridge().IsEntityDataValid(*entity));
  EXPECT_EQ(kId1, bridge().GetClientTag(*entity));
}

TEST_F(ValuableSyncBridgeTest, SupportsIncrementalUpdates) {
  ASSERT_FALSE(bridge().SupportsIncrementalUpdates());
}

// Tests that during the initial sync, `MergeFullSyncData()` incorporates remote
// loyalty cards.
TEST_F(ValuableSyncBridgeTest, MergeFullSyncData) {
  const LoyaltyCard remote1 = TestLoyaltyCard(kId1);
  const LoyaltyCard remote2 = TestLoyaltyCard(kId2);

  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(backend(), CommitChanges);
  EXPECT_CALL(backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_VALUABLE));

  EXPECT_TRUE(SyncLoyaltyCards({remote1, remote2}));

  EXPECT_THAT(GetAllLoyaltyCardsFromTable(),
              UnorderedElementsAre(remote1, remote2));
}

// Tests that loyalty cards with empty logo url are synced and stored.
TEST_F(ValuableSyncBridgeTest, LoyaltyCardsWithNoProgramLogo) {
  const LoyaltyCard remote1 = LoyaltyCard(
      ValuableId(std::string("no_logo")), "merchant_name", "program_name",
      GURL(), "card_number", {GURL("https://domain.example")});

  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(backend(), CommitChanges);
  EXPECT_CALL(backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_VALUABLE));

  EXPECT_TRUE(SyncLoyaltyCards({remote1}));

  EXPECT_THAT(GetAllLoyaltyCardsFromTable(), UnorderedElementsAre(remote1));
}

// Tests that `MergeFullSyncData()` replaces currently stored loyalty cards.
TEST_F(ValuableSyncBridgeTest, MergeFullSyncData_ReplacePreviousData) {
  const LoyaltyCard remote1 = TestLoyaltyCard(kId1);
  const LoyaltyCard remote2 = TestLoyaltyCard(kId2);

  EXPECT_CALL(backend(), CommitChanges).Times(2);
  EXPECT_CALL(backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_VALUABLE))
      .Times(2);

  EXPECT_TRUE(SyncLoyaltyCards({remote1}));
  EXPECT_THAT(GetAllLoyaltyCardsFromTable(), ElementsAre(remote1));

  EXPECT_TRUE(SyncLoyaltyCards({remote2}));
  EXPECT_THAT(GetAllLoyaltyCardsFromTable(), ElementsAre(remote2));
}

using ValuableSyncBridgeDeathTest = ValuableSyncBridgeTest;

// Tests that entity changes passed to `ApplyIncrementalSyncChanges()`
// are rejected.
TEST_F(ValuableSyncBridgeDeathTest, ApplyIncrementalSyncChanges) {
  const LoyaltyCard remote1 = TestLoyaltyCard(kId1);
  const LoyaltyCard remote2 = TestLoyaltyCard(kId2);

  // Add a new loyalty card.
  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(syncer::EntityChange::CreateAdd(
      kId2, std::move(*CreateEntityDataFromLoyaltyCard(remote2))));

  // Expect no changes to the loyalty cards.
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(backend(), CommitChanges());
  EXPECT_CALL(backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_VALUABLE));

  ASSERT_TRUE(SyncLoyaltyCards(/*loyalty_cards=*/{remote1}));
  EXPECT_THAT(GetAllLoyaltyCardsFromTable(), ElementsAre(remote1));

  // `ApplyIncrementalSyncChanges()` does not apply the incremental update.
  EXPECT_DEATH_IF_SUPPORTED(
      {
        bridge().ApplyIncrementalSyncChanges(
            bridge().CreateMetadataChangeList(), std::move(entity_change_list));
      },
      ".*");

  // Expect that the local loyalty cards have NOT changed.
  EXPECT_THAT(GetAllLoyaltyCardsFromTable(), ElementsAre(remote1));
}

// Tests that `GetDataForCommit()` returns empty collection.
TEST_F(ValuableSyncBridgeDeathTest, GetDataForCommit) {
  EXPECT_DEATH_IF_SUPPORTED({ bridge().GetDataForCommit({}); }, ".*");
}

// Tests that `GetAllDataForDebugging()` returns all loyalty cards.
TEST_F(ValuableSyncBridgeTest, GetAllDataForDebuggingForLoyaltyCards) {
  const LoyaltyCard card1 = TestLoyaltyCard(kId1);
  const LoyaltyCard card2 = TestLoyaltyCard(kId2);
  AddLoyaltyCards({card1, card2});

  std::vector<LoyaltyCard> loyalty_cards =
      ExtractLoyaltyCardsFromDataBatch(bridge().GetAllDataForDebugging());
  EXPECT_THAT(loyalty_cards, UnorderedElementsAre(card1, card2));
}

// Tests that `GetAllDataForDebugging()` returns no vehicle registrations if the
// profile DB flag is not enabled.
TEST_F(ValuableSyncBridgeTest,
       GetAllDataForDebuggingForVehicleRegistrationsFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(syncer::kSyncMoveValuablesToProfileDb);
  EntityInstance server_vehicle = GetServerVehicleEntityInstance(
      {.guid = "00000000-0000-4000-8000-500000000000"});
  AddEntities({server_vehicle});

  std::vector<EntityInstance> entities =
      ExtractEntitiesFromDataBatch(bridge().GetAllDataForDebugging());
  EXPECT_THAT(entities, IsEmpty());
}

// Tests that `ApplyDisableSyncChanges()` clears all data in ValuablesTable when
// the data type gets disabled.
TEST_F(ValuableSyncBridgeTest, ApplyDisableSyncChanges) {
  const LoyaltyCard card1 = TestLoyaltyCard(kId1);
  ASSERT_TRUE(SyncLoyaltyCards({card1}));
  ASSERT_THAT(GetAllLoyaltyCardsFromTable(), ElementsAre(card1));

  EXPECT_CALL(backend(), CommitChanges());
  EXPECT_CALL(backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_VALUABLE));

  bridge().ApplyDisableSyncChanges(bridge().CreateMetadataChangeList());

  EXPECT_TRUE(GetAllLoyaltyCardsFromTable().empty());
}

// Tests that trimming `AutofillValuableSpecifics` with only supported values
// set results in a zero-length specifics.
TEST_F(ValuableSyncBridgeTest,
       TrimAllSupportedFieldsFromRemoteSpecificsPreservesOnlySupportedFields) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillValuableSpecifics* autofill_valuables_specifics =
      specifics.mutable_autofill_valuable();
  sync_pb::LoyaltyCard* loyalty_card =
      autofill_valuables_specifics->mutable_loyalty_card();
  loyalty_card->mutable_program_name()->assign("program_name");
  loyalty_card->mutable_program_logo()->assign("program_logo");
  loyalty_card->mutable_merchant_name()->assign("merchant_name");
  loyalty_card->mutable_loyalty_card_number()->assign("card_number");
  *loyalty_card->add_merchant_domains() = "https://www.domain.example";

  EXPECT_EQ(bridge()
                .TrimAllSupportedFieldsFromRemoteSpecifics(specifics)
                .ByteSizeLong(),
            0u);
}

// Tests that trimming `AutofillValuableSpecifics` with unsupported fields
// will only preserve the unknown fields.
TEST_F(ValuableSyncBridgeTest,
       TrimRemoteSpecificsReturnsEmptyProtoWhenAllFieldsAreSupported) {
  sync_pb::EntitySpecifics specifics_with_only_unknown_fields;
  *specifics_with_only_unknown_fields.mutable_autofill_valuable()
       ->mutable_unknown_fields() = "unsupported_fields";

  sync_pb::EntitySpecifics specifics_with_known_and_unknown_fields =
      specifics_with_only_unknown_fields;
  sync_pb::AutofillValuableSpecifics* autofill_valuables_specifics =
      specifics_with_known_and_unknown_fields.mutable_autofill_valuable();
  sync_pb::LoyaltyCard* loyalty_card =
      autofill_valuables_specifics->mutable_loyalty_card();

  loyalty_card->mutable_program_name()->assign("program_name");
  loyalty_card->mutable_program_logo()->assign("program_logo");
  loyalty_card->mutable_merchant_name()->assign("merchant_name");
  loyalty_card->mutable_loyalty_card_number()->assign("card_number");
  *loyalty_card->add_merchant_domains() = "https://www.domain.example";

  EXPECT_THAT(bridge().TrimAllSupportedFieldsFromRemoteSpecifics(
                  specifics_with_known_and_unknown_fields),
              EqualsProto(specifics_with_only_unknown_fields));
}

// Tests that when the server sends the same data as the client has, nothing
// changes on the client.
TEST_F(ValuableSyncBridgeTest, MergeFullSyncData_SameValuablesData) {
  const LoyaltyCard card1 = TestLoyaltyCard(kId1);
  const LoyaltyCard card2 = TestLoyaltyCard(kId2);
  AddLoyaltyCards({card1, card2});

  EXPECT_CALL(backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_VALUABLE))
      .Times(0);
  // We still need to commit the updated progress marker on the client.
  EXPECT_CALL(backend(), CommitChanges());
  SyncLoyaltyCards({card1, card2});
  EXPECT_THAT(GetAllLoyaltyCardsFromTable(),
              UnorderedElementsAre(card1, card2));
}

// Tests that `SetEntities()` does nothing when the profile db migration feature
// flag is disabled.
TEST_F(ValuableSyncBridgeTest, SetEntities_ProfileDbMigrationFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(syncer::kSyncMoveValuablesToProfileDb);

  const EntityInstance local_vehicle = GetLocalVehicleEntityInstance(
      {.guid = "00000000-0000-4000-8000-300000000000"});
  const EntityInstance wallet_vehicle = GetServerVehicleEntityInstance(
      {.guid = "00000000-0000-5000-3000-200000000000"});

  AddEntities({local_vehicle});

  EXPECT_TRUE(SyncEntityInstances({wallet_vehicle}));
  EXPECT_THAT(GetAllEntityInstancesFromTable(),
              UnorderedElementsAre(local_vehicle));
}

// Tests that `GetAllDataForDebugging()` returns all vehicle registrations.
TEST_F(ValuableSyncBridgeTest, GetAllDataForDebuggingForVehicleRegistrations) {
  EntityInstance local_vehicle = GetLocalVehicleEntityInstance(
      {.guid = "00000000-0000-4000-8000-300000000000"});
  EntityInstance server_vehicle = GetServerVehicleEntityInstance(
      {.guid = "00000000-0000-4000-8000-500000000000"});
  AddEntities({local_vehicle, server_vehicle});

  std::vector<EntityInstance> entities =
      ExtractEntitiesFromDataBatch(bridge().GetAllDataForDebugging());
  EXPECT_THAT(entities, ElementsAre(server_vehicle));
}

// Tests that `GetAllDataForDebugging()` returns all flight reservations.
TEST_F(ValuableSyncBridgeTest, GetAllDataForDebuggingForFlightReservations) {
  const EntityInstance flight1 = GetServerFlightEntityInstance(
      {.guid = "00000000-0000-4000-8000-300000000000"});
  const EntityInstance flight2 = GetServerFlightEntityInstance(
      {.guid = "00000000-0000-5000-3000-200000000000"});
  AddEntities({flight1, flight2});

  std::vector<EntityInstance> entities =
      ExtractEntitiesFromDataBatch(bridge().GetAllDataForDebugging());
  EXPECT_THAT(entities, ElementsAre(flight1, flight2));
}

// Tests that `SetEntities()` does not add vehicle entities when the vehicle
// sync feature is disabled.
TEST_F(ValuableSyncBridgeTest, SetEntities_VehicleSyncDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({syncer::kSyncMoveValuablesToProfileDb},
                                {syncer::kSyncWalletVehicleRegistrations});

  EXPECT_CALL(backend(), CommitChanges);
  EXPECT_CALL(backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_VALUABLE));
  EXPECT_TRUE(SyncEntityInstances({GetServerVehicleEntityInstance()}));
  EXPECT_THAT(GetAllEntityInstancesFromTable(), IsEmpty());
}

// Tests that `SetEntities()` correctly adds vehicle entities to the table.
TEST_F(ValuableSyncBridgeTest, SetEntities_AddsVehicles) {
  const EntityInstance vehicle1 = GetServerVehicleEntityInstance(
      {.guid = "00000000-0000-4000-8000-300000000000"});
  const EntityInstance vehicle2 = GetServerVehicleEntityInstance(
      {.guid = "00000000-0000-5000-3000-200000000000"});

  EXPECT_CALL(backend(), CommitChanges);
  EXPECT_CALL(backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_VALUABLE));
  EXPECT_TRUE(SyncEntityInstances({vehicle1, vehicle2}));

  EXPECT_THAT(GetAllEntityInstancesFromTable(),
              UnorderedElementsAre(vehicle1, vehicle2));
}

// Tests that `SetEntities()` correctly adds flight reservations to the table.
TEST_F(ValuableSyncBridgeTest, SetEntities_AddsFlights) {
  const EntityInstance flight1 = GetServerFlightEntityInstance(
      {.guid = "00000000-0000-4000-8000-300000000000"});
  const EntityInstance flight2 = GetServerFlightEntityInstance(
      {.guid = "00000000-0000-5000-3000-200000000000"});

  EXPECT_CALL(backend(), CommitChanges);
  EXPECT_CALL(backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_VALUABLE));
  EXPECT_TRUE(SyncEntityInstances({flight1, flight2}));

  EXPECT_THAT(GetAllEntityInstancesFromTable(),
              UnorderedElementsAre(flight1, flight2));
}

// Tests that `SetEntities()` clears any existing entities before adding new
// ones.
TEST_F(ValuableSyncBridgeTest, SetEntities_ClearsExistingEntities) {
  const EntityInstance local_vehicle = GetLocalVehicleEntityInstance(
      {.guid = "00000000-0000-4000-8000-300000000000"});
  const EntityInstance wallet_vehicle = GetServerVehicleEntityInstance(
      {.guid = "00000000-0000-5000-3000-200000000000"});

  AddEntities({local_vehicle, wallet_vehicle});
  ASSERT_THAT(GetAllEntityInstancesFromTable(),
              UnorderedElementsAre(local_vehicle, wallet_vehicle));

  EXPECT_CALL(backend(), CommitChanges);
  EXPECT_CALL(backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_VALUABLE));

  const EntityInstance new_wallet_vehicle = GetServerVehicleEntityInstance(
      {.guid = "00000000-0000-1000-1000-100000000000"});
  EXPECT_TRUE(SyncEntityInstances({new_wallet_vehicle}));

  EXPECT_THAT(GetAllEntityInstancesFromTable(),
              UnorderedElementsAre(local_vehicle, new_wallet_vehicle));
}

}  // namespace autofill
