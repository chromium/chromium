// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/journeys/journeys_sync_bridge.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/observer_list.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "components/history/core/browser/history_backend_observer.h"
#include "components/history/core/browser/journeys/history_backend_for_journeys_sync.h"
#include "components/history/core/browser/journeys/journey_row.h"
#include "components/history/core/browser/journeys/journeys_sync_metadata_database.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/journey_specifics.pb.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync/test/unknown_field_util.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history::journeys {
namespace {

using base::test::EqualsProto;
using sync_pb::DataTypeState;
using sync_pb::EntityMetadata;
using sync_pb::EntitySpecifics;
using sync_pb::JourneySpecifics;
using syncer::EntityData;
using syncer::ModelError;
using testing::_;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::NiceMock;
using testing::Property;
using testing::Return;

constexpr char kTestJourneyId[] = "test_guid";
constexpr char kTestTitle[] = "test title";
constexpr char kTestEmoji[] = "test emoji";
constexpr char kTestOverview[] = "test overview";
constexpr char kTestShortOverview[] = "test short overview";
constexpr int64_t kTestVisitTimestamp = 200;
constexpr char kTestQueryTitle[] = "continuation title";
constexpr char kTestQueryPrompt[] = "continuation prompt";

JourneySpecifics CreateTestJourneySpecifics(
    const std::string& journey_id = kTestJourneyId,
    const std::string& title = kTestTitle) {
  JourneySpecifics specifics;
  specifics.set_journey_id(journey_id);
  specifics.set_title(title);
  specifics.set_emoji(kTestEmoji);
  specifics.set_creation_time_windows_epoch_micros(100);
  specifics.set_overview(kTestOverview);
  specifics.set_short_overview(kTestShortOverview);
  specifics.add_history_entries()->set_visit_timestamp_windows_epoch_micros(
      kTestVisitTimestamp);
  auto* query = specifics.add_continuation_queries();
  query->set_title(kTestQueryTitle);
  query->set_prompt(kTestQueryPrompt);
  return specifics;
}

JourneyRow CreateTestJourneyRow(const std::string& journey_id = kTestJourneyId,
                                const std::string& title = kTestTitle,
                                bool include_rich_fields = true) {
  JourneyRow row;
  row.journey_id = journey_id;
  row.title = title;
  row.emoji = kTestEmoji;
  row.creation_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(100));
  if (include_rich_fields) {
    row.overview = kTestOverview;
    row.short_overview = kTestShortOverview;
    row.history_entries.emplace_back(base::Time::FromDeltaSinceWindowsEpoch(
        base::Microseconds(kTestVisitTimestamp)));
    row.continuation_queries.emplace_back(kTestQueryTitle, kTestQueryPrompt);
  }
  return row;
}

EntityData CreateTestJourneyEntityData(const std::string& id = kTestJourneyId,
                                       const std::string& title = kTestTitle) {
  EntityData entity_data;
  *entity_data.specifics.mutable_journey() =
      CreateTestJourneySpecifics(id, title);
  entity_data.name = id;
  return entity_data;
}

MATCHER_P(HasModelErrorType, expected_type, "") {
  return testing::ExplainMatchResult(
      testing::Optional(
          testing::Property(&syncer::ModelError::type, expected_type)),
      arg, result_listener);
}

class FakeHistoryBackendForJourneysSync : public HistoryBackendForJourneysSync {
 public:
  bool AddOrUpdateJourneys(const std::vector<JourneyRow>& journeys) override {
    if (fail_operations_) {
      return false;
    }
    for (const auto& journey : journeys) {
      journeys_[journey.journey_id] = journey;
    }
    return true;
  }

  bool DeleteJourneys(const std::vector<std::string>& journey_ids) override {
    if (fail_operations_) {
      return false;
    }
    for (const auto& id : journey_ids) {
      journeys_.erase(id);
    }
    return true;
  }

  std::vector<JourneyRow> GetAllJourneys() override {
    std::vector<JourneyRow> result;
    for (const auto& [journey_id, journey] : journeys_) {
      result.push_back(journey);
    }
    return result;
  }

  bool DeleteAllJourneys() override {
    if (fail_operations_) {
      return false;
    }
    journeys_.clear();
    return true;
  }

