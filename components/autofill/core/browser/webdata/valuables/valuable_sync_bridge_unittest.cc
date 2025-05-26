// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/valuables/valuable_sync_bridge.h"

#include <memory>
#include <string_view>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/test_utils/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_sync_metadata_table.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/valuables/valuables_sync_util.h"
#include "components/autofill/core/browser/webdata/valuables/valuables_table.h"
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

using testing::_;
using testing::ElementsAre;
using testing::Return;
using testing::UnorderedElementsAre;

constexpr char kId1[] = "1";
constexpr char kId2[] = "2";
constexpr char kInvalidId[] = "";

LoyaltyCard TestLoyaltyCard(std::string_view id) {
  return LoyaltyCard(ValuableId(std::string(id)), "merchant_name",
                     "program_name", GURL("http://foobar.com/logo.png"),
                     "card_number", {GURL("https://domain.example")});
}

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

}  // namespace

class ValuableSyncBridgeTest : public testing::Test {
 public:
  // Creates the `bridge()` and mocks its `ValuablesTable`.
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&table_);
    db_.AddTable(&sync_metadata_table_);
    db_.Init(temp_dir_.GetPath().AppendASCII("SyncTestWebDatabase"));
    ON_CALL(backend_, GetDatabase()).WillByDefault(Return(&db_));

    bridge_ = std::make_unique<ValuableSyncBridge>(
        mock_processor_.CreateForwardingProcessor(), &backend_);
  }

  // Tells the processor to starts syncing with pre-existing `loyalty_cards`.
  // Triggers the `bridge()`'s `MergeFullSyncData()`.
  // Returns true if syncing started successfully.
  bool StartSyncing(const std::vector<LoyaltyCard>& loyalty_cards) {
    ON_CALL(mock_processor(), IsTrackingMetadata).WillByDefault(Return(true));
    syncer::EntityChangeList entity_data;
    for (const LoyaltyCard& card : loyalty_cards) {
      entity_data.push_back(syncer::EntityChange::CreateAdd(
          card.id().value(), CardToEntity(card)));
    }
    // `MergeFullSyncData()` returns an error if it fails.
    return !bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                                       std::move(entity_data));
  }

  void AddLoyaltyCardsToTheTable(
      const std::vector<LoyaltyCard>& loyalty_cards) {
    table_.SetLoyaltyCards(loyalty_cards);
  }

  std::vector<LoyaltyCard> GetAllDataFromTable() {
    return table_.GetLoyaltyCards();
  }

  syncer::EntityData CardToEntity(const LoyaltyCard& card) {
    return std::move(*CreateEntityDataFromLoyaltyCard(card));
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
  ValuablesTable table_;
  AutofillSyncMetadataTable sync_metadata_table_;
  WebDatabase db_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
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
  // Valid case.
  std::unique_ptr<syncer::EntityData> entity =
      CreateEntityDataFromLoyaltyCard(TestLoyaltyCard(kId1));
  EXPECT_TRUE(bridge().IsEntityDataValid(*entity));
  // Invalid logo.
  entity->specifics.mutable_autofill_valuable()
      ->mutable_loyalty_card()
      ->set_program_logo("invalid_url");
  EXPECT_FALSE(bridge().IsEntityDataValid(*entity));
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

  EXPECT_TRUE(StartSyncing({remote1, remote2}));

  EXPECT_THAT(GetAllDataFromTable(), UnorderedElementsAre(remote1, remote2));
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

  EXPECT_TRUE(StartSyncing({remote1}));

  EXPECT_THAT(GetAllDataFromTable(), UnorderedElementsAre(remote1));
}

// Tests that `MergeFullSyncData()` replaces currently stored loyalty cards.
TEST_F(ValuableSyncBridgeTest, MergeFullSyncData_ReplacePreviousData) {
  const LoyaltyCard remote1 = TestLoyaltyCard(kId1);
  const LoyaltyCard remote2 = TestLoyaltyCard(kId2);

  EXPECT_CALL(backend(), CommitChanges).Times(2);
  EXPECT_CALL(backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_VALUABLE))
      .Times(2);

  EXPECT_TRUE(StartSyncing({remote1}));
  EXPECT_THAT(GetAllDataFromTable(), ElementsAre(remote1));

  EXPECT_TRUE(StartSyncing({remote2}));
  EXPECT_THAT(GetAllDataFromTable(), ElementsAre(remote2));
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

  ASSERT_TRUE(StartSyncing(/*loyalty_cards=*/{remote1}));
  EXPECT_THAT(GetAllDataFromTable(), ElementsAre(remote1));

  // `ApplyIncrementalSyncChanges()` does not apply the incremental update.
  EXPECT_DEATH_IF_SUPPORTED(
      {
        bridge().ApplyIncrementalSyncChanges(
            bridge().CreateMetadataChangeList(), std::move(entity_change_list));
      },
      ".*");

  // Expect that the local loyalty cards have NOT changed.
  EXPECT_THAT(GetAllDataFromTable(), ElementsAre(remote1));
}

