// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/shared_tab_group_data_sync_bridge.h"

#include <array>
#include <initializer_list>
#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/prefs/testing_pref_service.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model_observer.h"
#include "components/saved_tab_groups/internal/sync_bridge_tab_group_model_wrapper.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/public/utils.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/shared_tab_group_data_specifics.pb.h"
#include "components/sync/protocol/unique_position.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {

// PrintTo must be defined in the same namespace as the SavedTabGroupTab class.
void PrintTo(const SavedTabGroupTab& tab, std::ostream* os) {
  *os << "(title: " << tab.title() << ", url: " << tab.url() << ")";
}
namespace {

using base::test::EqualsProto;
using tab_groups::test::HasSharedGroupMetadata;
using tab_groups::test::HasTabMetadata;
using testing::_;
using testing::AllOf;
using testing::ElementsAre;
using testing::Eq;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::IsNull;
using testing::NotNull;
using testing::Pointee;
using testing::Return;
using testing::SizeIs;
using testing::UnorderedElementsAre;
using testing::WithArg;

MATCHER_P(HasTabUrl, url, "") {
  return arg.url() == GURL(url);
}

MATCHER_P3(HasGroupEntityData, title, color, collaboration_id, "") {
  const sync_pb::SharedTabGroup& arg_tab_group =
      arg.specifics.shared_tab_group_data().tab_group();
  return arg_tab_group.title() == title && arg_tab_group.color() == color &&
         arg.collaboration_id == collaboration_id;
}

MATCHER_P(HasGroupEntityDataWithOriginatingGroup, originating_group_guid, "") {
  const sync_pb::SharedTabGroup& arg_tab_group =
      arg.specifics.shared_tab_group_data().tab_group();
  return arg_tab_group.originating_tab_group_guid() ==
         originating_group_guid.AsLowercaseString();
}

MATCHER_P3(HasTabEntityData, title, url, collaboration_id, "") {
  const sync_pb::SharedTab& arg_tab =
      arg.specifics.shared_tab_group_data().tab();
  return arg_tab.title() == title && arg_tab.url() == url &&
         arg.collaboration_id == collaboration_id;
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

MATCHER_P(HasClientTagHashForTab, tab, "") {
  return arg == syncer::ClientTagHash::FromUnhashed(
                    syncer::SHARED_TAB_GROUP_DATA, StorageKeyForTab(tab));
}

class MockTabGroupModelObserver : public SavedTabGroupModelObserver {
 public:
  MockTabGroupModelObserver() = default;

  void ObserveModel(SavedTabGroupModel* model) { observation_.Observe(model); }
  void Reset() { observation_.Reset(); }

  MOCK_METHOD(void, SavedTabGroupRemovedFromSync, (const SavedTabGroup&));
  MOCK_METHOD(void,
              SavedTabGroupUpdatedFromSync,
              (const base::Uuid&, const std::optional<base::Uuid>&));

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
    const std::string& collaboration_id) {
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_shared_tab_group_data() = specifics;
  entity_data.collaboration_id = collaboration_id;
  entity_data.name = specifics.guid();
  return entity_data;
}

std::unique_ptr<syncer::EntityChange> CreateAddEntityChange(
    const sync_pb::SharedTabGroupDataSpecifics& specifics,
    const std::string& collaboration_id) {
  const std::string& storage_key = specifics.guid();
  return syncer::EntityChange::CreateAdd(
      storage_key, CreateEntityData(specifics, collaboration_id));
}

std::unique_ptr<syncer::EntityChange> CreateUpdateEntityChange(
    const sync_pb::SharedTabGroupDataSpecifics& specifics,
    const std::string& collaboration_id) {
  const std::string& storage_key = specifics.guid();
  return syncer::EntityChange::CreateUpdate(
      storage_key, CreateEntityData(specifics, collaboration_id));
}

std::unique_ptr<syncer::EntityChange> CreateDeleteEntityChange(
    const std::string& storage_key) {
  return syncer::EntityChange::CreateDelete(storage_key);
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

sync_pb::EntityMetadata CreateMetadata(std::string collaboration_id) {
  sync_pb::EntityMetadata metadata;
  // Other metadata fields are not used in these tests.
  metadata.mutable_collaboration()->set_collaboration_id(
      std::move(collaboration_id));
  return metadata;
}

syncer::UniquePosition GenerateRandomUniquePosition() {
  return syncer::UniquePosition::InitialPosition(
      syncer::UniquePosition::RandomSuffix());
}

class SharedTabGroupDataSyncBridgeTest : public testing::Test {
 public:
  SharedTabGroupDataSyncBridgeTest()
      : store_(syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {}

  // Creates the bridges and initializes the model. Returns true when succeeds.
  bool InitializeBridgeAndModel() {
    ON_CALL(processor_, IsTrackingMetadata())
        .WillByDefault(testing::Return(true));

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
        processor_.CreateForwardingProcessor(), &pref_service_);
    observer_forwarder_ = std::make_unique<ModelObserverForwarder>(
        *saved_tab_group_model_, *bridge_);
    task_environment_.RunUntilIdle();

    return saved_tab_group_model_->is_loaded();
  }

  // Cleans up the bridge and the model, used to simulate browser restart.
  void StoreMetadataAndReset(const std::string& collaboration_id) {
    CHECK(saved_tab_group_model_);
    StoreSharedSyncMetadataBasedOnModel(collaboration_id);

    observer_forwarder_.reset();
    mock_model_observer_.Reset();
    bridge_.reset();
    sync_bridge_model_wrapper_.reset();
    saved_tab_group_model_.reset();
  }

  size_t GetNumEntriesInStore() {
    std::unique_ptr<syncer::DataTypeStore::RecordList> entries;
    base::RunLoop run_loop;
    store_->ReadAllData(base::BindLambdaForTesting(
        [&run_loop, &entries](
            const std::optional<syncer::ModelError>& error,
            std::unique_ptr<syncer::DataTypeStore::RecordList> data) {
          entries = std::move(data);
          run_loop.Quit();
        }));
    run_loop.Run();
    return entries->size();
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
  void ApplySingleEntityChange(
      std::unique_ptr<syncer::EntityChange> entity_change) {
    syncer::EntityChangeList change_list;
    change_list.push_back(std::move(entity_change));
    bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                          std::move(change_list));
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
  void StoreSharedSyncMetadataBasedOnModel(
      const std::string& collaboration_id) {
    std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
        store().CreateWriteBatch();
    syncer::MetadataChangeList* metadata_change_list =
        write_batch->GetMetadataChangeList();
    for (const SavedTabGroup* group : model()->GetSharedTabGroupsOnly()) {
      metadata_change_list->UpdateMetadata(
          group->saved_guid().AsLowercaseString(),
          CreateMetadata(collaboration_id));
      for (const SavedTabGroupTab& tab : group->saved_tabs()) {
        metadata_change_list->UpdateMetadata(
            tab.saved_tab_guid().AsLowercaseString(),
            CreateMetadata(collaboration_id));
      }
    }
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

 private:
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
  ASSERT_TRUE(InitializeBridgeAndModel());

  EXPECT_TRUE(bridge()->SupportsGetClientTag());
  sync_pb::SharedTabGroupDataSpecifics group_specifics =
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE);
  EXPECT_EQ(bridge()->GetClientTag(
                CreateEntityData(group_specifics, "collaboration")),
            group_specifics.guid() + "|collaboration");
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldCallModelReadyToSync) {
  EXPECT_CALL(mock_processor(), ModelReadyToSync).WillOnce(Invoke([]() {}));

  // This already invokes RunUntilIdle, so the call above is expected to happen.
  ASSERT_TRUE(InitializeBridgeAndModel());
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldAddRemoteGroupsAtInitialSync) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  syncer::EntityChangeList change_list;
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE),
      "collaboration"));
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title 2", sync_pb::SharedTabGroup::RED),
      "collaboration 2"));
  bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                              std::move(change_list));

  EXPECT_THAT(
      model()->saved_tab_groups(),
      UnorderedElementsAre(
          HasSharedGroupMetadata("title", tab_groups::TabGroupColorId::kBlue,
                                 "collaboration"),
          HasSharedGroupMetadata("title 2", tab_groups::TabGroupColorId::kRed,
                                 "collaboration 2")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldAddRemoteTabsAtInitialSync) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  sync_pb::SharedTabGroupDataSpecifics group_specifics =
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE);
  const std::string collaboration_id = "collaboration";
  const base::Uuid group_id =
      base::Uuid::ParseLowercase(group_specifics.guid());

  syncer::EntityChangeList change_list;
  change_list.push_back(
      CreateAddEntityChange(group_specifics, collaboration_id));
  change_list.push_back(CreateAddEntityChange(
      MakeTabSpecifics("tab title 1", GURL("https://google.com/1"),
                       /*group_id=*/group_id, GenerateRandomUniquePosition()),
      collaboration_id));
  change_list.push_back(CreateAddEntityChange(
      MakeTabSpecifics("tab title 2", GURL("https://google.com/2"),
                       /*group_id=*/group_id, GenerateRandomUniquePosition()),
      collaboration_id));

  bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                              std::move(change_list));
  ASSERT_THAT(
      model()->saved_tab_groups(),
      ElementsAre(HasSharedGroupMetadata(
          "title", tab_groups::TabGroupColorId::kBlue, "collaboration")));

  // Expect both tabs to be a part of the group.
  EXPECT_THAT(model()->saved_tab_groups().front().saved_tabs(),
              UnorderedElementsAre(
                  HasTabMetadata("tab title 1", "https://google.com/1"),
                  HasTabMetadata("tab title 2", "https://google.com/2")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldAddRemoteGroupsAtIncrementalUpdate) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  syncer::EntityChangeList change_list;
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE),
      "collaboration"));
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title 2", sync_pb::SharedTabGroup::RED),
      "collaboration 2"));
  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(change_list));

  EXPECT_THAT(
      model()->saved_tab_groups(),
      UnorderedElementsAre(
          HasSharedGroupMetadata("title", tab_groups::TabGroupColorId::kBlue,
                                 "collaboration"),
          HasSharedGroupMetadata("title 2", tab_groups::TabGroupColorId::kRed,
                                 "collaboration 2")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldAddRemoteTabsAtIncrementalUpdate) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  sync_pb::SharedTabGroupDataSpecifics group_specifics =
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE);
  const std::string collaboration_id = "collaboration";
  const base::Uuid group_id =
      base::Uuid::ParseLowercase(group_specifics.guid());

  syncer::EntityChangeList change_list;
  change_list.push_back(
      CreateAddEntityChange(group_specifics, collaboration_id));
  change_list.push_back(CreateAddEntityChange(
      MakeTabSpecifics("tab title 1", GURL("https://google.com/1"),
                       /*group_id=*/group_id, GenerateRandomUniquePosition()),
      collaboration_id));
  change_list.push_back(CreateAddEntityChange(
      MakeTabSpecifics("tab title 2", GURL("https://google.com/2"),
                       /*group_id=*/group_id, GenerateRandomUniquePosition()),
      collaboration_id));

  bridge()->ApplyIncrementalSyncChanges(bridge()->CreateMetadataChangeList(),
                                        std::move(change_list));
  ASSERT_THAT(
      model()->saved_tab_groups(),
      ElementsAre(HasSharedGroupMetadata(
          "title", tab_groups::TabGroupColorId::kBlue, "collaboration")));

  // Expect both tabs to be a part of the group.
  EXPECT_THAT(model()->saved_tab_groups().front().saved_tabs(),
              UnorderedElementsAre(
                  HasTabMetadata("tab title 1", "https://google.com/1"),
                  HasTabMetadata("tab title 2", "https://google.com/2")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldUpdateExistingGroupOnIncrementalUpdate) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  sync_pb::SharedTabGroupDataSpecifics group_specifics =
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE);
  const std::string collaboration_id1 = "collaboration";
  syncer::EntityChangeList change_list;
  change_list.push_back(
      CreateAddEntityChange(group_specifics, collaboration_id1));
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title 2", sync_pb::SharedTabGroup::RED),
      "collaboration 2"));
  bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                              std::move(change_list));
  ASSERT_EQ(model()->Count(), 2);

  group_specifics.mutable_tab_group()->set_title("updated title");
  group_specifics.mutable_tab_group()->set_color(sync_pb::SharedTabGroup::CYAN);
  ApplySingleEntityChange(
      CreateUpdateEntityChange(group_specifics, collaboration_id1));

  EXPECT_THAT(
      model()->saved_tab_groups(),
      UnorderedElementsAre(
          HasSharedGroupMetadata("updated title",
                                 tab_groups::TabGroupColorId::kCyan,
                                 "collaboration"),
          HasSharedGroupMetadata("title 2", tab_groups::TabGroupColorId::kRed,
                                 "collaboration 2")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldUpdateExistingTabOnIncrementalUpdate) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  sync_pb::SharedTabGroupDataSpecifics group_specifics =
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::BLUE);
  const std::string collaboration_id = "collaboration";
  const base::Uuid group_id =
      base::Uuid::ParseLowercase(group_specifics.guid());

  sync_pb::SharedTabGroupDataSpecifics tab_to_update_specifics =
      MakeTabSpecifics("tab title 1", GURL("https://google.com/1"),
                       /*group_id=*/group_id, GenerateRandomUniquePosition());

  syncer::EntityChangeList change_list;
  change_list.push_back(
      CreateAddEntityChange(group_specifics, collaboration_id));
  change_list.push_back(
      CreateAddEntityChange(tab_to_update_specifics, collaboration_id));
  change_list.push_back(CreateAddEntityChange(
      MakeTabSpecifics("tab title 2", GURL("https://google.com/2"),
                       /*group_id=*/group_id, GenerateRandomUniquePosition()),
      collaboration_id));

  bridge()->MergeFullSyncData(bridge()->CreateMetadataChangeList(),
                              std::move(change_list));
  ASSERT_EQ(model()->Count(), 1);
  ASSERT_THAT(model()->saved_tab_groups().front().saved_tabs(), SizeIs(2));

  tab_to_update_specifics.mutable_tab()->set_title("updated title");
  ApplySingleEntityChange(
      CreateUpdateEntityChange(tab_to_update_specifics, collaboration_id));

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
  group_to_delete.SetCollaborationId("collaboration");
  group_to_delete.AddTabLocally(SavedTabGroupTab(
      GURL("https://website.com"), u"Website Title",
      group_to_delete.saved_guid(), /*position=*/std::nullopt));
  model()->Add(group_to_delete);
  model()->Add(SavedTabGroup(u"title 2", tab_groups::TabGroupColorId::kGrey,
                             /*urls=*/{}, /*position=*/std::nullopt)
                   .SetCollaborationId("collaboration 2"));
  ASSERT_EQ(model()->Count(), 2);

  ApplySingleEntityChange(syncer::EntityChange::CreateDelete(
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
  group.SetCollaborationId("collaboration");
  SavedTabGroupTab tab_to_delete(GURL("https://google.com/1"), u"title 1",
                                 group.saved_guid(), /*position=*/std::nullopt);
  group.AddTabLocally(tab_to_delete);
  group.AddTabLocally(SavedTabGroupTab(GURL("https://google.com/2"), u"title 2",
                                       group.saved_guid(),
                                       /*position=*/std::nullopt));
  model()->Add(group);
  ASSERT_EQ(model()->Count(), 1);
  ASSERT_THAT(model()->saved_tab_groups().front().saved_tabs(), SizeIs(2));

  ApplySingleEntityChange(syncer::EntityChange::CreateDelete(
      tab_to_delete.saved_tab_guid().AsLowercaseString()));

  ASSERT_EQ(model()->Count(), 1);
  EXPECT_THAT(
      model()->saved_tab_groups().front().saved_tabs(),
      UnorderedElementsAre(HasTabMetadata("title 2", "https://google.com/2")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldCheckValidEntities) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  EXPECT_TRUE(bridge()->IsEntityDataValid(CreateEntityData(
      MakeTabGroupSpecifics("test title", sync_pb::SharedTabGroup::GREEN),
      "collaboration")));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldRemoveLocalGroupsOnDisableSync) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  // Initialize the model with some initial data. Create 2 entities to make it
  // sure that each of them is being deleted.
  syncer::EntityChangeList change_list;
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title", sync_pb::SharedTabGroup::RED),
      "collaboration"));
  change_list.push_back(CreateAddEntityChange(
      MakeTabGroupSpecifics("title 2", sync_pb::SharedTabGroup::GREEN),
      "collaboration"));
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
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId("collaboration");
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com", u"tab 2", group.saved_guid(), /*position=*/1);

  model()->Add(group);
  model()->AddTabToGroupLocally(group.saved_guid(), tab1);
  model()->AddTabToGroupLocally(group.saved_guid(), tab2);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);

  // Observers must be notified for closed groups and tabs to make it sure that
  // both will be closed.
  EXPECT_CALL(mock_model_observer(), SavedTabGroupRemovedFromSync);
  EXPECT_CALL(mock_model_observer(),
              SavedTabGroupUpdatedFromSync(Eq(group.saved_guid()),
                                           Eq(tab1.saved_tab_guid())));
  // TODO(crbug.com/319521964): uncomment the following line once fixed.
  // EXPECT_CALL(mock_model_observer(),
  // SavedTabGroupUpdatedFromSync(Eq(group.saved_guid()),
  // Eq(tab2.saved_tab_guid())));
  bridge()->ApplyDisableSyncChanges(bridge()->CreateMetadataChangeList());
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldReturnGroupDataForCommit) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId("collaboration");
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com", u"tab 2", group.saved_guid(), /*position=*/1);

  model()->Add(group);
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
  group.SetCollaborationId("collaboration");
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"tab 2", group.saved_guid(), /*position=*/1);

  model()->Add(group);
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

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldReturnAllDataForDebugging) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId("collaboration");
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"tab 2", group.saved_guid(), /*position=*/1);

  model()->Add(group);
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
  group.SetCollaborationId("collaboration");
  group.SetOriginatingSavedTabGroupGuid(kOriginatingSavedTabGroupGuid);
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

  model()->Add(group);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldSendToSyncUpdatedGroupMetadata) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt,
                      /*saved_guid=*/base::Uuid::GenerateRandomV4(),
                      test::GenerateRandomTabGroupID());
  group.SetCollaborationId("collaboration");
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"tab 2", group.saved_guid(), /*position=*/1);

  group.AddTabLocally(tab1);
  group.AddTabLocally(tab2);
  model()->Add(group);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);

  syncer::EntityData captured_entity_data;
  EXPECT_CALL(mock_processor(), Put)
      .WillOnce(WithArg<1>(
          Invoke([&captured_entity_data](
                     std::unique_ptr<syncer::EntityData> entity_data) {
            captured_entity_data = std::move(*entity_data);
          })));
  tab_groups::TabGroupVisualData visual_data(
      u"new title", tab_groups::TabGroupColorId::kYellow);
  model()->UpdateVisualData(group.local_group_id().value(), &visual_data);

  EXPECT_THAT(
      captured_entity_data,
      HasGroupEntityData("new title", sync_pb::SharedTabGroup_Color_YELLOW,
                         "collaboration"));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldSendToSyncNewLocalTab) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId("collaboration");
  SavedTabGroupTab tab = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);

  group.AddTabLocally(tab);
  model()->Add(group);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 1u);

  SavedTabGroupTab new_tab = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"new tab", group.saved_guid(), /*position=*/1);

  syncer::EntityData captured_entity_data;
  EXPECT_CALL(mock_processor(), Put)
      .WillOnce(WithArg<1>(
          Invoke([&captured_entity_data](
                     std::unique_ptr<syncer::EntityData> entity_data) {
            captured_entity_data = std::move(*entity_data);
          })));
  model()->AddTabToGroupLocally(group.saved_guid(), new_tab);

  EXPECT_THAT(
      captured_entity_data,
      HasTabEntityData("new tab", "http://google.com/2", "collaboration"));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldSendToSyncRemovedLocalTab) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId("collaboration");
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab_to_remove =
      test::CreateSavedTabGroupTab("http://google.com/2", u"tab to remove",
                                   group.saved_guid(), /*position=*/1);

  group.AddTabLocally(tab1);
  group.AddTabLocally(tab_to_remove);
  model()->Add(group);
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
  group.SetCollaborationId("collaboration");
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab_to_update = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"tab 2", group.saved_guid(), /*position=*/1);

  group.AddTabLocally(tab1);
  group.AddTabLocally(tab_to_update);
  model()->Add(group);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);

  syncer::EntityData captured_entity_data;
  EXPECT_CALL(mock_processor(), Put)
      .WillOnce(WithArg<1>(
          Invoke([&captured_entity_data](
                     std::unique_ptr<syncer::EntityData> entity_data) {
            captured_entity_data = std::move(*entity_data);
          })));
  tab_to_update.SetURL(GURL("http://google.com/updated"));
  tab_to_update.SetTitle(u"updated tab");
  model()->UpdateTabInGroup(group.saved_guid(), tab_to_update);

  EXPECT_THAT(captured_entity_data,
              HasTabEntityData("updated tab", "http://google.com/updated",
                               "collaboration"));
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldSendToSyncRemovedLocalGroup) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId("collaboration");
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"tab 2", group.saved_guid(), /*position=*/1);

  group.AddTabLocally(tab1);
  group.AddTabLocally(tab2);
  model()->Add(group);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);

  // Only the group is removed, its tabs remain orphaned.
  EXPECT_CALL(mock_processor(),
              Delete(group.saved_guid().AsLowercaseString(), _, _));
  EXPECT_CALL(mock_processor(),
              Delete(tab1.saved_tab_guid().AsLowercaseString(), _, _))
      .Times(0);
  EXPECT_CALL(mock_processor(),
              Delete(tab2.saved_tab_guid().AsLowercaseString(), _, _))
      .Times(0);
  model()->Remove(group.saved_guid());
}

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldReloadDataOnBrowserRestart) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  const std::string collaboration_id = "collaboration";

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(collaboration_id);
  SavedTabGroupTab tab1 = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab 1", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab2 = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"tab 2", group.saved_guid(), /*position=*/1);

  group.AddTabLocally(tab1);
  group.AddTabLocally(tab2);
  model()->Add(group);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);

  // Verify that the model is destroyed to simulate browser restart.
  StoreMetadataAndReset(collaboration_id);
  ASSERT_EQ(model(), nullptr);

  // Note that sync metadata is not checked explicitly because the collaboration
  // ID is stored as a part of sync metadata.
  ASSERT_TRUE(InitializeBridgeAndModel());
  ASSERT_THAT(
      model()->saved_tab_groups(),
      UnorderedElementsAre(HasSharedGroupMetadata(
          "title", tab_groups::TabGroupColorId::kGrey, "collaboration")));
  EXPECT_THAT(
      model()->saved_tab_groups().front().saved_tabs(),
      UnorderedElementsAre(HasTabMetadata("tab 1", "http://google.com/1"),
                           HasTabMetadata("tab 2", "http://google.com/2")));
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

  const std::string collaboration_id = "collaboration";

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(collaboration_id);
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
              "title", sync_pb::SharedTabGroup_Color_GREY, "collaboration")),
          _));

  model()->Add(group);
  ASSERT_TRUE(model()->Contains(group.saved_guid()));
  ASSERT_EQ(model()->Get(group.saved_guid())->saved_tabs().size(), 2u);
}

