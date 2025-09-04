// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/shared_tab_group_data_sync_bridge.h"

#include <algorithm>
#include <array>
#include <initializer_list>
#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/data_sharing/public/logger.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/testing_pref_service.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model_observer.h"
#include "components/saved_tab_groups/internal/sync_bridge_tab_group_model_wrapper.h"
#include "components/saved_tab_groups/proto/shared_tab_group_data.pb.h"
#include "components/saved_tab_groups/public/pref_names.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/public/utils.h"
#include "components/saved_tab_groups/test_support/extended_shared_tab_group_data_specifics.pb.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/protocol/collaboration_metadata.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/shared_tab_group_data_specifics.pb.h"
#include "components/sync/protocol/unique_position.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/tab_groups/tab_group_color.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {

// PrintTo must be defined in the same namespace as the SavedTabGroupTab class.
void PrintTo(const SavedTabGroupTab& tab, std::ostream* os) {
  *os << "(title: " << tab.title() << ", url: " << tab.url() << ")";
}
namespace {

using base::test::EqualsProto;
using syncer::CollaborationId;
using tab_groups::test::HasSharedGroupMetadata;
using tab_groups::test::HasTabMetadata;
using testing::_;
using testing::AllOf;
using testing::Each;
using testing::ElementsAre;
using testing::Eq;
using testing::InvokeWithoutArgs;
using testing::IsEmpty;
using testing::IsNull;
using testing::Not;
using testing::NotNull;
using testing::Pair;
using testing::Pointee;
using testing::Property;
using testing::Return;
using testing::ReturnRef;
using testing::Sequence;
using testing::SizeIs;
using testing::UnorderedElementsAre;
using testing::WithArg;

constexpr GaiaId::Literal kDefaultGaiaId("1234567890");

// Returns the extra (unsupported) field from `specifics` which don't have a
// corresponding field in proto.
std::string GetGroupExtraFieldFromSpecifics(
    const sync_pb::SharedTabGroupDataSpecifics& specifics) {
  sync_pb::test_utils::SharedTabGroupDataSpecifics extended_specifics;
  bool success =
      extended_specifics.ParseFromString(specifics.SerializeAsString());
  CHECK(success);
  return extended_specifics.tab_group().extra_field_for_testing();
}

// Creator is not verified for the local changes because this field is not used
// in the processor for updates.
MATCHER_P(HasAttributionMetadata, updated_by, "") {
  const std::optional<syncer::CollaborationMetadata>& collaboration_metadata =
      arg.collaboration_metadata;
  return collaboration_metadata.has_value() &&
         collaboration_metadata->last_updated_by() == GaiaId(updated_by);
}

MATCHER_P(HasLocalGroupId, local_group_id, "") {
  return arg.local_group_id() == local_group_id;
}

MATCHER_P(HasTabUrl, url, "") {
  return arg.url() == GURL(url);
}

MATCHER_P2(HasSharedAttribution, created_by, updated_by, "") {
  return arg.shared_attribution().created_by == GaiaId(created_by) &&
         arg.shared_attribution().updated_by == GaiaId(updated_by);
}

MATCHER_P3(HasGroupEntityData, title, color, collaboration_id, "") {
  const sync_pb::SharedTabGroupDataSpecifics& arg_specifics =
      arg.specifics.shared_tab_group_data();
  const sync_pb::SharedTabGroup& arg_tab_group = arg_specifics.tab_group();
  const std::optional<syncer::CollaborationMetadata>& collab_metadata =
      arg.collaboration_metadata;
  return arg_specifics.version() ==
             kCurrentSharedTabGroupDataSpecificsProtoVersion &&
         arg_tab_group.title() == title && arg_tab_group.color() == color &&
         collab_metadata.has_value() &&
         CollaborationId(collab_metadata->collaboration_id()) ==
             CollaborationId(collaboration_id);
}

MATCHER_P(EntityDataHasGroupUnsupportedFields, extra_field, "") {
  const sync_pb::SharedTabGroupDataSpecifics& arg_specifics =
      arg.specifics.shared_tab_group_data();
  return GetGroupExtraFieldFromSpecifics(arg_specifics) == extra_field;
}

MATCHER_P(GroupSpecificsHasUnsupportedField, extra_field, "") {
  return GetGroupExtraFieldFromSpecifics(arg) == extra_field;
}

MATCHER_P(HasCreationTime, time, "") {
  return arg.creation_time == time;
}

MATCHER_P(HasGroupEntityDataWithOriginatingGroup, originating_group_guid, "") {
  const sync_pb::SharedTabGroup& arg_tab_group =
      arg.specifics.shared_tab_group_data().tab_group();
  return arg_tab_group.originating_tab_group_guid() ==
         originating_group_guid.AsLowercaseString();
}

MATCHER(EntityDataHasOriginatingGroup, "") {
  const sync_pb::SharedTabGroup& arg_tab_group =
      arg.specifics.shared_tab_group_data().tab_group();
  return arg_tab_group.has_originating_tab_group_guid();
}

MATCHER_P3(HasTabEntityData, title, url, collaboration_id, "") {
  const sync_pb::SharedTabGroupDataSpecifics& arg_specifics =
      arg.specifics.shared_tab_group_data();
  const sync_pb::SharedTab& arg_tab = arg_specifics.tab();
  const std::optional<syncer::CollaborationMetadata>& collab_metadata =
      arg.collaboration_metadata;
  return arg_specifics.version() ==
             kCurrentSharedTabGroupDataSpecificsProtoVersion &&
         arg_tab.title() == title && arg_tab.url() == url &&
         collab_metadata.has_value() &&
         CollaborationId(collab_metadata->collaboration_id()) ==
             CollaborationId(collaboration_id);
}

MATCHER_P4(HasTabEntityDataWithVersion,
           title,
           url,
           collaboration_id,
           version,
           "") {
  const sync_pb::SharedTabGroupDataSpecifics& arg_specifics =
      arg.specifics.shared_tab_group_data();
  const sync_pb::SharedTab& arg_tab = arg_specifics.tab();
  const std::optional<syncer::CollaborationMetadata>& collab_metadata =
      arg.collaboration_metadata;
  return arg_specifics.version() == version && arg_tab.title() == title &&
         arg_tab.url() == url && collab_metadata.has_value() &&
         CollaborationId(collab_metadata->collaboration_id()) ==
             CollaborationId(collaboration_id);
}

MATCHER_P2(HasTabEntityDataWithPosition, title, unique_position, "") {
  const sync_pb::SharedTab& arg_tab =
      arg.specifics.shared_tab_group_data().tab();
  return arg_tab.title() == title &&
         testing::Matcher<decltype(unique_position)>(
             EqualsProto(unique_position))
             .MatchAndExplain(arg_tab.unique_position(), result_listener);
}

std::string StorageKeyForTab(const SavedTabGroupTab& tab) {
  return tab.saved_tab_guid().AsLowercaseString();
}

std::string StorageKeyForGroup(const SavedTabGroup& group) {
  return group.saved_guid().AsLowercaseString();
}

MATCHER_P(HasClientTagHashForTab, tab, "") {
  return arg == syncer::ClientTagHash::FromUnhashed(
                    syncer::SHARED_TAB_GROUP_DATA, StorageKeyForTab(tab));
}

class MockTabGroupModelObserver : public SavedTabGroupModelObserver {
 public:
  MockTabGroupModelObserver() = default;

  void ObserveModel(SavedTabGroupModel* model) { observation_.Observe(model); }
  void Reset() { observation_.Reset(); }

  MOCK_METHOD(void, SavedTabGroupAddedFromSync, (const base::Uuid&));
  MOCK_METHOD(void, SavedTabGroupRemovedFromSync, (const SavedTabGroup&));
  MOCK_METHOD(void,
              SavedTabGroupUpdatedFromSync,
              (const base::Uuid&, const std::optional<base::Uuid>&));
  MOCK_METHOD(void,
              SavedTabGroupUpdatedLocally,
              (const base::Uuid&, const std::optional<base::Uuid>&));
  MOCK_METHOD(void, OnSyncBridgeUpdateTypeChanged, (SyncBridgeUpdateType));

 private:
  base::ScopedObservation<SavedTabGroupModel, SavedTabGroupModelObserver>
      observation_{this};
};

// Forwards SavedTabGroupModel's observer notifications to the bridge.
class ModelObserverForwarder : public SavedTabGroupModelObserver {
 public:
  ModelObserverForwarder(SavedTabGroupModel& model,
                         SharedTabGroupDataSyncBridge& bridge)
      : model_(model), bridge_(bridge) {
    observation_.Observe(&model);
  }

  ~ModelObserverForwarder() override = default;

  // SavedTabGroupModelObserver overrides.
  void SavedTabGroupAddedLocally(const base::Uuid& guid) override {
    bridge_->SavedTabGroupAddedLocally(guid);
  }

  void SavedTabGroupRemovedLocally(
      const SavedTabGroup& removed_group) override {
    bridge_->SavedTabGroupRemovedLocally(removed_group);
  }

  void SavedTabGroupUpdatedLocally(
      const base::Uuid& group_guid,
      const std::optional<base::Uuid>& tab_guid) override {
    bridge_->SavedTabGroupUpdatedLocally(group_guid, tab_guid);
  }

  void SavedTabGroupTabMovedLocally(const base::Uuid& group_guid,
                                    const base::Uuid& tab_guid) override {
    bridge_->SavedTabGroupUpdatedLocally(group_guid, tab_guid);
  }

  void SavedTabGroupLocalIdChanged(const base::Uuid& group_guid) override {
    bridge_->ProcessTabGroupLocalIdChanged(group_guid);
  }

 private:
  raw_ref<SavedTabGroupModel> model_;
  raw_ref<SharedTabGroupDataSyncBridge> bridge_;

  base::ScopedObservation<SavedTabGroupModel, SavedTabGroupModelObserver>
      observation_{this};
};

sync_pb::SharedTabGroupDataSpecifics MakeTabGroupSpecifics(
    const std::string& title,
    sync_pb::SharedTabGroup::Color color) {
  sync_pb::SharedTabGroupDataSpecifics specifics;
  specifics.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  sync_pb::SharedTabGroup* tab_group = specifics.mutable_tab_group();
  tab_group->set_title(title);
  tab_group->set_color(color);
  return specifics;
}

sync_pb::SharedTabGroupDataSpecifics MakeTabGroupSpecificsWithUnknownFields(
    const std::string& title,
    sync_pb::SharedTabGroup::Color color,
    const base::Uuid& originating_group_id,
    const std::string& extra_field) {
  sync_pb::SharedTabGroupDataSpecifics specifics =
      MakeTabGroupSpecifics(title, color);
  sync_pb::test_utils::SharedTabGroupDataSpecifics extended_specifics;
  extended_specifics.mutable_tab_group()->set_extra_field_for_testing(
      extra_field);
  sync_pb::SharedTabGroupDataSpecifics specifics_with_unknown_fields;
  bool success = specifics_with_unknown_fields.ParseFromString(
      extended_specifics.SerializeAsString());
  CHECK(success);
  specifics.MergeFrom(specifics_with_unknown_fields);
  return specifics;
}

sync_pb::SharedTabGroupDataSpecifics MakeTabSpecifics(
    const std::string& title,
    const GURL& url,
    const base::Uuid& group_id,
    const syncer::UniquePosition& unique_position) {
  sync_pb::SharedTabGroupDataSpecifics specifics;
  specifics.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  sync_pb::SharedTab* pb_tab = specifics.mutable_tab();
  pb_tab->set_url(url.spec());
  pb_tab->set_title(title);
  pb_tab->set_shared_tab_group_guid(group_id.AsLowercaseString());
  *pb_tab->mutable_unique_position() = unique_position.ToProto();
  return specifics;
}

syncer::EntityData CreateEntityData(
    const sync_pb::SharedTabGroupDataSpecifics& specifics,
    const CollaborationId& collaboration_id,
    const GaiaId& created_by,
    const GaiaId& updated_by,
    base::Time creation_time = base::Time::Now(),
    base::Time modification_time = base::Time::Now()) {
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_shared_tab_group_data() = specifics;
  sync_pb::SyncEntity::CollaborationMetadata collaboration_metadata_proto;
  collaboration_metadata_proto.set_collaboration_id(collaboration_id.value());
  collaboration_metadata_proto.mutable_creation_attribution()
      ->set_obfuscated_gaia_id(created_by.ToString());
  collaboration_metadata_proto.mutable_last_update_attribution()
      ->set_obfuscated_gaia_id(updated_by.ToString());
  entity_data.collaboration_metadata =
      syncer::CollaborationMetadata::FromRemoteProto(
          collaboration_metadata_proto);
  entity_data.name = specifics.guid();
  entity_data.creation_time = creation_time;
  entity_data.modification_time = modification_time;
  return entity_data;
}

std::unique_ptr<syncer::EntityChange> CreateAddEntityChange(
    const sync_pb::SharedTabGroupDataSpecifics& specifics,
    const CollaborationId& collaboration_id,
    base::Time creation_time = base::Time::Now(),
    base::Time modification_time = base::Time::Now()) {
  const std::string& storage_key = specifics.guid();
  return syncer::EntityChange::CreateAdd(
      storage_key, CreateEntityData(specifics, collaboration_id, kDefaultGaiaId,
                                    /*updated_by=*/kDefaultGaiaId,
                                    creation_time, modification_time));
}

std::unique_ptr<syncer::EntityChange> CreateUpdateEntityChange(
    const sync_pb::SharedTabGroupDataSpecifics& specifics,
    const CollaborationId& collaboration_id,
    base::Time creation_time = base::Time::Now(),
    base::Time modification_time = base::Time::Now()) {
  const std::string& storage_key = specifics.guid();
  return syncer::EntityChange::CreateUpdate(
      storage_key, CreateEntityData(specifics, collaboration_id, kDefaultGaiaId,
                                    /*updated_by=*/kDefaultGaiaId,
                                    creation_time, modification_time));
}

std::unique_ptr<syncer::EntityChange> CreateDeleteEntityChange(
    const std::string& storage_key) {
  return syncer::EntityChange::CreateDelete(storage_key, syncer::EntityData());
}

std::vector<syncer::EntityData> ExtractEntityDataFromBatch(
    std::unique_ptr<syncer::DataBatch> batch) {
  std::vector<syncer::EntityData> result;
  while (batch->HasNext()) {
    const syncer::KeyAndData& data_pair = batch->Next();
    result.push_back(std::move(*data_pair.second));
  }
  return result;
}

sync_pb::EntityMetadata CreateMetadata(
    CollaborationId collaboration_id,
    const SharedAttribution& shared_attribution,
    std::optional<sync_pb::UniquePosition> unique_position) {
  sync_pb::EntityMetadata metadata;
  // Other metadata fields are not used in these tests.
  metadata.mutable_collaboration()->set_collaboration_id(
      std::move(collaboration_id.value()));
  metadata.mutable_collaboration()
      ->mutable_creation_attribution()
      ->set_obfuscated_gaia_id(shared_attribution.created_by.ToString());
  metadata.mutable_collaboration()
      ->mutable_last_update_attribution()
      ->set_obfuscated_gaia_id(shared_attribution.updated_by.ToString());
  if (unique_position) {
    *metadata.mutable_unique_position() = std::move(unique_position.value());
  }
  return metadata;
}

syncer::UniquePosition GenerateRandomUniquePosition() {
  return syncer::UniquePosition::InitialPosition(
      syncer::UniquePosition::RandomSuffix());
}

class SharedTabGroupDataSyncBridgeTest : public testing::Test {
 public:
  SharedTabGroupDataSyncBridgeTest()
      : store_(syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kDidEnableSharedTabGroupsInLastSession, true);
  }