// Tests that `GetDataForCommit()` returns empty collection.
TEST_F(ValuableSyncBridgeDeathTest, GetDataForCommit) {
  EXPECT_DEATH_IF_SUPPORTED({ bridge().GetDataForCommit({}); }, ".*");
}

// Tests that `GetAllDataForDebugging()` returns all local data.
TEST_F(ValuableSyncBridgeTest, GetAllDataForDebugging) {
  const LoyaltyCard card1 = TestLoyaltyCard(kId1);
  const LoyaltyCard card2 = TestLoyaltyCard(kId2);
  AddLoyaltyCardsToTheTable({card1, card2});

  std::vector<LoyaltyCard> loyalty_cards =
      ExtractLoyaltyCardsFromDataBatch(bridge().GetAllDataForDebugging());
  EXPECT_THAT(loyalty_cards, UnorderedElementsAre(card1, card2));
}

// Tests that `ApplyDisableSyncChanges()` clears all data in ValuablesTable when
// the data type gets disabled.
TEST_F(ValuableSyncBridgeTest, ApplyDisableSyncChanges) {
  const LoyaltyCard card1 = TestLoyaltyCard(kId1);
  ASSERT_TRUE(StartSyncing({card1}));
  ASSERT_THAT(GetAllDataFromTable(), ElementsAre(card1));

  EXPECT_CALL(backend(), CommitChanges());
  EXPECT_CALL(backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_VALUABLE));

  bridge().ApplyDisableSyncChanges(bridge().CreateMetadataChangeList());

  EXPECT_TRUE(GetAllDataFromTable().empty());
}

// Tests that trimming `AutofillValuableSpecifics` with only supported values
// set results in a zero-length specifics.
TEST_F(ValuableSyncBridgeTest,
       TrimAllSupportedFieldsFromRemoteSpecificsPreservesOnlySupportedFields) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillValuableSpecifics* autofill_valuables_specifics =
      specifics.mutable_autofill_valuable();
  sync_pb::AutofillValuableSpecifics::LoyaltyCard* loyalty_card =
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
  sync_pb::AutofillValuableSpecifics::LoyaltyCard* loyalty_card =
      autofill_valuables_specifics->mutable_loyalty_card();

  loyalty_card->mutable_program_name()->assign("program_name");
  loyalty_card->mutable_program_logo()->assign("program_logo");
  loyalty_card->mutable_merchant_name()->assign("merchant_name");
  loyalty_card->mutable_loyalty_card_number()->assign("card_number");
  *loyalty_card->add_merchant_domains() = "https://www.domain.example";

  EXPECT_EQ(bridge()
                .TrimAllSupportedFieldsFromRemoteSpecifics(
                    specifics_with_known_and_unknown_fields)
                .SerializeAsString(),
            specifics_with_only_unknown_fields.SerializePartialAsString());
}

// Tests that when the server sends the same data as the client has, nothing
// changes on the client.
TEST_F(ValuableSyncBridgeTest, MergeFullSyncData_SameValuablesData) {
  const LoyaltyCard card1 = TestLoyaltyCard(kId1);
  const LoyaltyCard card2 = TestLoyaltyCard(kId2);
  AddLoyaltyCardsToTheTable({card1, card2});

  EXPECT_CALL(backend(),
              NotifyOnAutofillChangedBySync(syncer::AUTOFILL_VALUABLE))
      .Times(0);
  // We still need to commit the updated progress marker on the client.
  EXPECT_CALL(backend(), CommitChanges());
  StartSyncing({card1, card2});
  EXPECT_THAT(GetAllDataFromTable(), UnorderedElementsAre(card1, card2));
}

}  // namespace autofill