  void AddObserver(HistoryBackendObserver* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(HistoryBackendObserver* observer) override {
    observers_.RemoveObserver(observer);
  }

  void NotifyHistoryDeletions(bool all_history) {
    for (HistoryBackendObserver& observer : observers_) {
      observer.OnHistoryDeletions(/*history_backend=*/nullptr, all_history,
                                  /*expired=*/false, /*deleted_rows=*/{},
                                  /*favicon_urls=*/{});
    }
  }

  void set_fail_operations(bool fail) { fail_operations_ = fail; }
  const std::map<std::string, JourneyRow>& journeys() const {
    return journeys_;
  }

 private:
  bool fail_operations_ = false;
  std::map<std::string, JourneyRow> journeys_;
  base::ObserverList<HistoryBackendObserver>::Unchecked observers_;
};

class JourneysSyncDatabaseWrapper {
 public:
  JourneysSyncDatabaseWrapper() : sync_metadata_db_(&db_, &meta_table_) {
    EXPECT_TRUE(db_.OpenInMemory());
    EXPECT_TRUE(
        meta_table_.Init(&db_, /*version=*/1, /*compatible_version=*/1));
    EXPECT_TRUE(sync_metadata_db_.Init());
  }
  ~JourneysSyncDatabaseWrapper() = default;

  JourneysSyncMetadataDatabase* sync_metadata_db() {
    return &sync_metadata_db_;
  }

  bool GetAllSyncMetadata(syncer::MetadataBatch* metadata_batch) {
    return sync_metadata_db_.GetAllSyncMetadata(metadata_batch);
  }

  bool UpdateEntityMetadata(syncer::DataType data_type,
                            const std::string& storage_key,
                            const sync_pb::EntityMetadata& metadata) {
    return sync_metadata_db_.UpdateEntityMetadata(data_type, storage_key,
                                                  metadata);
  }
  sql::Database* db() { return &db_; }

 private:
  sql::Database db_{sql::test::kTestTag};
  sql::MetaTable meta_table_;
  JourneysSyncMetadataDatabase sync_metadata_db_;
};

class JourneysSyncBridgeTest : public testing::Test {
 public:
  JourneysSyncBridgeTest() = default;
  ~JourneysSyncBridgeTest() override = default;

  JourneysSyncBridge CreateBridge() {
    return JourneysSyncBridge(&fake_backend_, db_.sync_metadata_db(),
                              mock_processor_.CreateForwardingProcessor());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  FakeHistoryBackendForJourneysSync fake_backend_;
  JourneysSyncDatabaseWrapper db_;
  NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
};

TEST_F(JourneysSyncBridgeTest, ModelReadyToSyncOnInitialization) {
  EXPECT_CALL(mock_processor_, ModelReadyToSync(_));
  JourneysSyncBridge bridge = CreateBridge();
}

TEST_F(JourneysSyncBridgeTest, ModelReadyToSyncWithExistingMetadata) {
  EntityMetadata metadata;
  metadata.set_client_tag_hash("test_hash");
  EXPECT_TRUE(
      db_.UpdateEntityMetadata(syncer::JOURNEY, kTestJourneyId, metadata));

  DataTypeState data_type_state;
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  EXPECT_TRUE(db_.sync_metadata_db()->UpdateDataTypeState(syncer::JOURNEY,
                                                          data_type_state));

  EXPECT_CALL(mock_processor_, ModelReadyToSync(_))
      .WillOnce([&data_type_state](
                    std::unique_ptr<syncer::MetadataBatch> batch) {
        ASSERT_TRUE(batch);
        EXPECT_EQ(batch->GetAllMetadata().size(), 1u);
        EXPECT_THAT(batch->GetDataTypeState(), EqualsProto(data_type_state));
      });
  JourneysSyncBridge bridge = CreateBridge();
}

TEST_F(JourneysSyncBridgeTest, IsEntityDataValid) {
  JourneysSyncBridge bridge = CreateBridge();
  EntityData data = CreateTestJourneyEntityData(kTestJourneyId);
  EXPECT_TRUE(bridge.IsEntityDataValid(data));

  EntityData invalid_data = CreateTestJourneyEntityData("");
  EXPECT_FALSE(bridge.IsEntityDataValid(invalid_data));

  EntityData invalid_no_specifics;
  EXPECT_FALSE(bridge.IsEntityDataValid(invalid_no_specifics));
}

TEST_F(JourneysSyncBridgeTest, GetClientTagAndStorageKey) {
  JourneysSyncBridge bridge = CreateBridge();
  EntityData entity_data = CreateTestJourneyEntityData(kTestJourneyId);

  EXPECT_EQ(kTestJourneyId, bridge.GetClientTag(entity_data));
  EXPECT_EQ(kTestJourneyId, bridge.GetStorageKey(entity_data));
}

TEST_F(JourneysSyncBridgeTest, TrimAllSupportedFieldsFromRemoteSpecifics) {
  JourneysSyncBridge bridge = CreateBridge();
  EntitySpecifics specifics;
  *specifics.mutable_journey() = CreateTestJourneySpecifics();

  EXPECT_THAT(bridge.TrimAllSupportedFieldsFromRemoteSpecifics(specifics),
              EqualsProto(EntitySpecifics()));
}

TEST_F(JourneysSyncBridgeTest,
       TrimAllSupportedFieldsFromRemoteSpecificsPreservesUnknownFields) {
  JourneysSyncBridge bridge = CreateBridge();
  EntitySpecifics specifics;
  JourneySpecifics* journey_specifics = specifics.mutable_journey();
  *journey_specifics = CreateTestJourneySpecifics();
  syncer::test::AddUnknownFieldToProto(*journey_specifics, "unknown_field");

  EntitySpecifics trimmed_specifics =
      bridge.TrimAllSupportedFieldsFromRemoteSpecifics(specifics);

  EXPECT_TRUE(trimmed_specifics.has_journey());
  EXPECT_THAT(trimmed_specifics.journey(),
              syncer::test::HasUnknownField("unknown_field"));
}

TEST_F(JourneysSyncBridgeTest, ApplyIncrementalSyncChangesAdd) {
  JourneysSyncBridge bridge = CreateBridge();

  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      "guid_1", CreateTestJourneyEntityData("guid_1", "Title 1")));
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      "guid_2", CreateTestJourneyEntityData("guid_2", "Title 2")));

  EXPECT_EQ(std::nullopt,
            bridge.ApplyIncrementalSyncChanges(
                bridge.CreateMetadataChangeList(), std::move(add_changes)));

  EXPECT_EQ(fake_backend_.journeys().size(), 2u);
  EXPECT_EQ(fake_backend_.journeys().at("guid_1"),
            CreateTestJourneyRow("guid_1", "Title 1"));
  EXPECT_EQ(fake_backend_.journeys().at("guid_2"),
            CreateTestJourneyRow("guid_2", "Title 2"));
}