  // Creates the bridges and initializes the model. Returns true when succeeds.
  bool InitializeBridgeAndModel() {
    ON_CALL(processor_, IsTrackingMetadata()).WillByDefault(Return(true));
    ON_CALL(processor_, GetPossiblyTrimmedRemoteSpecifics(_))
        .WillByDefault(ReturnRef(sync_pb::EntitySpecifics::default_instance()));

    CHECK(!saved_tab_group_model_) << "InitializeBridgeAndModel must not be "
                                      "called when the model is initialized";
    saved_tab_group_model_ = std::make_unique<SavedTabGroupModel>();
    sync_bridge_model_wrapper_ =
        std::make_unique<SyncBridgeTabGroupModelWrapper>(
            syncer::SHARED_TAB_GROUP_DATA, saved_tab_group_model_.get(),
            base::BindOnce(&SavedTabGroupModel::LoadStoredEntries,
                           base::Unretained(saved_tab_group_model_.get())));
    mock_model_observer_.ObserveModel(saved_tab_group_model_.get());

    bridge_ = std::make_unique<SharedTabGroupDataSyncBridge>(
        sync_bridge_model_wrapper_.get(),
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        processor_.CreateForwardingProcessor(), &pref_service_,
        /*logger=*/nullptr);
    observer_forwarder_ = std::make_unique<ModelObserverForwarder>(
        *saved_tab_group_model_, *bridge_);
    task_environment_.RunUntilIdle();

    return saved_tab_group_model_->is_loaded();
  }

  // Cleans up the bridge and the model, used to simulate browser restart.
  void StoreMetadataAndReset() {
    CHECK(saved_tab_group_model_);
    StoreSharedSyncMetadataBasedOnModel();

    observer_forwarder_.reset();
    mock_model_observer_.Reset();
    bridge_.reset();
    sync_bridge_model_wrapper_.reset();
    saved_tab_group_model_.reset();
  }

  size_t GetNumEntriesInStore() {
    return syncer::DataTypeStoreTestUtil::ReadAllDataAndWait(store()).size();
  }

  std::map<std::string, proto::SharedTabGroupData> GetAllLocalDataFromStore() {
    return syncer::DataTypeStoreTestUtil::ReadAllDataAsProtoAndWait<
        proto::SharedTabGroupData>(store());
  }

  // Generates and mocks unique positions for all the tabs in the `group`.
  void GenerateUniquePositionsForTabsInGroup(const SavedTabGroup& group) {
    syncer::UniquePosition unique_position = GenerateRandomUniquePosition();
    for (const SavedTabGroupTab& tab : group.saved_tabs()) {
      ON_CALL(mock_processor(),
              GetUniquePositionForStorageKey(StorageKeyForTab(tab)))
          .WillByDefault(Return(unique_position.ToProto()));
      unique_position = syncer::UniquePosition::After(
          unique_position, syncer::UniquePosition::RandomSuffix());
    }
  }

  // Updates and mocks unique position for `tab_specifics`. This is helpful to
  // simulate the order of applying updates: first, sync metadata is stored in
  // the processor (with unique position), then updates are applied.
  void UpdateAndMockUniquePositionForTabSpecifics(
      sync_pb::SharedTabGroupDataSpecifics& tab_specifics,
      sync_pb::UniquePosition new_unique_position) {
    CHECK(tab_specifics.has_tab());
    *tab_specifics.mutable_tab()->mutable_unique_position() =
        std::move(new_unique_position);
    ON_CALL(mock_processor(),
            GetUniquePositionForStorageKey(tab_specifics.guid()))
        .WillByDefault(Return(tab_specifics.tab().unique_position()));
  }

  // Returns last mocked unique position for `tab`.
  syncer::UniquePosition GetMockedUniquePosition(
      const SavedTabGroupTab& tab) const {
    return syncer::UniquePosition::FromProto(
        mock_processor().GetUniquePositionForStorageKey(StorageKeyForTab(tab)));
  }

  // Generates a unique position before the unique position of `tab`.
  syncer::UniquePosition GenerateUniquePositionBefore(
      const SavedTabGroupTab& tab) const {
    return syncer::UniquePosition::Before(
        GetMockedUniquePosition(tab), syncer::UniquePosition::RandomSuffix());
  }

  // Generates a unique position after the unique position of `tab`.
  syncer::UniquePosition GenerateUniquePositionAfter(
      const SavedTabGroupTab& tab) const {
    return syncer::UniquePosition::After(
        GetMockedUniquePosition(tab), syncer::UniquePosition::RandomSuffix());
  }

  // Generates a unique position between the unique positions of tabs.
  syncer::UniquePosition GenerateUniquePositionBetween(
      const SavedTabGroupTab& tab_before,
      const SavedTabGroupTab& tab_after) const {
    return syncer::UniquePosition::Between(
        GetMockedUniquePosition(tab_before), GetMockedUniquePosition(tab_after),
        syncer::UniquePosition::RandomSuffix());
  }

  // Applies remote incremental update with a single change.
  std::optional<syncer::ModelError> ApplySingleEntityChange(
      std::unique_ptr<syncer::EntityChange> entity_change) {
    syncer::EntityChangeList change_list;
    change_list.push_back(std::move(entity_change));
    return bridge()->ApplyIncrementalSyncChanges(
        bridge()->CreateMetadataChangeList(), std::move(change_list));
  }

  sync_pb::UniquePosition GetMockedUniquePosition(
      const std::string& storage_key) {
    return mock_processor().GetUniquePositionForStorageKey(storage_key);
  }

  // Generates specifics for the given tab update using the current mocked
  // unique position.
  sync_pb::SharedTabGroupDataSpecifics GenerateTabUpdateSpecifics(
      const SavedTabGroupTab& tab,
      const std::string& new_title) {
    sync_pb::UniquePosition tab_unique_position =
        GetMockedUniquePosition(StorageKeyForTab(tab));
    sync_pb::SharedTabGroupDataSpecifics tab_update_specifics =
        MakeTabSpecifics(
            new_title, tab.url(), tab.saved_group_guid(),
            syncer::UniquePosition::FromProto(tab_unique_position));
    tab_update_specifics.set_guid(tab.saved_tab_guid().AsLowercaseString());
    return tab_update_specifics;
  }

  // Stores sync metadata for the shared tab groups from the current model. This
  // is helpful to verify storing data across browser restarts.
  void StoreSharedSyncMetadataBasedOnModel() {
    std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
        store().CreateWriteBatch();
    syncer::MetadataChangeList* metadata_change_list =
        write_batch->GetMetadataChangeList();
    for (const SavedTabGroup* group : model()->GetSharedTabGroupsOnly()) {
      CHECK(group->collaboration_id().has_value());
      metadata_change_list->UpdateMetadata(
          group->saved_guid().AsLowercaseString(),
          CreateMetadata(group->collaboration_id().value(),
                         group->shared_attribution(),
                         /*unique_position=*/std::nullopt));
      syncer::UniquePosition next_unique_position =
          GenerateRandomUniquePosition();
      for (const SavedTabGroupTab& tab : group->saved_tabs()) {
        metadata_change_list->UpdateMetadata(
            tab.saved_tab_guid().AsLowercaseString(),
            CreateMetadata(group->collaboration_id().value(),
                           tab.shared_attribution(),
                           next_unique_position.ToProto()));
        next_unique_position = syncer::UniquePosition::After(
            next_unique_position, syncer::UniquePosition::RandomSuffix());
      }
    }
    store().CommitWriteBatch(std::move(write_batch), base::DoNothing());
  }

  // Stores sync metadata for the tab. Used to store metadata for tabs missing
  // their group (which are not present in the model).
  void StoreMetadataForTabSpecifics(
      const sync_pb::SharedTabGroupDataSpecifics& tab_specifics,
      const syncer::CollaborationId& collaboration_id) {
    std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
        store().CreateWriteBatch();
    syncer::MetadataChangeList* metadata_change_list =
        write_batch->GetMetadataChangeList();
    metadata_change_list->UpdateMetadata(
        tab_specifics.guid(),
        CreateMetadata(collaboration_id, SharedAttribution(),
                       tab_specifics.tab().unique_position()));
    store().CommitWriteBatch(std::move(write_batch), base::DoNothing());
  }

  SharedTabGroupDataSyncBridge* bridge() { return bridge_.get(); }
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor>&
  mock_processor() {
    return processor_;
  }
  const testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor>&
  mock_processor() const {
    return processor_;
  }
  SavedTabGroupModel* model() { return saved_tab_group_model_.get(); }
  testing::NiceMock<MockTabGroupModelObserver>& mock_model_observer() {
    return mock_model_observer_;
  }
  syncer::DataTypeStore& store() { return *store_; }