TEST_F(SharedTabGroupDataSyncBridgeTest,
       ShouldGenerateUniquePositionForInitialTabEntity) {
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId("collaboration");
  model()->Add(group);

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
  group.SetCollaborationId("collaboration");
  SavedTabGroupTab tab = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab", group.saved_guid(), /*position=*/0);
  group.AddTabLocally(tab);
  model()->Add(group);

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
  group.SetCollaborationId("collaboration");
  SavedTabGroupTab tab = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab", group.saved_guid(), /*position=*/0);
  group.AddTabLocally(tab);
  model()->Add(group);

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
  group.SetCollaborationId("collaboration");
  SavedTabGroupTab tab_before = test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab before", group.saved_guid(), /*position=*/0);
  SavedTabGroupTab tab_after = test::CreateSavedTabGroupTab(
      "http://google.com/2", u"tab after", group.saved_guid(), /*position=*/1);
  group.AddTabLocally(tab_before);
  group.AddTabLocally(tab_after);
  model()->Add(group);

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
  const std::string kCollaborationId = "collaboration";
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
  model()->Add(group);

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
  const std::string kCollaborationId = "collaboration";
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
  model()->Add(group);

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
  const std::string kCollaborationId = "collaboration";
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
  model()->Add(group);
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
  const std::string kCollaborationId = "collaboration";
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
  model()->Add(group);

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

TEST_F(SharedTabGroupDataSyncBridgeTest, ShouldAssignLocalGroupId) {
  const std::string kCollaborationId = "collaboration";
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(kCollaborationId);
  group.AddTabLocally(test::CreateSavedTabGroupTab(
      "http://google.com/1", u"tab", group.saved_guid(), /*position=*/0));
  model()->Add(group);

  LocalTabGroupID local_group_id = test::GenerateRandomTabGroupID();
  model()->OnGroupOpenedInTabStrip(group.saved_guid(), local_group_id);

  // Verify that the local group ID is persisted if supported.
  if (!AreLocalIdsPersisted()) {
    return;
  }

  // Simulate browser restart to verify that the local group ID is persisted.
  StoreMetadataAndReset(kCollaborationId);
  ASSERT_THAT(model(), IsNull());
  ASSERT_TRUE(InitializeBridgeAndModel());
  ASSERT_THAT(model()->Get(group.saved_guid()), NotNull());

  EXPECT_EQ(model()->Get(group.saved_guid())->local_group_id(), local_group_id);

  // Close the tab group and simulate browser restart.
  model()->OnGroupClosedInTabStrip(local_group_id);

  StoreMetadataAndReset(kCollaborationId);
  ASSERT_THAT(model(), IsNull());
  ASSERT_TRUE(InitializeBridgeAndModel());
  ASSERT_THAT(model()->Get(group.saved_guid()), NotNull());

  EXPECT_EQ(model()->Get(group.saved_guid())->local_group_id(), std::nullopt);
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
  const std::string kCollaborationId = "collaboration";
  ASSERT_TRUE(InitializeBridgeAndModel());

  SavedTabGroup group(u"title", tab_groups::TabGroupColorId::kGrey,
                      /*urls=*/{}, /*position=*/std::nullopt);
  group.SetCollaborationId(kCollaborationId);
  model()->Add(group);

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