TEST_F(JourneysSyncBridgeTest, ApplyIncrementalSyncChangesUpdate) {
  JourneysSyncBridge bridge = CreateBridge();

  fake_backend_.AddOrUpdateJourneys(
      {CreateTestJourneyRow("guid_1", "Title 1"),
       CreateTestJourneyRow("guid_2", "Title 2")});
  ASSERT_EQ(fake_backend_.journeys().size(), 2u);

  syncer::EntityChangeList update_changes;
  update_changes.push_back(syncer::EntityChange::CreateUpdate(
      "guid_1", CreateTestJourneyEntityData("guid_1", "Updated Title 1")));

  EXPECT_EQ(std::nullopt,
            bridge.ApplyIncrementalSyncChanges(
                bridge.CreateMetadataChangeList(), std::move(update_changes)));

  EXPECT_EQ(fake_backend_.journeys().size(), 2u);
  EXPECT_EQ(fake_backend_.journeys().at("guid_1"),
            CreateTestJourneyRow("guid_1", "Updated Title 1"));
  EXPECT_EQ(fake_backend_.journeys().at("guid_2"),
            CreateTestJourneyRow("guid_2", "Title 2"));
}

TEST_F(JourneysSyncBridgeTest, ApplyIncrementalSyncChangesDelete) {
  JourneysSyncBridge bridge = CreateBridge();

  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      "guid_1", CreateTestJourneyEntityData("guid_1", "Title 1")));
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      "guid_2", CreateTestJourneyEntityData("guid_2", "Title 2")));
  EXPECT_EQ(std::nullopt,
            bridge.ApplyIncrementalSyncChanges(
                bridge.CreateMetadataChangeList(), std::move(add_changes)));
  EXPECT_EQ(fake_backend_.journeys().size(), 2u);

  syncer::EntityChangeList delete_changes;
  delete_changes.push_back(syncer::EntityChange::CreateDelete(
      "guid_1", CreateTestJourneyEntityData("guid_1")));

  EXPECT_EQ(std::nullopt,
            bridge.ApplyIncrementalSyncChanges(
                bridge.CreateMetadataChangeList(), std::move(delete_changes)));

  EXPECT_EQ(fake_backend_.journeys().size(), 1u);
  EXPECT_FALSE(fake_backend_.journeys().contains("guid_1"));
  EXPECT_TRUE(fake_backend_.journeys().contains("guid_2"));
}