 protected:
  // In memory data type store needs to be able to post tasks.
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<SavedTabGroupModel> saved_tab_group_model_;
  std::unique_ptr<SyncBridgeTabGroupModelWrapper> sync_bridge_model_wrapper_;
  testing::NiceMock<MockTabGroupModelObserver> mock_model_observer_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> processor_;
  std::unique_ptr<syncer::DataTypeStore> store_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<SharedTabGroupDataSyncBridge> bridge_;
  std::unique_ptr<ModelObserverForwarder> observer_forwarder_;
};

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldReturnClientTag) {
  const CollaborationId kCollaborationId("collaboration");
  ASSERT_TRUE(InitializeBridgeAndModel());

  EXPECT_TRUE(bridge()->SupportsGetClientTag());
  sync_pb::SharedTabGroupDataSpecifics group_specifics =
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE);
  EXPECT_EQ(bridge()->GetClientTag(CreateEntityData(
                group_specifics, kCollaborationId, kDefaultGaiaId,
                /*updated_by=*/kDefaultGaiaId)),
            group_specifics.guid() + "|" + kCollaborationId.value());
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldCallModelReadyToSync) {
  EXPECT_CALL(mock_processor(), ModelReadyToSync).WillOnce([]() {});

  // This already invokes RunUntilIdle, so the call above is expected to happen.
  ASSERT_TRUE(InitializeBridgeAndModel());
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldAddRemoteGroupsAtInitialSync) {
  const CollaborationId kCollaborationId1("collaboration 1");
  const CollaborationId kCollaborationId2("collaboration 2");
  ASSERT_TRUE(InitializeBridgeAndModel());

  syncer::EntityChangeList change_list;
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE),
      kCollaborationId1));
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title 2", sync_pb::SharedTabGroup::RED),
      kCollaborationId2));

  Sequence s;
  EXPECT_CALL(
      mock_model_observer(),
      OnSyncBridgeUpdateTypeChanged(Eq(SyncBridgeUpdateType::kInitialMerge)))
      .InSequence(s);
  EXPECT_CALL(mock_model_observer(),
              OnSyncBridgeUpdateTypeChanged(
                  Eq(SyncBridgeUpdateType::kCompletedInitialMergeThisSession)))
      .InSequence(s);
  bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                              std::move(change_list));

  EXPECT_THAT(
      model()->saved_tab_groups(),
      UnorderedElementsAre(
          HasSharedGroupMetadata("title", tab_groups::TabGroupColorId::kBlue,
                                 kCollaborationId1),
          HasSharedGroupMetadata("title 2", tab_groups::TabGroupColorId::kRed,
                                 kCollaborationId2)));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldAddRemoteTabsAtInitialSync) {
  const CollaborationId kCollaborationId("collaboration");
  ASSERT_TRUE(InitializeBridgeAndModel());

  sync_pb::SharedTabGroupDataSpecifics group_specifics =
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE);
  const base::Uuid group_id =
      base::Uuid::ParseLowercase(group_specifics.guid());

  syncer::EntityChangeList change_list;
  change_list.push_back(
      CreateAddEntityChange(group_specifics, kCollaborationId));
  change_list.push_back(CreateAddEntityChange(
      MakeTabSpecifics("tab title 1", GURL("https://google.com/1"), group_id,
                       GenerateRandomUniquePosition()),
      kCollaborationId));
  change_list.push_back(CreateAddEntityChange(
      MakeTabSpecifics("tab title 2", GURL("https://google.com/2"), group_id,
                       GenerateRandomUniquePosition()),
      kCollaborationId));

  bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                              std::move(change_list));
  ASSERT_THAT(
      model()->saved_tab_groups(),
      ElementsAre(HasSharedGroupMetadata(
          "title", tab_groups::TabGroupColorId::kBlue, kCollaborationId)));

  // Expect both tabs to be a part of the group.
  EXPECT_THAT(model()->saved_tab_groups().front().saved_tabs(),
              UnorderedElementsAre(
                  HasTabMetadata("tab title 1", "https://google.com/1"),
                  HasTabMetadata("tab title 2", "https://google.com/2")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, AddRemoteTabsWithUnsupportedURL) {
  const CollaborationId kCollaborationId("collaboration");
  ASSERT_TRUE(InitializeBridgeAndModel());

  sync_pb::SharedTabGroupDataSpecifics group_specifics =
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE);
  const base::Uuid group_id =
      base::Uuid::ParseLowercase(group_specifics.guid());

  syncer::EntityChangeList change_list;
  change_list.push_back(
      CreateAddEntityChange(group_specifics, kCollaborationId));
  change_list.push_back(CreateAddEntityChange(
      MakeTabSpecifics("tab title 1", GURL("https://google.com/1"), group_id,
                       GenerateRandomUniquePosition()),
      kCollaborationId));
  change_list.push_back(CreateAddEntityChange(
      MakeTabSpecifics("xyz", GURL("chrome://crash"), group_id,
                       GenerateRandomUniquePosition()),
      kCollaborationId));

  bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                              std::move(change_list));

  // Expect both tabs to be a part of the group.
  EXPECT_THAT(model()->saved_tab_groups().front().saved_tabs(),
              UnorderedElementsAre(
                  HasTabMetadata("tab title 1", "https://google.com/1"),
                  HasTabMetadata("", kChromeSavedTabGroupUnsupportedURL)));
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldAddRemoteGroupsAtIncrementalUpdate) {
  const CollaborationId kCollaborationId1("collaboration 1");
  const CollaborationId kCollaborationId2("collaboration 2");
  ASSERT_TRUE(InitializeBridgeAndModel());

  syncer::EntityChangeList change_list;
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE),
      kCollaborationId1));
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title 2", sync_pb::SharedTabGroup::RED),
      kCollaborationId2));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(change_list));

  EXPECT_THAT(
      model()->saved_tab_groups(),
      UnorderedElementsAre(
          HasSharedGroupMetadata("title", tab_groups::TabGroupColorId::kBlue,
                                 kCollaborationId1),
          HasSharedGroupMetadata("title 2", tab_groups::TabGroupColorId::kRed,
                                 kCollaborationId2)));
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldAddRemoteTabsAtIncrementalUpdate) {
  const CollaborationId kCollaborationId("collaboration");
  ASSERT_TRUE(InitializeBridgeAndModel());

  sync_pb::SharedTabGroupDataSpecifics group_specifics =
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE);
  const base::Uuid kGroupId =
      base::Uuid::ParseLowercase(group_specifics.guid());

  syncer::EntityChangeList change_list;
  change_list.push_back(
      CreateAddEntityChange(group_specifics, kCollaborationId));
  change_list.push_back(CreateAddEntityChange(
      MakeTabSpecifics("tab title 1", GURL("https://google.com/1"), kGroupId,
                       GenerateRandomUniquePosition()),
      kCollaborationId));
  change_list.push_back(CreateAddEntityChange(
      MakeTabSpecifics("tab title 2", GURL("https://google.com/2"), kGroupId,
                       GenerateRandomUniquePosition()),
      kCollaborationId));

  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(change_list));
  ASSERT_THAT(
      model()->saved_tab_groups(),
      ElementsAre(HasSharedGroupMetadata(
          "title", tab_groups::TabGroupColorId::kBlue, kCollaborationId)));

  // Expect both tabs to be a part of the group.
  EXPECT_THAT(model()->saved_tab_groups().front().saved_tabs(),
              UnorderedElementsAre(
                  HasTabMetadata("tab title 1", "https://google.com/1"),
                  HasTabMetadata("tab title 2", "https://google.com/2")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldUpdateExistingGroupOnIncrementalUpdate) {
  const CollaborationId kCollaborationId1("collaboration 1");
  const CollaborationId kCollaborationId2("collaboration 2");
  ASSERT_TRUE(InitializeBridgeAndModel());

  sync_pb::SharedTabGroupDataSpecifics group_specifics =
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE);
  syncer::EntityChangeList change_list;
  change_list.push_back(
      CreateAddEntityChange(group_specifics, kCollaborationId1));
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title 2", sync_pb::SharedTabGroup::RED),
      kCollaborationId2));
  bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                              std::move(change_list));
  ASSERT_EQ(model()->Count(), 2);

  group_specifics.mutable_tab_group()->set_title("updated title");
  group_specifics.mutable_tab_group()->set_color(sync_pb::SharedTabGroup::CYAN);
  ApplySingleEntityChange(
      CreateUpdateEntityChange(group_specifics, kCollaborationId1));

  EXPECT_THAT(
      model()->saved_tab_groups(),
      UnorderedElementsAre(
          HasSharedGroupMetadata("updated title",
                                 tab_groups::TabGroupColorId::kCyan,
                                 kCollaborationId1),
          HasSharedGroupMetadata("title 2", tab_groups::TabGroupColorId::kRed,
                                 kCollaborationId2)));
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldUpdateExistingTabOnIncrementalUpdate) {
  const CollaborationId kCollaborationId("collaboration");
  ASSERT_TRUE(InitializeBridgeAndModel());

  sync_pb::SharedTabGroupDataSpecifics group_specifics =
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE);
  const base::Uuid group_id =
      base::Uuid::ParseLowercase(group_specifics.guid());

  sync_pb::SharedTabGroupDataSpecifics tab_to_update_specifics =
      MakeTabSpecifics("tab title 1", GURL("https://google.com/1"), group_id,
                       GenerateRandomUniquePosition());

  syncer::EntityChangeList change_list;
  change_list.push_back(
      CreateAddEntityChange(group_specifics, kCollaborationId));
  change_list.push_back(
      CreateAddEntityChange(tab_to_update_specifics, kCollaborationId));
  change_list.push_back(CreateAddEntityChange(
      MakeTabSpecifics("tab title 2", GURL("https://google.com/2"),
                       /*group_id=*/group_id, GenerateRandomUniquePosition()),
      kCollaborationId));

  bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                              std::move(change_list));
  ASSERT_EQ(model()->Count(), 1);
  ASSERT_THAT(model()->saved_tab_groups().front().saved_tabs(), SizeIs(2));

  tab_to_update_specifics.mutable_tab()->set_title("updated title");
  ApplySingleEntityChange(
      CreateUpdateEntityChange(tab_to_update_specifics, kCollaborationId));

  ASSERT_EQ(model()->Count(), 1);
  EXPECT_THAT(model()->saved_tab_groups().front().saved_tabs(),
              UnorderedElementsAre(
                  HasTabMetadata("updated title", "https://google.com/1"),
                  HasTabMetadata("tab title 2", "https://google.com/2")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldDeleteExistingGroupOnIncrementalUpdate) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group_to_delete(u"title", tab_groups::TabGroupColorId::kBlue,
                                /*urls=*/{}, /*position=*/std::nullopt);
  group_to_delete.SetCollaborationId(CollaborationId("collaboration"));
  group_to_delete.AddTabLocally(SavedTabGroupTab(
      GURL("https://website.com"), u"Website Title",
      group_to_delete.saved_guid(), /*position=*/std::nullopt));
  model()->AddedLocally(group_to_delete);
  model()->AddedLocally(
      SavedTabGroup(u"title 2", tab_groups::TabGroupColorId::kGrey,
                    /*urls=*/{}, /*position=*/std::nullopt)
          .SetCollaborationId(CollaborationId("collaboration 2")));
  ASSERT_EQ(model()->Count(), 2);

  ApplySingleEntityChange(CreateDeleteEntityChange(
      group_to_delete.saved_guid().AsLowercaseString()));

  EXPECT_THAT(
      model()->saved_tab_groups(),
      UnorderedElementsAre(HasSharedGroupMetadata(
          "title 2", tab_groups::TabGroupColorId::kGrey, "collaboration 2")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldDeleteExistingTabOnIncrementalUpdate) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"group title", tab_groups::TabGroupColorId::kBlue,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(CollaborationId("collaboration"));
  SavedTabGroupTab tab_to_delete(GURL("https://google.com/1"), u"title 1",
                                 group.saved_guid(), /*position=*/std::nullopt);
  group.AddTabLocally(tab_to_delete);
  group.AddTabLocally(SavedTabGroupTab(GURL("https://google.com/2"), u"title 2",
                                       group.saved_guid(),
                                       /*position=*/std::nullopt));
  model()->AddedLocally(group);
  ASSERT_EQ(model()->Count(), 1);
  ASSERT_THAT(model()->saved_tab_groups().front().saved_tabs(), SizeIs(2));

  ApplySingleEntityChange(CreateDeleteEntityChange(
      tab_to_delete.saved_tab_guid().AsLowercaseString()));

  ASSERT_EQ(model()->Count(), 1);
  EXPECT_THAT(
      model()->saved_tab_groups().front().saved_tabs(),
      UnorderedElementsAre(HasTabMetadata("title 2", "https://google.com/2")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldCheckValidEntities) {
  const CollaborationId kCollaborationId("collaboration");
  ASSERT_TRUE(InitializeBridgeAndModel());

  EXPECT_TRUE(bridge()->IsEntityDataValid(CreateEntityData(
      MakeTabGroupSpecifics("test title", sync_pb::SharedTabGroup::GREEN),
      kCollaborationId, kDefaultGaiaId,
      /*updated_by=*/kDefaultGaiaId)));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldRemoveLocalGroupsOnDisableSync) {
  const CollaborationId kCollaborationId("collaboration");
  ASSERT_TRUE(InitializeBridgeAndModel());

  // Initialize the model with some initial data. Create 2 entities to make it
  // sure that each of them is being deleted.
  syncer::EntityChangeList change_list;
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::RED),
      kCollaborationId));
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title 2", sync_pb::SharedTabGroup::GREEN),
      kCollaborationId));
  bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                              std::move(change_list));
  ASSERT_EQ(model()->Count(), 2);
  ASSERT_EQ(GetNumEntriesInStore(), 2u);
  change_list.clear();

  // Stop sync and verify that data is removed from the model.
  bridge()->ApplyDisableSyncChanges(bridge()->CreateMetadataChangeList());
  EXPECT_EQ(model()->Count(), 0);
  EXPECT_EQ(GetNumEntriesInStore(), 0u);
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldNotifyObserversOnDisableSync) {
  const CollaborationId kCollaborationId("collaboration");
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(kCollaborationId);
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com", u"tab 2", group.saved_guid(), /*position=*/1);

  model()->AddedLocally(group);
  model()->AddTabToGroupLocally(group.saved_guid(), tab1);
  model()->AddTabToGroupLocally(group.saved_guid(), tab2);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);

  // Observers must be notified for closed groups to make it sure that
  // the group will be closed. Note that only group closure is notified which
  // will remove the whole group from model and UI.
  Sequence s;
  EXPECT_CALL(
      mock_model_observer(),
      OnSyncBridgeUpdateTypeChanged(Eq(SyncBridgeUpdateType::kDisableSync)))
      .InSequence(s);
  EXPECT_CALL(mock_model_observer(), SavedTabGroupRemovedFromSync)
      .InSequence(s);
  EXPECT_CALL(mock_model_observer(),
              OnSyncBridgeUpdateTypeChanged(
                  Eq(SyncBridgeUpdateType::kCompletedDisableSyncThisSession)))
      .InSequence(s);
  bridge()->ApplyDisableSyncChanges(bridge()->CreateMetadataChangeList());
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldReturnGroupDataForCommit) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(CollaborationId("collaboration"));
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com", u"tab 2", group.saved_guid(), /*position=*/1);

  model()->AddedLocally(group);
  model()->AddTabToGroupLocally(group.saved_guid(), tab1);
  model()->AddTabToGroupLocally(group.saved_guid(), tab2);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);

  std::vector<syncer::EntityData> entity_data_list = ExtractEntityDataFromBatch(
      bridge()->GetDataForCommit({group.saved_guid().AsLowercaseString()}));

  EXPECT_THAT(entity_data_list, UnorderedElementsAre(HasGroupEntityData(
                                    "title", sync_pb::SharedTabGroup_Color_GREY,
                                    "collaboration")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldReturnTabDataForCommit) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(CollaborationId("collaboration"));
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"tab 2", group.saved_guid(), /*position=*/1);

  model()->AddedLocally(group);
  model()->AddTabToGroupLocally(group.saved_guid(), tab1);
  model()->AddTabToGroupLocally(group.saved_guid(), tab2);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);

  std::vector<syncer::EntityData> entity_data_list = ExtractEntityDataFromBatch(
      bridge()->GetDataForCommit({tab2.saved_tab_guid().AsLowercaseString(),
                                  tab1.saved_tab_guid().AsLowercaseString()}));

  EXPECT_THAT(
      entity_data_list,
      UnorderedElementsAre(
          HasTabEntityData("tab 2", "http://google.com/2", "collaboration"),
          HasTabEntityData("tab 1", "http://google.com/1", "collaboration")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ReturnUnsupportedURLForCommit) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(CollaborationId("collaboration"));
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "chrome://abc", u"tab 1", group.saved_guid(), /*position=*/0);

  model()->AddedLocally(group);
  model()->AddTabToGroupLocally(group.saved_guid(), tab1);

  std::vector<syncer::EntityData> entity_data_list = ExtractEntityDataFromBatch(
      bridge()->GetDataForCommit({tab1.saved_tab_guid().AsLowercaseString()}));

  EXPECT_THAT(entity_data_list,
              UnorderedElementsAre(HasTabEntityData(
                  "", kChromeSavedTabGroupUnsupportedURL, "collaboration")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldReturnAllDataForDebugging) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(CollaborationId("collaboration"));
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"tab 2", group.saved_guid(), /*position=*/1);

  model()->AddedLocally(group);
  model()->AddTabToGroupLocally(group.saved_guid(), tab1);
  model()->AddTabToGroupLocally(group.saved_guid(), tab2);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);

  std::vector<syncer::EntityData> entity_data_list =
      ExtractEntityDataFromBatch(bridge()->GetAllDataForDebugging());

  EXPECT_THAT(
      entity_data_list,
      UnorderedElementsAre(
          HasTabEntityData("tab 2", "http://google.com/2", "collaboration"),
          HasTabEntityData("tab 1", "http://google.com/1", "collaboration"),
          HasGroupEntityData("title", sync_pb::SharedTabGroup_Color_GREY,
                             "collaboration")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldSendToSyncNewGroupWithTabs) {
  ASSERT_TRUE(InitializeBridgeAndModel());
  const base::Uuid kOriginatingSavedTabGroupGuid =
      base::Uuid::GenerateRandomV4();

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(CollaborationId("collaboration"));
  group.SetOriginatingTabGroupGuid(kOriginatingSavedTabGroupGuid,
                                   /*use_originating_tab_group_guid=*/true);
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"tab 2", group.saved_guid(), /*position=*/1);

  group.AddTabLocally(tab1);
  group.AddTabLocally(tab2);

  EXPECT_CALL(mock_processor(),
              Put(_,
                  Pointee(HasTabEntityData("tab 2", "http://google.com/2",
                                           "collaboration")),
                  _));
  EXPECT_CALL(mock_processor(),
              Put(_,
                  Pointee(HasTabEntityData("tab 1", "http://google.com/1",
                                           "collaboration")),
                  _));
  EXPECT_CALL(mock_processor(),
              Put(_,
                  AllOf(Pointee(HasGroupEntityData(
                            "title", sync_pb::SharedTabGroup_Color_GREY,
                            "collaboration")),
                        Pointee(HasGroupEntityDataWithOriginatingGroup(
                            kOriginatingSavedTabGroupGuid))),
                  _));

  model()->AddedLocally(group);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldSendToSyncUpdatedGroupMetadata) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt,
                      /*saved_guid=*/base::Uuid::GenerateRandomV4(),
                      test::GenerateRandomTabGroupID());
  group.SetCollaborationId(CollaborationId("collaboration"));
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"tab 2", group.saved_guid(), /*position=*/1);

  group.AddTabLocally(tab1);
  group.AddTabLocally(tab2);
  model()->AddedLocally(group);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);

  syncer::EntityData captured_entity_data;
  EXPECT_CALL(mock_processor(), Put)
      .WillOnce(
          WithArg<1>([&captured_entity_data](
                         std::unique_ptr<syncer::EntityData> entity_data) {
            captured_entity_data = std::move(*entity_data);
          }));
  tab_groups::TabGroupVisualData visual_data(
      u"new title", tab_groups::TabGroupColorId::kYellow);
  model()->UpdateVisualDataLocally(group.local_group_id().value(),
                                   &visual_data);

  EXPECT_THAT(
      captured_entity_data,
      HasGroupEntityData("new title", sync_pb::SharedTabGroup_Color_YELLOW,
                         "collaboration"));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldSendToSyncNewLocalTab) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(CollaborationId("collaboration"));
  SavedTabGroupTab tab = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);

  group.AddTabLocally(tab);
  model()->AddedLocally(group);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 1u);

  SavedTabGroupTab new_tab = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"new tab", group.saved_guid(), /*position=*/1);

  syncer::EntityData captured_entity_data;
  EXPECT_CALL(mock_processor(), Put)
      .WillOnce(
          WithArg<1>([&captured_entity_data](
                         std::unique_ptr<syncer::EntityData> entity_data) {
            captured_entity_data = std::move(*entity_data);
          }));
  model()->AddTabToGroupLocally(group.saved_guid(), new_tab);

  EXPECT_THAT(
      captured_entity_data,
      HasTabEntityData("new tab", "http://google.com/2", "collaboration"));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldSendToSyncRemovedLocalTab) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(CollaborationId("collaboration"));
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab_to_remove =
      test::CreateSavedTabGroupTab("http://google.com/2", u"tab to remove",
                                   group.saved_guid(), /*position=*/1);

  group.AddTabLocally(tab1);
  group.AddTabLocally(tab_to_remove);
  model()->AddedLocally(group);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);

  EXPECT_CALL(mock_processor(),
              Delete(tab_to_remove.saved_tab_guid().AsLowercaseString(), _, _));
  model()->RemoveTabFromGroupLocally(group.saved_guid(),
                                     tab_to_remove.saved_tab_guid());
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldSendToSyncUpdatedLocalTab) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(CollaborationId("collaboration"));
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab_to_update = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"tab 2", group.saved_guid(), /*position=*/1);

  group.AddTabLocally(tab1);
  group.AddTabLocally(tab_to_update);
  model()->AddedLocally(group);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);

  syncer::EntityData captured_entity_data;
  EXPECT_CALL(mock_processor(), Put)
      .WillOnce(
          WithArg<1>([&captured_entity_data](
                         std::unique_ptr<syncer::EntityData> entity_data) {
            captured_entity_data = std::move(*entity_data);
          }));
  tab_to_update.SetURL(GURL("http://google.com/updated"));
  tab_to_update.SetTitle(u"updated tab");
  model()->UpdateTabInGroup(group.saved_guid(), tab_to_update,
                            /*notify_observers=*/true);

  EXPECT_THAT(captured_entity_data,
              HasTabEntityData("updated tab", "http://google.com/updated",
                               "collaboration"));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldSendToSyncRemovedLocalGroup) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(CollaborationId("collaboration"));
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"tab 2", group.saved_guid(), /*position=*/1);

  group.AddTabLocally(tab1);
  group.AddTabLocally(tab2);
  model()->AddedLocally(group);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);

  EXPECT_CALL(mock_processor(),
              Delete(group.saved_guid().AsLowercaseString(), _, _));
  EXPECT_CALL(mock_processor(),
              Delete(tab1.saved_tab_guid().AsLowercaseString(), _, _));
  EXPECT_CALL(mock_processor(),
              Delete(tab2.saved_tab_guid().AsLowercaseString(), _, _));
  model()->RemovedLocally(group.saved_guid());
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldReloadDataOnBrowserRestart) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  const CollaborationId kCollaborationId("collaboration");

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(kCollaborationId);
  group.SetUpdatedByAttribution(kDefaultGaiaId);
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);
  tab1.SetUpdatedByAttribution(kDefaultGaiaId);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"tab 2", group.saved_guid(), /*position=*/1);
  tab2.SetUpdatedByAttribution(kDefaultGaiaId);

  group.AddTabLocally(tab1);
  group.AddTabLocally(tab2);

  model()->AddedLocally(group);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);

  // Verify that the model is destroyed to simulate browser restart.
  StoreMetadataAndReset();
  ASSERT_EQ(model(), nullptr);

  // Note that sync metadata is not checked explicitly because the collaboration
  // ID is stored as a part of sync metadata.
  ASSERT_TRUE(InitializeBridgeAndModel());
  ASSERT_THAT(
      model()->saved_tab_groups(),
      UnorderedElementsAre(HasSharedGroupMetadata(
          "title", tab_groups::TabGroupColorId::kGrey, kCollaborationId)));
  EXPECT_THAT(model()->saved_tab_groups(),
              UnorderedElementsAre(
                  HasSharedAttribution(kDefaultGaiaId, kDefaultGaiaId)));
  EXPECT_THAT(model()->saved_tab_groups().front().saved_tabs(),
              ElementsAre(HasTabMetadata("tab 1", "http://google.com/1"),
                          HasTabMetadata("tab 2", "http://google.com/2")));
  EXPECT_THAT(model()->saved_tab_groups().front().saved_tabs(),
              Each(HasSharedAttribution(kDefaultGaiaId, kDefaultGaiaId)));
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       Migration_FixLocalTabGroupIDsForSharedGroupsDuringFeatureEnabling) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  const CollaborationId kCollaborationId("collaboration");

  // Create a group add to model.
  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(kCollaborationId);
  group.SetUpdatedByAttribution(kDefaultGaiaId);
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);
  tab1.SetUpdatedByAttribution(kDefaultGaiaId);
  group.AddTabLocally(tab1);

  model()->AddedLocally(group);

  // Open the group locally.
  model()->OnGroupOpenedInTabStrip(group.saved_guid(),
                                   test::GenerateRandomTabGroupID());
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 1u);
  ASSERT_TRUE(model()->Get(group.saved_guid())->local_group_id().has_value());

  // Mimic browser restart and mimic that the previous session had shared tab
  // group disabled. On loading from DB, it will clear the local group ID.
  pref_service_.SetBoolean(prefs::kDidEnableSharedTabGroupsInLastSession,
                           false);
  StoreMetadataAndReset();
  ASSERT_EQ(model(), nullptr);
  ASSERT_TRUE(InitializeBridgeAndModel());

  const SavedTabGroup* loaded_group = model()->Get(group.saved_guid());
  EXPECT_THAT(loaded_group->saved_tabs(),
              ElementsAre(HasTabMetadata("tab 1", "http://google.com/1")));

  // Local group ID should have been cleared after restart.
  EXPECT_FALSE(loaded_group->local_group_id().has_value());

  model()->OnGroupOpenedInTabStrip(loaded_group->saved_guid(),
                                   test::GenerateRandomTabGroupID());

  // Mimic browser restart again and mimic that the previous session had shared
  // tab group enabled. So it will persist the local group ID.
  pref_service_.SetBoolean(prefs::kDidEnableSharedTabGroupsInLastSession, true);
  StoreMetadataAndReset();
  ASSERT_EQ(model(), nullptr);
  ASSERT_TRUE(InitializeBridgeAndModel());

  loaded_group = model()->Get(group.saved_guid());
  EXPECT_THAT(loaded_group->saved_tabs(),
              ElementsAre(HasTabMetadata("tab 1", "http://google.com/1")));

  // Local group ID should not be cleared after restart on supported platforms.
  EXPECT_EQ(AreLocalIdsPersisted(), loaded_group->local_group_id().has_value());
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldReturnUniquePositionFromTabSpecifics) {
  ASSERT_TRUE(InitializeBridgeAndModel());
  EXPECT_TRUE(bridge()->SupportsUniquePositions());

  sync_pb::EntitySpecifics specifics;
  EXPECT_THAT(bridge()->GetUniquePosition(specifics),
              EqualsProto(sync_pb::UniquePosition()));

  // Verify that tabs without unique position returns default proto.
  *specifics.mutable_shared_tab_group_data() =
      MakeTabSpecifics("title", GURL("https://google.com/1"),
                       /*group_id=*/base::Uuid::GenerateRandomV4(),
                       GenerateRandomUniquePosition());
  specifics.mutable_shared_tab_group_data()
      ->mutable_tab()
      ->clear_unique_position();
  EXPECT_THAT(bridge()->GetUniquePosition(specifics),
              EqualsProto(sync_pb::UniquePosition()));

  sync_pb::UniquePosition unique_position =
      GenerateRandomUniquePosition().ToProto();
  *specifics.mutable_shared_tab_group_data()
       ->mutable_tab()
       ->mutable_unique_position() = unique_position;
  EXPECT_THAT(bridge()->GetUniquePosition(specifics),
              EqualsProto(unique_position));
}

// Unique position is used by the tabs only.
TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldReturnEmptyUniquePositionFromGroupSpecifics) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  sync_pb::EntitySpecifics specifics;
  *specifics.mutable_shared_tab_group_data() =
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE);
  EXPECT_THAT(bridge()->GetUniquePosition(specifics),
              EqualsProto(sync_pb::UniquePosition()));
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldGenerateUniquePositionsWhenGroupAddedLocally) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  const CollaborationId kCollaborationId("collaboration");

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(kCollaborationId);
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"tab 2", group.saved_guid(), /*position=*/1);
  group.AddTabLocally(tab1);
  group.AddTabLocally(tab2);

  sync_pb::UniquePosition unique_position_1 =
      GenerateRandomUniquePosition().ToProto();
  sync_pb::UniquePosition unique_position_2 =
      GenerateRandomUniquePosition().ToProto();
  EXPECT_CALL(mock_processor(),
              UniquePositionForInitialEntity(HasClientTagHashForTab(tab1)))
      .WillOnce(Return(unique_position_1));
  EXPECT_CALL(mock_processor(),
              UniquePositionAfter(Eq(StorageKeyForTab(tab1)),
                                  HasClientTagHashForTab(tab2)))
      .WillOnce(Return(unique_position_2));

  EXPECT_CALL(
      mock_processor(),
      Put(_, Pointee(HasTabEntityDataWithPosition("tab 2", unique_position_2)),
          _));
  EXPECT_CALL(
      mock_processor(),
      Put(_, Pointee(HasTabEntityDataWithPosition("tab 1", unique_position_1)),
          _));
  EXPECT_CALL(
      mock_processor(),
      Put(_,
          Pointee(HasGroupEntityData(
              "title", sync_pb::SharedTabGroup_Color_GREY, kCollaborationId)),
          _));

  model()->AddedLocally(group);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldGenerateUniquePositionForInitialTabEntity) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(CollaborationId("collaboration"));
  model()->AddedLocally(group);

  // Add the first tab to the group.
  sync_pb::UniquePosition unique_position =
      GenerateRandomUniquePosition().ToProto();
  SavedTabGroupTab new_tab = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"new tab", group.saved_guid(), /*position=*/0);
  EXPECT_CALL(mock_processor(),
              UniquePositionForInitialEntity(HasClientTagHashForTab(new_tab)))
      .WillOnce(Return(unique_position));
  EXPECT_CALL(
      mock_processor(),
      Put(StorageKeyForTab(new_tab),
          Pointee(HasTabEntityDataWithPosition("new tab", unique_position)),
          _));
  model()->AddTabToGroupLocally(group.saved_guid(), new_tab);
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldGenerateUniquePositionForNewTabBefore) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(CollaborationId("collaboration"));
  SavedTabGroupTab tab = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab", group.saved_guid(), /*position=*/0);
  group.AddTabLocally(tab);
  model()->AddedLocally(group);

  // Add new tab before the existing tab.
  sync_pb::UniquePosition unique_position =
      GenerateRandomUniquePosition().ToProto();
  SavedTabGroupTab new_tab = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"new tab", group.saved_guid(), /*position=*/0);
  EXPECT_CALL(mock_processor(),
              UniquePositionBefore(StorageKeyForTab(tab),
                                   HasClientTagHashForTab(new_tab)))
      .WillOnce(Return(unique_position));

  // Only the new tab is expected to be committed.
  EXPECT_CALL(
      mock_processor(),
      Put(StorageKeyForTab(new_tab),
          Pointee(HasTabEntityDataWithPosition("new tab", unique_position)),
          _));
  EXPECT_CALL(mock_processor(), Put(StorageKeyForTab(tab), _, _)).Times(0);
  model()->AddTabToGroupLocally(group.saved_guid(), new_tab);

  // Verify that the new tab was added to the beginning of the group.
  ASSERT_THAT(model()->Get(group.saved_guid())->saved_tabs(), SizeIs(2));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs()[0].saved_tab_guid(),
            new_tab.saved_tab_guid());
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldGenerateUniquePositionForNewTabAfter) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(CollaborationId("collaboration"));
  SavedTabGroupTab tab = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab", group.saved_guid(), /*position=*/0);
  group.AddTabLocally(tab);
  model()->AddedLocally(group);

  // Add new tab after the existing tab.
  sync_pb::UniquePosition unique_position =
      GenerateRandomUniquePosition().ToProto();
  SavedTabGroupTab new_tab = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"new tab", group.saved_guid(), /*position=*/1);
  EXPECT_CALL(mock_processor(),
              UniquePositionAfter(StorageKeyForTab(tab),
                                  HasClientTagHashForTab(new_tab)))
      .WillOnce(Return(unique_position));

  // Only the new tab is expected to be committed.
  EXPECT_CALL(
      mock_processor(),
      Put(StorageKeyForTab(new_tab),
          Pointee(HasTabEntityDataWithPosition("new tab", unique_position)),
          _));
  EXPECT_CALL(mock_processor(), Put(StorageKeyForTab(tab), _, _)).Times(0);
  model()->AddTabToGroupLocally(group.saved_guid(), new_tab);

  // Verify that the new tab was added to the back of the group.
  ASSERT_THAT(model()->Get(group.saved_guid())->saved_tabs(), SizeIs(2));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs()[1].saved_tab_guid(),
            new_tab.saved_tab_guid());
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldGenerateUniquePositionForNewTabBetween) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(CollaborationId("collaboration"));
  SavedTabGroupTab tab_before = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab before", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab_after = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"tab after", group.saved_guid(), /*position=*/1);
  group.AddTabLocally(tab_before);
  group.AddTabLocally(tab_after);
  model()->AddedLocally(group);

  // Add new tab after the existing tab.
  sync_pb::UniquePosition unique_position =
      GenerateRandomUniquePosition().ToProto();
  SavedTabGroupTab new_tab = test::CreateSavedTabGroupTab(
      "http://google.com/3", u"new tab", group.saved_guid(), /*position=*/1);
  EXPECT_CALL(mock_processor(),
              UniquePositionBetween(StorageKeyForTab(tab_before),
                                    StorageKeyForTab(tab_after),
                                    HasClientTagHashForTab(new_tab)))
      .WillOnce(Return(unique_position));

  // Only the new tab is expected to be committed.
  EXPECT_CALL(
      mock_processor(),
      Put(StorageKeyForTab(new_tab),
          Pointee(HasTabEntityDataWithPosition("new tab", unique_position)),
          _));
  EXPECT_CALL(mock_processor(), Put(StorageKeyForTab(tab_before), _, _))
      .Times(0);
  EXPECT_CALL(mock_processor(), Put(StorageKeyForTab(tab_after), _, _))
      .Times(0);
  model()->AddTabToGroupLocally(group.saved_guid(), new_tab);

  // Verify that the new tab was added between the existing tabs (with index 1).
  ASSERT_THAT(model()->Get(group.saved_guid())->saved_tabs(), SizeIs(3));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs()[1].saved_tab_guid(),
            new_tab.saved_tab_guid());
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldUpdateUniquePositionOnLocalMove) {
  const CollaborationId kCollaborationId("collaboration");
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(kCollaborationId);
  SavedTabGroupTab tab_to_move =
      test::CreateSavedTabGroupTab("http://google.com/1", u"tab to move",
                                   group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab_2 = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"tab 2", group.saved_guid(), /*position=*/1);
  group.AddTabLocally(tab_to_move);
  group.AddTabLocally(tab_2);
  model()->AddedLocally(group);

  // Generate unique position for the moved tab.
  sync_pb::UniquePosition unique_position =
      GenerateRandomUniquePosition().ToProto();
  EXPECT_CALL(mock_processor(),
              UniquePositionAfter(StorageKeyForTab(tab_2),
                                  HasClientTagHashForTab(tab_to_move)))
      .WillOnce(Return(unique_position));

  // Only the moved tab is expected to be committed.
  EXPECT_CALL(
      mock_processor(),
      Put(StorageKeyForTab(tab_to_move),
          Pointee(HasTabEntityDataWithPosition("tab to move", unique_position)),
          _));
  EXPECT_CALL(mock_processor(), Put(StorageKeyForTab(tab_2), _, _)).Times(0);
  model()->MoveTabInGroupTo(group.saved_guid(), tab_to_move.saved_tab_guid(),
                            /*index=*/1);

  EXPECT_THAT(
      model()->saved_tab_groups().front().saved_tabs(),
      ElementsAre(HasTabMetadata("tab 2", "http://google.com/2"),
                  HasTabMetadata("tab to move", "http://google.com/1")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldUpdatePositionOnRemoteUpdate) {
  const CollaborationId kCollaborationId("collaboration");
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(kCollaborationId);

  // Create 3 local tabs to test all the cases of remote updates: insertion to
  // the beginning, to the end and in the middle.
  for (size_t i = 0; i < 3; ++i) {
    group.AddTabLocally(test::CreateSavedTabGroupTab(
        "https://google.com/" + base::NumberToString(i),
        u"tab " + base::NumberToString16(i), group.saved_guid(),
        /*position=*/i));
  }
  GenerateUniquePositionsForTabsInGroup(group);
  model()->AddedLocally(group);

  ASSERT_THAT(model()->saved_tab_groups().front().saved_tabs(),
              ElementsAre(HasTabMetadata("tab 0", "https://google.com/0"),
                          HasTabMetadata("tab 1", "https://google.com/1"),
                          HasTabMetadata("tab 2", "https://google.com/2")));

  // Use "tab 1" for the following remote updates.
  base::Uuid tab_guid_to_update = group.saved_tabs()[1].saved_tab_guid();
  sync_pb::SharedTabGroupDataSpecifics tab_1_specifics = MakeTabSpecifics(
      "tab 1", GURL("https://google.com/1"),
      /*group_id=*/group.saved_guid(), GenerateRandomUniquePosition());
  tab_1_specifics.set_guid(tab_guid_to_update.AsLowercaseString());

  // Move "tab 1" to the end, after "tab 2".
  UpdateAndMockUniquePositionForTabSpecifics(
      tab_1_specifics,
      GenerateUniquePositionAfter(group.saved_tabs()[2]).ToProto());
  ApplySingleEntityChange(
      CreateUpdateEntityChange(tab_1_specifics, kCollaborationId));

  EXPECT_THAT(model()->saved_tab_groups().front().saved_tabs(),
              ElementsAre(HasTabMetadata("tab 0", "https://google.com/0"),
                          HasTabMetadata("tab 2", "https://google.com/2"),
                          HasTabMetadata("tab 1", "https://google.com/1")));

  // Move "tab 1" to the beginning, before "tab 0".
  UpdateAndMockUniquePositionForTabSpecifics(
      tab_1_specifics,
      GenerateUniquePositionBefore(group.saved_tabs()[0]).ToProto());
  ApplySingleEntityChange(
      CreateUpdateEntityChange(tab_1_specifics, kCollaborationId));

  EXPECT_THAT(model()->saved_tab_groups().front().saved_tabs(),
              ElementsAre(HasTabMetadata("tab 1", "https://google.com/1"),
                          HasTabMetadata("tab 0", "https://google.com/0"),
                          HasTabMetadata("tab 2", "https://google.com/2")));

  // Move "tab 1" in the middle, between "tab 0" and "tab 2".
  UpdateAndMockUniquePositionForTabSpecifics(
      tab_1_specifics, GenerateUniquePositionBetween(group.saved_tabs()[0],
                                                     group.saved_tabs()[2])
                           .ToProto());
  ApplySingleEntityChange(
      CreateUpdateEntityChange(tab_1_specifics, kCollaborationId));

  EXPECT_THAT(model()->saved_tab_groups().front().saved_tabs(),
              ElementsAre(HasTabMetadata("tab 0", "https://google.com/0"),
                          HasTabMetadata("tab 1", "https://google.com/1"),
                          HasTabMetadata("tab 2", "https://google.com/2")));

  // Keep "tab 1" in the middle, between "tab 0" and "tab 2".
  UpdateAndMockUniquePositionForTabSpecifics(
      tab_1_specifics, GenerateUniquePositionBetween(group.saved_tabs()[0],
                                                     group.saved_tabs()[2])
                           .ToProto());
  ApplySingleEntityChange(
      CreateUpdateEntityChange(tab_1_specifics, kCollaborationId));

  EXPECT_THAT(model()->saved_tab_groups().front().saved_tabs(),
              ElementsAre(HasTabMetadata("tab 0", "https://google.com/0"),
                          HasTabMetadata("tab 1", "https://google.com/1"),
                          HasTabMetadata("tab 2", "https://google.com/2")));
}

// This test verifies that the model remains stable from the tab positions POV
// to avoid changing tab positions when unnecessary. This is helpful to verify
// applying remote updates which don't contain actual unique position changes.
TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldKeepTabsOrderDuringRemoteUpdate) {
  const CollaborationId kCollaborationId("collaboration");
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(kCollaborationId);

  // Create 3 local tabs and update only 2 of them (but without any moves).
  for (size_t i = 0; i < 3; ++i) {
    group.AddTabLocally(test::CreateSavedTabGroupTab(
        "https://google.com/" + base::NumberToString(i),
        u"tab " + base::NumberToString16(i), group.saved_guid(),
        /*position=*/i));
  }
  GenerateUniquePositionsForTabsInGroup(group);
  model()->AddedLocally(group);
  ASSERT_THAT(model()->saved_tab_groups().front().saved_tabs(),
              ElementsAre(HasTabMetadata("tab 0", "https://google.com/0"),
                          HasTabMetadata("tab 1", "https://google.com/1"),
                          HasTabMetadata("tab 2", "https://google.com/2")));

  EXPECT_CALL(mock_model_observer(), SavedTabGroupUpdatedFromSync)
      .Times(2)
      .WillRepeatedly(InvokeWithoutArgs([this]() {
        // Verify that tab updates do not reorder them within a group.
        EXPECT_THAT(model()->saved_tab_groups().front().saved_tabs(),
                    ElementsAre(HasTabUrl("https://google.com/0"),
                                HasTabUrl("https://google.com/1"),
                                HasTabUrl("https://google.com/2")));
      }));

  // Apply updates for the 2 first tabs keeping their unique positions. Keep tab
  // URLs to be able to verify the order during applying updates.
  syncer::EntityChangeList change_list;
  change_list.push_back(CreateUpdateEntityChange(
      GenerateTabUpdateSpecifics(group.saved_tabs()[0], "updated tab 0"),
      kCollaborationId));
  change_list.push_back(CreateUpdateEntityChange(
      GenerateTabUpdateSpecifics(group.saved_tabs()[1], "updated tab 1"),
      kCollaborationId));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(change_list));

  // Verify that the order hasn't changed but updates were applied.
  ASSERT_THAT(
      model()->saved_tab_groups().front().saved_tabs(),
      ElementsAre(HasTabMetadata("updated tab 0", "https://google.com/0"),
                  HasTabMetadata("updated tab 1", "https://google.com/1"),
                  HasTabMetadata("tab 2", "https://google.com/2")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldUpdateMultiplePositionsOnRemoteUpdate) {
  const CollaborationId kCollaborationId("collaboration");
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(kCollaborationId);

  // Create 3 local tabs to have the following states during one update: tab 0
  // is deleted, tab 1 is moved, tab 2 stays intact.
  for (size_t i = 0; i < 3; ++i) {
    group.AddTabLocally(test::CreateSavedTabGroupTab(
        "https://google.com/" + base::NumberToString(i),
        u"tab " + base::NumberToString16(i), group.saved_guid(),
        /*position=*/i));
  }
  GenerateUniquePositionsForTabsInGroup(group);
  model()->AddedLocally(group);

  ASSERT_THAT(model()->saved_tab_groups().front().saved_tabs(),
              ElementsAre(HasTabMetadata("tab 0", "https://google.com/0"),
                          HasTabMetadata("tab 1", "https://google.com/1"),
                          HasTabMetadata("tab 2", "https://google.com/2")));

  // Move tab 1 to the end, and remove tab 0.
  base::Uuid tab_guid_to_update = group.saved_tabs()[1].saved_tab_guid();
  sync_pb::SharedTabGroupDataSpecifics tab_1_specifics = MakeTabSpecifics(
      "tab 1", GURL("https://google.com/1"),
      /*group_id=*/group.saved_guid(), GenerateRandomUniquePosition());
  tab_1_specifics.set_guid(tab_guid_to_update.AsLowercaseString());

  // Update unique positions in the processor before applying updates.
  UpdateAndMockUniquePositionForTabSpecifics(
      tab_1_specifics,
      GenerateUniquePositionAfter(group.saved_tabs()[2]).ToProto());
  ON_CALL(mock_processor(), GetUniquePositionForStorageKey(
                                StorageKeyForTab(group.saved_tabs()[0])))
      .WillByDefault(Return(sync_pb::UniquePosition()));

  syncer::EntityChangeList change_list;
  change_list.push_back(
      CreateUpdateEntityChange(tab_1_specifics, kCollaborationId));
  change_list.push_back(
      CreateDeleteEntityChange(StorageKeyForTab(group.saved_tabs()[0])));

  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(change_list));

  EXPECT_THAT(model()->saved_tab_groups().front().saved_tabs(),
              ElementsAre(HasTabMetadata("tab 2", "https://google.com/2"),
                          HasTabMetadata("tab 1", "https://google.com/1")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, UpdateExistingTabWithUnsupportedURL) {
  const CollaborationId kCollaborationId("collaboration");
  ASSERT_TRUE(InitializeBridgeAndModel());

  sync_pb::SharedTabGroupDataSpecifics group_specifics =
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE);
  const base::Uuid group_id =
      base::Uuid::ParseLowercase(group_specifics.guid());

  sync_pb::SharedTabGroupDataSpecifics tab_to_update_specifics =
      MakeTabSpecifics("tab title 1", GURL("https://google.com/1"), group_id,
                       GenerateRandomUniquePosition());

  syncer::EntityChangeList change_list;
  change_list.push_back(
      CreateAddEntityChange(group_specifics, kCollaborationId));
  change_list.push_back(
      CreateAddEntityChange(tab_to_update_specifics, kCollaborationId));

  bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                              std::move(change_list));
  ASSERT_EQ(model()->Count(), 1);
  ASSERT_THAT(model()->saved_tab_groups().front().saved_tabs(), SizeIs(1));

  tab_to_update_specifics.mutable_tab()->set_url(
      kChromeSavedTabGroupUnsupportedURL);
  ApplySingleEntityChange(
      CreateUpdateEntityChange(tab_to_update_specifics, kCollaborationId));

  // Previous tab url will be preserved if the remote URL is unsupported.
  ASSERT_EQ(model()->Count(), 1);
  EXPECT_THAT(
      model()->saved_tab_groups().front().saved_tabs(),
      ElementsAre(HasTabMetadata("tab title 1", "https://google.com/1")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldCreatePendingNtpOnLastTabRemoval) {
  const CollaborationId kCollaborationId("collaboration");
  ASSERT_TRUE(InitializeBridgeAndModel());

  // Create a shared tab group locally with one tab.
  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(kCollaborationId);
  group.AddTabLocally(test::CreateSavedTabGroupTab(
      "https://google.com/0", u"tab 0", group.saved_guid(), /*position=*/0));

  GenerateUniquePositionsForTabsInGroup(group);
  model()->AddedLocally(group);

  ASSERT_THAT(model()->saved_tab_groups().front().saved_tabs(),
              ElementsAre(HasTabMetadata("tab 0", "https://google.com/0")));

  // Delete the tab from sync. It should result in creating a pending NTP which
  // is never synced.
  EXPECT_CALL(mock_processor(), Put).Times(0);

  syncer::EntityChangeList change_list;
  change_list.push_back(
      CreateDeleteEntityChange(StorageKeyForTab(group.saved_tabs()[0])));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(change_list));

  // Verify that the only tab in the group is a pending NTP.
  ASSERT_THAT(model()->saved_tab_groups(), Not(testing::IsEmpty()));
  EXPECT_THAT(model()->saved_tab_groups().front().saved_tabs(),
              ElementsAre(HasTabMetadata("New tab", "chrome://newtab/")));

  // Simulate browser restart to verify that the pending NTP is persisted.
  SavedTabGroupTab pending_ntp =
      model()->saved_tab_groups().front().saved_tabs()[0];

  StoreMetadataAndReset();
  ASSERT_THAT(model(), IsNull());
  ASSERT_TRUE(InitializeBridgeAndModel());
  ASSERT_THAT(model()->Get(group.saved_guid()), NotNull());

  ASSERT_EQ(1u, model()->Get(group.saved_guid())->saved_tabs().size());
  EXPECT_EQ(model()->Get(group.saved_guid())->saved_tabs()[0].saved_tab_guid(),
            pending_ntp.saved_tab_guid());
  EXPECT_EQ(model()->Get(group.saved_guid())->saved_tabs()[0].url(),
            pending_ntp.url());
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldAssignLocalGroupId) {
  const CollaborationId kCollaborationId("collaboration");
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(kCollaborationId);
  group.AddTabLocally(test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab", group.saved_guid(), /*position=*/0));
  model()->AddedLocally(group);

  LocalTabGroupID local_group_id = test::GenerateRandomTabGroupID();
  model()->OnGroupOpenedInTabStrip(group.saved_guid(), local_group_id);

  // Verify that the local group ID is persisted if supported.
  if (!AreLocalIdsPersisted()) {
    return;
  }

  // Simulate browser restart to verify that the local group ID is persisted.
  StoreMetadataAndReset();
  ASSERT_THAT(model(), IsNull());
  ASSERT_TRUE(InitializeBridgeAndModel());
  ASSERT_THAT(model()->Get(group.saved_guid()), NotNull());

  EXPECT_EQ(model()->Get(group.saved_guid())->local_group_id(), local_group_id);

  // Close the tab group and simulate browser restart.
  model()->OnGroupClosedInTabStrip(local_group_id);

  StoreMetadataAndReset();
  ASSERT_THAT(model(), IsNull());
  ASSERT_TRUE(InitializeBridgeAndModel());
  ASSERT_THAT(model()->Get(group.saved_guid()), NotNull());

  EXPECT_EQ(model()->Get(group.saved_guid())->local_group_id(), std::nullopt);
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldReturnErrorOnUnexpectedCollaborationId) {
  const CollaborationId kCollaborationId("collaboration");
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(kCollaborationId);
  group.AddTabLocally(test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab", group.saved_guid(), /*position=*/0));
  model()->AddedLocally(group);

  sync_pb::SharedTabGroupDataSpecifics group_update_specifics =
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE);
  group_update_specifics.set_guid(group.saved_guid().AsLowercaseString());
  EXPECT_NE(ApplySingleEntityChange(CreateUpdateEntityChange(
                group_update_specifics,
                CollaborationId("unexpected_collaboration_id"))),
            std::nullopt);

  sync_pb::SharedTabGroupDataSpecifics tab_update_specifics = MakeTabSpecifics(
      "tab", GURL("http://google.com/1"),
      /*group_id=*/group.saved_guid(), GenerateRandomUniquePosition());
  tab_update_specifics.set_guid(
      group.saved_tabs()[0].saved_tab_guid().AsLowercaseString());
  EXPECT_NE(ApplySingleEntityChange(CreateUpdateEntityChange(
                tab_update_specifics,
                CollaborationId("unexpected_collaboration_id"))),
            std::nullopt);
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldStoreLocalIdOnRemoteUpdate) {
  if (!AreLocalIdsPersisted()) {
    // This test is only relevant if local IDs are persisted.
    return;
  }

  ASSERT_TRUE(InitializeBridgeAndModel());

  const CollaborationId kCollaborationId("collaboration");
  const LocalTabGroupID kLocalGroupId = test::GenerateRandomTabGroupID();

  // Simulate a reentrant call during applying remote updates.
  EXPECT_CALL(mock_model_observer(), SavedTabGroupAddedFromSync)
      .WillOnce([this, &kLocalGroupId](const base::Uuid& group_guid) {
        model()->OnGroupOpenedInTabStrip(group_guid, kLocalGroupId);
      });
  ApplySingleEntityChange(CreateAddEntityChange(
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::RED),
      kCollaborationId));
  ASSERT_THAT(model()->saved_tab_groups(),
              UnorderedElementsAre(HasLocalGroupId(kLocalGroupId)));
  testing::Mock::VerifyAndClearExpectations(&mock_model_observer());

  // Verify that the model is destroyed to simulate browser restart.
  StoreMetadataAndReset();
  ASSERT_EQ(model(), nullptr);
  ASSERT_TRUE(InitializeBridgeAndModel());

  // Verify that the local group ID is persisted.
  EXPECT_THAT(model()->saved_tab_groups(),
              UnorderedElementsAre(HasLocalGroupId(kLocalGroupId)));
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldPropagateCreationTimeOnRemoteUpdate) {
  const CollaborationId kCollaborationId("collaboration");
  ASSERT_TRUE(InitializeBridgeAndModel());

  const base::Time kCreationTime = base::Time::Now();

  sync_pb::SharedTabGroupDataSpecifics group_specifics =
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::RED);
  ApplySingleEntityChange(
      CreateAddEntityChange(group_specifics, kCollaborationId, kCreationTime));

  ApplySingleEntityChange(CreateAddEntityChange(
      MakeTabSpecifics("title", GURL("http://url.com"),
                       base::Uuid::ParseLowercase(group_specifics.guid()),
                       GenerateRandomUniquePosition()),
      kCollaborationId, kCreationTime));

  ASSERT_THAT(model()->saved_tab_groups(), SizeIs(1));
  const SavedTabGroup& group = model()->saved_tab_groups().front();
  ASSERT_THAT(group.saved_tabs(), SizeIs(1));
  const SavedTabGroupTab& tab = group.saved_tabs()[0];

  EXPECT_EQ(group.creation_time(), kCreationTime);
  EXPECT_EQ(tab.creation_time(), kCreationTime);
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldPopulateCreationTimeOnLocalCreation) {
  const CollaborationId kCollaborationId("collaboration");
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(kCollaborationId);
  SavedTabGroupTab tab = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab", group.saved_guid(), /*position=*/0);
  group.AddTabLocally(tab);

  EXPECT_CALL(mock_processor(),
              Put(StorageKeyForTab(tab),
                  Pointee(HasCreationTime(tab.creation_time())), _));
  EXPECT_CALL(mock_processor(),
              Put(StorageKeyForGroup(group),
                  Pointee(HasCreationTime(group.creation_time())), _));
  model()->AddedLocally(group);
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldPropagateNavigationTimeOnRemoteUpdate) {
  const CollaborationId kCollaborationId("collaboration");
  ASSERT_TRUE(InitializeBridgeAndModel());

  const base::Time kCreationTime = base::Time::Now() - base::Hours(4);
  const base::Time kModificationTime = base::Time::Now() - base::Hours(3);

  // Add a group with a single tab from sync. Check navigation time.
  sync_pb::SharedTabGroupDataSpecifics group_specifics =
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::RED);
  ApplySingleEntityChange(CreateAddEntityChange(
      group_specifics, kCollaborationId, kCreationTime, kModificationTime));

  sync_pb::SharedTabGroupDataSpecifics tab_specifics =
      MakeTabSpecifics("title", GURL("http://url.com"),
                       base::Uuid::ParseLowercase(group_specifics.guid()),
                       GenerateRandomUniquePosition());
  ApplySingleEntityChange(CreateAddEntityChange(
      tab_specifics, kCollaborationId, kCreationTime, kModificationTime));

  ASSERT_THAT(model()->saved_tab_groups(), SizeIs(1));
  SavedTabGroup group = model()->saved_tab_groups().front();
  ASSERT_THAT(group.saved_tabs(), SizeIs(1));
  SavedTabGroupTab tab = group.saved_tabs()[0];

  EXPECT_EQ(group.creation_time(), kCreationTime);
  EXPECT_EQ(tab.creation_time(), kCreationTime);
  EXPECT_EQ(tab.navigation_time(), kModificationTime);

  // Update the tab again from sync. Verify the updated navigation time.
  const base::Time kModificationTime2 = base::Time::Now() - base::Hours(2);
  ApplySingleEntityChange(CreateUpdateEntityChange(
      tab_specifics, kCollaborationId, kCreationTime, kModificationTime2));
  group = model()->saved_tab_groups().front();
  tab = group.saved_tabs()[0];
  EXPECT_EQ(group.creation_time(), kCreationTime);
  EXPECT_EQ(tab.creation_time(), kCreationTime);
  EXPECT_EQ(tab.navigation_time(), kModificationTime2);
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldPopulateAttributionMetadataOnRemoteUpdate) {
  const CollaborationId kCollaborationId("collaboration");
  const GaiaId kUpdatedBy1("gaia_id_1");
  const GaiaId kUpdatedBy2("gaia_id_2");
  ASSERT_TRUE(InitializeBridgeAndModel());

  syncer::EntityChangeList change_list;
  sync_pb::SharedTabGroupDataSpecifics group_specifics =
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::RED);
  base::Uuid group_guid = base::Uuid::ParseLowercase(group_specifics.guid());

  sync_pb::SharedTabGroupDataSpecifics tab_specifics =
      MakeTabSpecifics("tab", GURL("http://url.com"), group_guid,
                       GenerateRandomUniquePosition());
  change_list.push_back(syncer::EntityChange::CreateAdd(
      group_specifics.guid(),
      CreateEntityData(group_specifics, kCollaborationId,
                       /*created_by=*/kDefaultGaiaId, kUpdatedBy1,
                       /*creation_time=*/base::Time::Now())));
  change_list.push_back(syncer::EntityChange::CreateAdd(
      tab_specifics.guid(),
      CreateEntityData(tab_specifics, kCollaborationId,
                       /*created_by=*/kDefaultGaiaId, kUpdatedBy1,
                       /*creation_time=*/base::Time::Now())));
  bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                              std::move(change_list));

  const SavedTabGroup* group = model()->Get(group_guid);
  ASSERT_THAT(group, NotNull());
  EXPECT_EQ(group->shared_attribution().created_by, kDefaultGaiaId);
  EXPECT_EQ(group->shared_attribution().updated_by, kUpdatedBy1);

  ASSERT_THAT(group->saved_tabs(), SizeIs(1));
  EXPECT_EQ(group->saved_tabs()[0].shared_attribution().created_by,
            kDefaultGaiaId);
  EXPECT_EQ(group->saved_tabs()[0].shared_attribution().updated_by,
            kUpdatedBy1);

  change_list.clear();
  change_list.push_back(syncer::EntityChange::CreateUpdate(
      group_specifics.guid(),
      CreateEntityData(group_specifics, kCollaborationId,
                       /*created_by=*/kDefaultGaiaId, kUpdatedBy2,
                       /*creation_time=*/base::Time::Now())));
  change_list.push_back(syncer::EntityChange::CreateUpdate(
      tab_specifics.guid(),
      CreateEntityData(tab_specifics, kCollaborationId,
                       /*created_by=*/kDefaultGaiaId, kUpdatedBy2,
                       /*creation_time=*/base::Time::Now())));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(change_list));

  EXPECT_EQ(group->shared_attribution().created_by, kDefaultGaiaId);
  EXPECT_EQ(group->shared_attribution().updated_by, kUpdatedBy2);
  ASSERT_THAT(group->saved_tabs(), SizeIs(1));
  EXPECT_EQ(group->saved_tabs()[0].shared_attribution().created_by,
            kDefaultGaiaId);
  EXPECT_EQ(group->saved_tabs()[0].shared_attribution().updated_by,
            kUpdatedBy2);
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldPropagateAttributionMetadataOnLocalCreation) {
  const CollaborationId kCollaborationId("collaboration");
  const GaiaId kUpdatedBy1("gaia_id_1");
  const GaiaId kUpdatedBy2("gaia_id_2");

  ASSERT_TRUE(InitializeBridgeAndModel());
  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(kCollaborationId);
  group.SetCreatedByAttribution(kDefaultGaiaId);
  group.SetUpdatedByAttribution(kUpdatedBy1);

  SavedTabGroupTab tab = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab", group.saved_guid(), /*position=*/0);
  tab.SetCreatedByAttribution(kDefaultGaiaId);
  tab.SetUpdatedByAttribution(kUpdatedBy1);
  group.AddTabLocally(tab);

  EXPECT_CALL(mock_processor(),
              Put(StorageKeyForGroup(group),
                  Pointee(HasAttributionMetadata(kUpdatedBy1)), _));
  EXPECT_CALL(mock_processor(),
              Put(StorageKeyForTab(tab),
                  Pointee(HasAttributionMetadata(kUpdatedBy1)), _));
  model()->AddedLocally(group);

  // Update the tab attribution metadata.
  tab.SetUpdatedByAttribution(kUpdatedBy2);
  EXPECT_CALL(mock_processor(),
              Put(StorageKeyForTab(tab),
                  Pointee(HasAttributionMetadata(kUpdatedBy2)), _));
  model()->UpdateTabInGroup(group.saved_guid(), tab, /*notify_observers=*/true);
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldKeepGroupWhenAllTabsAreUpdated) {
  const CollaborationId kCollaborationId("collaboration");
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(kCollaborationId);
  SavedTabGroupTab tab = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"Tab 1", group.saved_guid(), /*position=*/0);
  group.AddTabLocally(tab);
  model()->AddedLocally(group);

  // Remote update replaces the existing tab with a new one.
  sync_pb::SharedTabGroupDataSpecifics new_tab_specifics =
      MakeTabSpecifics("Tab 2", GURL("http://google.com/2"), group.saved_guid(),
                       GenerateRandomUniquePosition());
  syncer::EntityChangeList change_list;
  change_list.push_back(CreateDeleteEntityChange(StorageKeyForTab(tab)));
  change_list.push_back(
      CreateUpdateEntityChange(new_tab_specifics, kCollaborationId));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(change_list));

  // The group should still be present in the model with the new tab.
  ASSERT_THAT(
      model()->saved_tab_groups(),
      ElementsAre(HasSharedGroupMetadata(
          "title", tab_groups::TabGroupColorId::kGrey, kCollaborationId)));
  EXPECT_THAT(model()->saved_tab_groups().front().saved_tabs(),
              ElementsAre(HasTabMetadata("Tab 2", "http://google.com/2")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldNotifyWhenCommittedNewTabGroup) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup saved_group(u"title", tab_groups::TabGroupColorId::kGrey,
                            /*urls=*/{}, /*position=*/std::nullopt);

  SavedTabGroup shared_group =
      saved_group.CloneAsSharedTabGroup(CollaborationId("collaboration"));
  SavedTabGroupTab tab1 =
      test::CreateSavedTabGroupTab("http://google.com/1", u"tab 1",
                                   shared_group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 =
      test::CreateSavedTabGroupTab("http://google.com/2", u"tab 2",
                                   shared_group.saved_guid(), /*position=*/1);
  shared_group.AddTabLocally(tab1);
  shared_group.AddTabLocally(tab2);
  model()->AddedLocally(shared_group);

  ASSERT_THAT(model()->Get(shared_group.saved_guid()), NotNull());
  ASSERT_TRUE(
      model()->Get(shared_group.saved_guid())->is_transitioning_to_shared());

  // Simulate commit completion but the group is not committed yet.
  EXPECT_CALL(mock_processor(), IsEntityUnsynced).WillOnce(Return(true));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        syncer::EntityChangeList());
  testing::Mock::VerifyAndClearExpectations(&mock_processor());
  EXPECT_TRUE(
      model()->Get(shared_group.saved_guid())->is_transitioning_to_shared());

  // Simulate that the group is committed. IsEntityUnsynced() should be called
  // for the group and all its tabs.
  EXPECT_CALL(mock_processor(), IsEntityUnsynced)
      .Times(3)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_model_observer(),
              SavedTabGroupUpdatedLocally(Eq(shared_group.saved_guid()),
                                          /*tab_guid=*/Eq(std::nullopt)));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        syncer::EntityChangeList());

  EXPECT_FALSE(
      model()->Get(shared_group.saved_guid())->is_transitioning_to_shared());
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldNotNotifyWhenRemovedBeforeCommitted) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup saved_group(u"title", tab_groups::TabGroupColorId::kGrey,
                            /*urls=*/{}, /*position=*/std::nullopt);

  SavedTabGroup shared_group =
      saved_group.CloneAsSharedTabGroup(CollaborationId("collaboration"));
  SavedTabGroupTab tab1 =
      test::CreateSavedTabGroupTab("http://google.com/1", u"tab 1",
                                   shared_group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 =
      test::CreateSavedTabGroupTab("http://google.com/2", u"tab 2",
                                   shared_group.saved_guid(), /*position=*/1);
  shared_group.AddTabLocally(tab1);
  shared_group.AddTabLocally(tab2);
  model()->AddedLocally(shared_group);

  ASSERT_THAT(model()->Get(shared_group.saved_guid()), NotNull());
  ASSERT_TRUE(
      model()->Get(shared_group.saved_guid())->is_transitioning_to_shared());

  // Remove the group locally before it's successfully committed.
  model()->RemovedLocally(shared_group.saved_guid());

  // On the commit completion, no entity should be checked for syncing. There is
  // no other way to verify this case, so use only this expectation.
  EXPECT_CALL(mock_processor(), IsEntityUnsynced).Times(0);
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        syncer::EntityChangeList());
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldReturnTabsMissingGroups) {
  const CollaborationId kCollaborationId("collaboration");
  const base::Time kCreationTime = base::Time::Now();
  const base::Uuid kMissingGroupGuid = base::Uuid::GenerateRandomV4();
  ASSERT_TRUE(InitializeBridgeAndModel());

  sync_pb::SharedTabGroupDataSpecifics tab_specifics =
      MakeTabSpecifics("tab title", GURL("http://google.com/1"),
                       kMissingGroupGuid, GenerateRandomUniquePosition());

  syncer::EntityChangeList change_list;
  change_list.push_back(
      CreateAddEntityChange(tab_specifics, kCollaborationId, kCreationTime));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(change_list));

  std::vector<syncer::EntityData> entity_data_list =
      ExtractEntityDataFromBatch(bridge()->GetAllDataForDebugging());

  EXPECT_THAT(entity_data_list,
              UnorderedElementsAre(HasTabEntityDataWithVersion(
                  "tab title", "http://google.com/1", kCollaborationId, 0)));

  // Simulate browser restart and verify that the tab missing group is loaded.
  StoreMetadataAndReset();

  // Store the metadata for the tab missing group explicitly.
  StoreMetadataForTabSpecifics(tab_specifics, kCollaborationId);

  ASSERT_TRUE(InitializeBridgeAndModel());
  entity_data_list =
      ExtractEntityDataFromBatch(bridge()->GetAllDataForDebugging());
  EXPECT_THAT(entity_data_list,
              UnorderedElementsAre(HasTabEntityDataWithVersion(
                  "tab title", "http://google.com/1", kCollaborationId, 0)));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, UntrackEntitiesForCollaboration) {
  ASSERT_TRUE(InitializeBridgeAndModel());
  CollaborationId collaboration("collaboration");

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(collaboration);
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);

  group.AddTabLocally(tab1);
  model()->AddedLocally(group);

  SavedTabGroup group2(u"title2", tab_groups::TabGroupColorId::kBlue,
                       /*urls=*/{}, /*position=*/std::nullopt);
  group2.SetCollaborationId(CollaborationId("collaboration2"));
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"tab 2", group2.saved_guid(), /*position=*/0);
  model()->AddedLocally(group2);

  StoreSharedSyncMetadataBasedOnModel();
  std::string group_key = group.saved_guid().AsLowercaseString();
  std::string tab_key = tab1.saved_tab_guid().AsLowercaseString();
  base::RunLoop run_loop;
  store().ReadAllMetadata(base::BindLambdaForTesting(
      [&run_loop, &group_key, &tab_key](
          const std::optional<syncer::ModelError>& error,
          std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
        syncer::EntityMetadataMap metadata_map =
            metadata_batch->TakeAllMetadata();
        ASSERT_TRUE(metadata_map.find(group_key) != metadata_map.end());
        ASSERT_TRUE(metadata_map.find(tab_key) != metadata_map.end());
        run_loop.Quit();
      }));
  run_loop.Run();

  // Only group 1 and its tab will be untracked.
  EXPECT_CALL(mock_processor(), UntrackEntityForStorageKey(group_key)).Times(1);
  EXPECT_CALL(mock_processor(), UntrackEntityForStorageKey(tab_key)).Times(1);
  bridge()->UntrackEntitiesForCollaboration(collaboration);

  base::RunLoop run_loop2;
  store().ReadAllMetadata(base::BindLambdaForTesting(
      [&run_loop2, &group_key, &tab_key](
          const std::optional<syncer::ModelError>& error,
          std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
        syncer::EntityMetadataMap metadata_map =
            metadata_batch->TakeAllMetadata();
        EXPECT_TRUE(metadata_map.find(group_key) == metadata_map.end());
        EXPECT_TRUE(metadata_map.find(tab_key) == metadata_map.end());
        run_loop2.Quit();
      }));
  run_loop2.Run();
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldResolveTabsMissingGroupsOnRemoteUpdate) {
  const CollaborationId kCollaborationId("collaboration");
  const base::Uuid kMissingGroupGuid = base::Uuid::GenerateRandomV4();
  ASSERT_TRUE(InitializeBridgeAndModel());

  // Add a tab missing group remotely.
  sync_pb::SharedTabGroupDataSpecifics tab_specifics =
      MakeTabSpecifics("tab title", GURL("http://google.com/1"),
                       kMissingGroupGuid, GenerateRandomUniquePosition());
  tab_specifics.set_version(999);

  ApplySingleEntityChange(
      CreateAddEntityChange(tab_specifics, kCollaborationId));

  std::vector<syncer::EntityData> entity_data_list =
      ExtractEntityDataFromBatch(bridge()->GetAllDataForDebugging());

  // Verify that the model is still empty but the tab missing group is stored.
  ASSERT_THAT(entity_data_list,
              UnorderedElementsAre(HasTabEntityDataWithVersion(
                  "tab title", "http://google.com/1", kCollaborationId, 999)));
  ASSERT_THAT(model()->saved_tab_groups(), IsEmpty());

  // Add the missing group entry remotely.
  sync_pb::SharedTabGroupDataSpecifics group_specifics =
      MakeTabGroupSpecifics("group title", sync_pb::SharedTabGroup::CYAN);
  group_specifics.set_guid(kMissingGroupGuid.AsLowercaseString());
  ApplySingleEntityChange(
      CreateAddEntityChange(group_specifics, kCollaborationId));

  // Both the group and the tab should be present in the model.
  ASSERT_THAT(model()->saved_tab_groups(),
              ElementsAre(HasSharedGroupMetadata(
                  "group title", tab_groups::TabGroupColorId::kCyan,
                  kCollaborationId)));
  EXPECT_THAT(model()->saved_tab_groups().front().saved_tabs(),
              ElementsAre(HasTabMetadata("tab title", "http://google.com/1")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldTrimAllSupportedFieldsFromRemoteTabGroupSpecifics) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  sync_pb::EntitySpecifics remote_tab_group_specifics;
  sync_pb::SharedTabGroupDataSpecifics* tab_group_specifics =
      remote_tab_group_specifics.mutable_shared_tab_group_data();
  tab_group_specifics->set_guid("guid");
  tab_group_specifics->set_update_time_windows_epoch_micros(1234567890);
  tab_group_specifics->mutable_tab_group()->set_title("title");
  tab_group_specifics->mutable_tab_group()->set_color(
      sync_pb::SharedTabGroup::BLUE);
  tab_group_specifics->mutable_tab_group()->set_originating_tab_group_guid(
      "originating_guid");

  EXPECT_THAT(bridge()->TrimAllSupportedFieldsFromRemoteSpecifics(
                  remote_tab_group_specifics),
              EqualsProto(sync_pb::EntitySpecifics()));
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldKeepUnknownFieldsFromRemoteTabGroupSpecifics) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  // Serialize and deserialize the proto to get unknown fields.
  sync_pb::EntitySpecifics remote_tab_group_specifics;
  *remote_tab_group_specifics.mutable_shared_tab_group_data() =
      MakeTabGroupSpecificsWithUnknownFields(
          "title", sync_pb::SharedTabGroup::CYAN,
          /*originating_group_id=*/base::Uuid::GenerateRandomV4(),
          "extra_field_for_testing");

  sync_pb::EntitySpecifics trimmed_specifics =
      bridge()->TrimAllSupportedFieldsFromRemoteSpecifics(
          remote_tab_group_specifics);
  EXPECT_THAT(trimmed_specifics, Not(EqualsProto(sync_pb::EntitySpecifics())));

  // Verify that deserialized proto keeps unknown fields.
  sync_pb::test_utils::SharedTabGroupDataSpecifics
      deserialized_extended_specifics;
  ASSERT_TRUE(deserialized_extended_specifics.ParseFromString(
      trimmed_specifics.shared_tab_group_data().SerializeAsString()));
  EXPECT_EQ(
      deserialized_extended_specifics.tab_group().extra_field_for_testing(),
      "extra_field_for_testing");
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldTrimAllSupportedFieldsFromRemoteTabSpecifics) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  sync_pb::EntitySpecifics remote_tab_specifics;
  sync_pb::SharedTabGroupDataSpecifics* tab_specifics =
      remote_tab_specifics.mutable_shared_tab_group_data();
  tab_specifics->set_guid("guid");
  tab_specifics->set_update_time_windows_epoch_micros(1234567890);
  tab_specifics->mutable_tab()->set_url("http://google.com/1");
  tab_specifics->mutable_tab()->set_title("title");
  tab_specifics->mutable_tab()->set_shared_tab_group_guid("group_guid");
  *tab_specifics->mutable_tab()->mutable_unique_position() =
      GenerateRandomUniquePosition().ToProto();

  EXPECT_THAT(
      bridge()->TrimAllSupportedFieldsFromRemoteSpecifics(remote_tab_specifics),
      EqualsProto(sync_pb::EntitySpecifics()));
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldKeepUnknownFieldsFromRemoteTabSpecifics) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  sync_pb::test_utils::SharedTabGroupDataSpecifics extended_tab_specifics;
  extended_tab_specifics.set_guid("guid");
  extended_tab_specifics.set_update_time_windows_epoch_micros(1234567890);
  extended_tab_specifics.mutable_tab()->set_url("http://google.com/1");
  extended_tab_specifics.mutable_tab()->set_title("title");
  extended_tab_specifics.mutable_tab()->set_shared_tab_group_guid("group_guid");
  *extended_tab_specifics.mutable_tab()->mutable_unique_position() =
      GenerateRandomUniquePosition().ToProto();
  extended_tab_specifics.mutable_tab()->set_extra_field_for_testing(
      "extra_field_for_testing");

  // Serialize and deserialize the proto to get unknown fields.
  sync_pb::EntitySpecifics remote_tab_specifics;
  ASSERT_TRUE(
      remote_tab_specifics.mutable_shared_tab_group_data()->ParseFromString(
          extended_tab_specifics.SerializeAsString()));

  sync_pb::EntitySpecifics trimmed_specifics =
      bridge()->TrimAllSupportedFieldsFromRemoteSpecifics(remote_tab_specifics);

  EXPECT_THAT(trimmed_specifics, Not(EqualsProto(sync_pb::EntitySpecifics())));

  // Verify that deserialized proto keeps unknown fields.
  sync_pb::test_utils::SharedTabGroupDataSpecifics
      deserialized_extended_specifics;
  ASSERT_TRUE(deserialized_extended_specifics.ParseFromString(
      trimmed_specifics.shared_tab_group_data().SerializeAsString()));
  EXPECT_EQ(deserialized_extended_specifics.tab().extra_field_for_testing(),
            "extra_field_for_testing");
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldPopulateUnknownFieldsOnLocalChanges) {
  const CollaborationId kCollaborationId("collaboration");
  ASSERT_TRUE(InitializeBridgeAndModel());

  sync_pb::EntitySpecifics remote_tab_group_specifics;
  *remote_tab_group_specifics.mutable_shared_tab_group_data() =
      MakeTabGroupSpecificsWithUnknownFields(
          "title", sync_pb::SharedTabGroup::CYAN,
          /*originating_group_id=*/base::Uuid::GenerateRandomV4(),
          "extra_field");
  sync_pb::EntitySpecifics trimmed_specifics =
      bridge()->TrimAllSupportedFieldsFromRemoteSpecifics(
          remote_tab_group_specifics);
  ON_CALL(mock_processor(), GetPossiblyTrimmedRemoteSpecifics(_))
      .WillByDefault(ReturnRef(trimmed_specifics));

  ApplySingleEntityChange(CreateAddEntityChange(
      remote_tab_group_specifics.shared_tab_group_data(), kCollaborationId));

  ASSERT_THAT(
      model()->saved_tab_groups(),
      ElementsAre(HasSharedGroupMetadata(
          "title", tab_groups::TabGroupColorId::kCyan, kCollaborationId)));

  // Simulate opening the group in the tab strip to make local changes.
  const base::Uuid group_guid =
      model()->saved_tab_groups().front().saved_guid();
  const tab_groups::LocalTabGroupID local_tab_group_id =
      test::GenerateRandomTabGroupID();
  model()->OnGroupOpenedInTabStrip(group_guid, local_tab_group_id);

  // Make local changes to the group. The bridge should make a local change
  // with the unknown fields populated.
  EXPECT_CALL(
      mock_processor(),
      Put(_, Pointee(EntityDataHasGroupUnsupportedFields("extra_field")), _));
  tab_groups::TabGroupVisualData visual_data(
      u"new title", tab_groups::TabGroupColorId::kYellow);
  model()->UpdateVisualDataLocally(local_tab_group_id, &visual_data);
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldStoreUnsupportedFieldsInLocalStorage) {
  const CollaborationId kCollaborationId("collaboration");
  ASSERT_TRUE(InitializeBridgeAndModel());

  sync_pb::EntitySpecifics remote_tab_group_specifics;
  *remote_tab_group_specifics.mutable_shared_tab_group_data() =
      MakeTabGroupSpecificsWithUnknownFields(
          "title", sync_pb::SharedTabGroup::CYAN,
          /*originating_group_id=*/base::Uuid::GenerateRandomV4(),
          "extra_field");

  ApplySingleEntityChange(CreateAddEntityChange(
      remote_tab_group_specifics.shared_tab_group_data(), kCollaborationId));

  const std::string group_guid =
      remote_tab_group_specifics.shared_tab_group_data().guid();
  EXPECT_THAT(GetAllLocalDataFromStore(),
              ElementsAre(Pair(
                  group_guid,
                  Property(&proto::SharedTabGroupData::specifics,
                           GroupSpecificsHasUnsupportedField("extra_field")))));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldNotPopulateKnownClearedFields) {
  const CollaborationId kCollaborationId("collaboration");
  ASSERT_TRUE(InitializeBridgeAndModel());

  ApplySingleEntityChange(CreateAddEntityChange(
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::CYAN),
      kCollaborationId));
  ASSERT_THAT(
      model()->saved_tab_groups(),
      ElementsAre(HasSharedGroupMetadata(
          "title", tab_groups::TabGroupColorId::kCyan, kCollaborationId)));

  // Simulate that `originating_tab_group_guid` field was unknown on the client.
  // But now, the field is known and needs to be cleared.
  sync_pb::EntitySpecifics trimmed_specifics;
  base::Uuid originating_group_guid = base::Uuid::GenerateRandomV4();
  trimmed_specifics.mutable_shared_tab_group_data()
      ->mutable_tab_group()
      ->set_originating_tab_group_guid(
          originating_group_guid.AsLowercaseString());
  ON_CALL(mock_processor(), GetPossiblyTrimmedRemoteSpecifics(_))
      .WillByDefault(ReturnRef(trimmed_specifics));

  // Simulate opening the group in the tab strip to make local changes.
  const base::Uuid group_guid =
      model()->saved_tab_groups().front().saved_guid();
  const tab_groups::LocalTabGroupID local_tab_group_id =
      test::GenerateRandomTabGroupID();
  model()->OnGroupOpenedInTabStrip(group_guid, local_tab_group_id);

  ASSERT_EQ(model()->saved_tab_groups().front().GetOriginatingTabGroupGuid(
                /*for_sync=*/true),
            std::nullopt);

  // Make local changes to the group. The bridge should make a local change
  // without the originating group guid (which was unsupported and is stored in
  // possibly trimmed specifics in sync metadata).
  EXPECT_CALL(mock_processor(),
              Put(_, Pointee(Not(EntityDataHasOriginatingGroup())), _));
  tab_groups::TabGroupVisualData visual_data(
      u"new title", tab_groups::TabGroupColorId::kYellow);
  model()->UpdateVisualDataLocally(local_tab_group_id, &visual_data);
}