TEST_F(JourneysSyncBridgeTest,
       ApplyIncrementalSyncChangesDeletionsBeforeAdditions) {
  JourneysSyncBridge bridge = CreateBridge();

  fake_backend_.AddOrUpdateJourneys(
      {CreateTestJourneyRow("guid_1", "Old Title")});
  ASSERT_EQ(fake_backend_.journeys().size(), 1u);

  // In the same change list, delete guid_1 and add updated guid_1.
  syncer::EntityChangeList changes;
  changes.push_back(syncer::EntityChange::CreateDelete(
      "guid_1", CreateTestJourneyEntityData("guid_1")));
  changes.push_back(syncer::EntityChange::CreateAdd(
      "guid_1", CreateTestJourneyEntityData("guid_1", "Re-added Title")));

  EXPECT_EQ(std::nullopt,
            bridge.ApplyIncrementalSyncChanges(
                bridge.CreateMetadataChangeList(), std::move(changes)));

  // Deletion must be executed before addition, so guid_1 is preserved with
  // the added data.
  EXPECT_EQ(fake_backend_.journeys().size(), 1u);
  ASSERT_TRUE(fake_backend_.journeys().contains("guid_1"));
  EXPECT_EQ(fake_backend_.journeys().at("guid_1").title, "Re-added Title");
}

TEST_F(JourneysSyncBridgeTest, MergeFullSyncDataAppliesRemoteData) {
  JourneysSyncBridge bridge = CreateBridge();

  syncer::EntityChangeList changes;
  changes.push_back(syncer::EntityChange::CreateAdd(
      "new_guid", CreateTestJourneyEntityData("new_guid", "New Title")));

  EXPECT_EQ(std::nullopt,
            bridge.MergeFullSyncData(bridge.CreateMetadataChangeList(),
                                     std::move(changes)));

  EXPECT_EQ(fake_backend_.journeys().size(), 1u);
  EXPECT_TRUE(fake_backend_.journeys().contains("new_guid"));
  EXPECT_EQ(fake_backend_.journeys().at("new_guid"),
            CreateTestJourneyRow("new_guid", "New Title"));
}

TEST_F(JourneysSyncBridgeTest, MergeFullSyncDataBackendError) {
  JourneysSyncBridge bridge = CreateBridge();
  fake_backend_.set_fail_operations(true);

  syncer::EntityChangeList changes;
  changes.push_back(syncer::EntityChange::CreateAdd(
      "new_guid", CreateTestJourneyEntityData("new_guid", "New Title")));

  EXPECT_THAT(
      bridge.MergeFullSyncData(bridge.CreateMetadataChangeList(),
                               std::move(changes)),
      HasModelErrorType(syncer::ModelError::Type::kJourneysDatabaseError));
}

TEST_F(JourneysSyncBridgeTest, ApplyIncrementalSyncChangesBackendError) {
  JourneysSyncBridge bridge = CreateBridge();
  fake_backend_.set_fail_operations(true);

  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      "guid_1", CreateTestJourneyEntityData("guid_1", "Title 1")));

  EXPECT_THAT(
      bridge.ApplyIncrementalSyncChanges(bridge.CreateMetadataChangeList(),
                                         std::move(add_changes)),
      HasModelErrorType(syncer::ModelError::Type::kJourneysDatabaseError));
}

TEST_F(JourneysSyncBridgeTest,
       ApplyIncrementalSyncChangesPropagatesProcessorError) {
  JourneysSyncBridge bridge = CreateBridge();
  syncer::ModelError error(FROM_HERE,
                           syncer::ModelError::Type::kJourneysDatabaseError);
  ON_CALL(mock_processor_, GetError()).WillByDefault(testing::Return(error));

  syncer::EntityChangeList entity_changes;
  entity_changes.push_back(syncer::EntityChange::CreateAdd(
      kTestJourneyId, CreateTestJourneyEntityData(kTestJourneyId)));

  EXPECT_THAT(
      bridge.ApplyIncrementalSyncChanges(bridge.CreateMetadataChangeList(),
                                         std::move(entity_changes)),
      HasModelErrorType(syncer::ModelError::Type::kJourneysDatabaseError));
}

TEST_F(JourneysSyncBridgeTest, GetAllDataForDebugging) {
  JourneysSyncBridge bridge = CreateBridge();

  fake_backend_.AddOrUpdateJourneys(
      {CreateTestJourneyRow("guid_1", "Title 1"),
       CreateTestJourneyRow("guid_2", "Title 2")});

  std::unique_ptr<syncer::DataBatch> batch = bridge.GetAllDataForDebugging();
  ASSERT_TRUE(batch);

  std::map<std::string, sync_pb::JourneySpecifics> batch_specifics;
  while (batch->HasNext()) {
    auto [key, entity_data] = batch->Next();
    ASSERT_TRUE(entity_data);
    EXPECT_EQ(entity_data->name, key);
    ASSERT_TRUE(entity_data->specifics.has_journey());
    batch_specifics[key] = entity_data->specifics.journey();
  }
  EXPECT_EQ(batch_specifics.size(), 2u);
  EXPECT_THAT(batch_specifics["guid_1"],
              EqualsProto(CreateTestJourneySpecifics("guid_1", "Title 1")));
  EXPECT_THAT(batch_specifics["guid_2"],
              EqualsProto(CreateTestJourneySpecifics("guid_2", "Title 2")));
}