// The number of tabs to test the correct ordering of remote updates.
constexpr size_t kNumTabsForOrderTest = 5;

class SharedTabGroupDataSyncBridgeRemoteUpdateOrderTest
    : public SharedTabGroupDataSyncBridgeTest,
      public testing::WithParamInterface<
          std::array<size_t, kNumTabsForOrderTest>> {
 public:
  static syncer::EntityChangeList ReorderEntityChanges(
      syncer::EntityChangeList&& change_list,
      const std::array<size_t, kNumTabsForOrderTest>& order) {
    CHECK_EQ(change_list.size(), kNumTabsForOrderTest);

    syncer::EntityChangeList ordered_change_list(kNumTabsForOrderTest);
    for (size_t i = 0; i < change_list.size(); ++i) {
      CHECK_LT(order[i], ordered_change_list.size());
      CHECK(!ordered_change_list[order[i]]);
      ordered_change_list[order[i]] = std::move(change_list[i]);
    }

    return ordered_change_list;
  }
};

TEST_P(SharedTabGroupDataSyncBridgeRemoteUpdateOrderTest,
       ShouldAddRemoteTabsInCorrectOrder) {
  const CollaborationId kCollaborationId("collaboration");
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(kCollaborationId);
  model()->AddedLocally(group);

  // Generate remote tabs and then shuffle them.
  syncer::EntityChangeList change_list;
  syncer::UniquePosition next_unique_position = GenerateRandomUniquePosition();
  for (size_t i = 0; i < kNumTabsForOrderTest; ++i) {
    sync_pb::SharedTabGroupDataSpecifics tab_specifics =
        MakeTabSpecifics("tab " + base::NumberToString(i),
                         GURL("https://google.com/" + base::NumberToString(i)),
                         /*group_id=*/group.saved_guid(), next_unique_position);
    ON_CALL(mock_processor(),
            GetUniquePositionForStorageKey(tab_specifics.guid()))
        .WillByDefault(Return(next_unique_position.ToProto()));
    change_list.push_back(
        CreateAddEntityChange(tab_specifics, kCollaborationId));
    next_unique_position = syncer::UniquePosition::After(
        next_unique_position, syncer::UniquePosition::RandomSuffix());
  }

  change_list =
      ReorderEntityChanges(std::move(change_list), /*order=*/GetParam());

  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(change_list));

  // Expect the tabs added in the correct order.
  EXPECT_THAT(model()->saved_tab_groups().front().saved_tabs(),
              ElementsAre(HasTabMetadata("tab 0", "https://google.com/0"),
                          HasTabMetadata("tab 1", "https://google.com/1"),
                          HasTabMetadata("tab 2", "https://google.com/2"),
                          HasTabMetadata("tab 3", "https://google.com/3"),
                          HasTabMetadata("tab 4", "https://google.com/4")));
}

INSTANTIATE_TEST_SUITE_P(All,
                         SharedTabGroupDataSyncBridgeRemoteUpdateOrderTest,
                         testing::Values(std::to_array<size_t>({0, 1, 2, 3, 4}),
                                         std::to_array<size_t>({4, 3, 2, 1, 0}),
                                         std::to_array<size_t>({4, 1, 2, 3, 0}),
                                         std::to_array<size_t>({3, 1, 0, 4, 2}),
                                         std::to_array<size_t>({0, 4, 3, 1,
                                                                2})));

}  // namespace
}  // namespace tab_groups