TEST_F(JourneysSyncBridgeTest,
       ApplyDisableSyncChangesDeletesMetadataAndJourneys) {
  JourneysSyncBridge bridge = CreateBridge();

  fake_backend_.AddOrUpdateJourneys({CreateTestJourneyRow(kTestJourneyId)});
  EXPECT_EQ(fake_backend_.journeys().size(), 1u);

  EntityMetadata metadata;
  metadata.set_client_tag_hash("test_hash");
  EXPECT_TRUE(
      db_.UpdateEntityMetadata(syncer::JOURNEY, kTestJourneyId, metadata));

  DataTypeState data_type_state;
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  EXPECT_TRUE(db_.sync_metadata_db()->UpdateDataTypeState(syncer::JOURNEY,
                                                          data_type_state));

  bridge.ApplyDisableSyncChanges(bridge.CreateMetadataChangeList());

  syncer::MetadataBatch metadata_batch;
  EXPECT_TRUE(db_.GetAllSyncMetadata(&metadata_batch));
  EXPECT_EQ(metadata_batch.GetAllMetadata().size(), 0u);
  EXPECT_THAT(metadata_batch.GetDataTypeState(), EqualsProto(DataTypeState()));
  EXPECT_EQ(fake_backend_.journeys().size(), 0u);
}

TEST_F(JourneysSyncBridgeTest, LoadMetadataReportsErrorOnCorruptedData) {
  EXPECT_TRUE(db_.db()->Execute(
      "INSERT INTO journey_sync_metadata (storage_key, value) "
      "VALUES('invalid_key', 'not_a_valid_serialized_proto')"));

  EXPECT_CALL(
      mock_processor_,
      ReportError(Property(&ModelError::type,
                           ModelError::Type::kJourneysFailedToLoadMetadata)));
  EXPECT_CALL(mock_processor_, ModelReadyToSync(_)).Times(0);

  JourneysSyncBridge bridge = CreateBridge();
}

TEST_F(JourneysSyncBridgeTest, OnDatabaseErrorReportsError) {
  JourneysSyncBridge bridge = CreateBridge();

  EXPECT_CALL(mock_processor_,
              ReportError(Property(&ModelError::type,
                                   ModelError::Type::kJourneysDatabaseError)));

  bridge.OnDatabaseError();
}

TEST_F(JourneysSyncBridgeTest, OnHistoryDeletionsClearsMetadataOnAllHistory) {
  JourneysSyncBridge bridge = CreateBridge();

  EntityMetadata metadata;
  metadata.set_client_tag_hash("test_hash");
  EXPECT_TRUE(
      db_.UpdateEntityMetadata(syncer::JOURNEY, kTestJourneyId, metadata));

  ON_CALL(mock_processor_, GetAllTrackedStorageKeys())
      .WillByDefault(Return(std::vector<std::string>{kTestJourneyId}));

  EXPECT_CALL(mock_processor_, UntrackEntityForStorageKey(kTestJourneyId));

  fake_backend_.NotifyHistoryDeletions(/*all_history=*/true);

  syncer::MetadataBatch metadata_batch;
  EXPECT_TRUE(db_.GetAllSyncMetadata(&metadata_batch));
  EXPECT_EQ(metadata_batch.GetAllMetadata().size(), 0u);
}

TEST_F(JourneysSyncBridgeTest, OnHistoryDeletionsIgnoresPartialHistory) {
  JourneysSyncBridge bridge = CreateBridge();

  EntityMetadata metadata;
  metadata.set_client_tag_hash("test_hash");
  EXPECT_TRUE(
      db_.UpdateEntityMetadata(syncer::JOURNEY, kTestJourneyId, metadata));

  EXPECT_CALL(mock_processor_, UntrackEntityForStorageKey(testing::_)).Times(0);

  fake_backend_.NotifyHistoryDeletions(/*all_history=*/false);

  syncer::MetadataBatch metadata_batch;
  EXPECT_TRUE(db_.GetAllSyncMetadata(&metadata_batch));
  EXPECT_EQ(metadata_batch.GetAllMetadata().size(), 1u);
}

}  // namespace
}  // namespace history::journeys
