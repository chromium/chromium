// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/group_data_model.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "components/data_sharing/internal/collaboration_group_sync_bridge.h"
#include "components/data_sharing/public/group_data.h"
#include "components/data_sharing/test_support/fake_data_sharing_sdk_delegate.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_sharing {

namespace {

using base::test::RunClosure;
using testing::ElementsAre;
using testing::Eq;
using testing::Optional;

// TODO(crbug.com/301390275): move helpers to work with CollaborationGroup
// entities to test utils files, they are used across multiple files.
sync_pb::CollaborationGroupSpecifics MakeSpecifics(
    const GroupId& id,
    const base::Time& changed_at = base::Time::Now()) {
  sync_pb::CollaborationGroupSpecifics result;
  result.set_collaboration_id(id.value());
  result.set_changed_at_timestamp_millis_since_unix_epoch(
      changed_at.InMillisecondsSinceUnixEpoch());
  return result;
}

syncer::EntityData EntityDataFromSpecifics(
    const sync_pb::CollaborationGroupSpecifics& specifics) {
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_collaboration_group() = specifics;
  entity_data.name = specifics.collaboration_id();
  return entity_data;
}

std::unique_ptr<syncer::EntityChange> EntityChangeAddFromSpecifics(
    const sync_pb::CollaborationGroupSpecifics& specifics) {
  return syncer::EntityChange::CreateAdd(specifics.collaboration_id(),
                                         EntityDataFromSpecifics(specifics));
}

std::unique_ptr<syncer::EntityChange> EntityChangeUpdateFromSpecifics(
    const sync_pb::CollaborationGroupSpecifics& specifics) {
  return syncer::EntityChange::CreateUpdate(specifics.collaboration_id(),
                                            EntityDataFromSpecifics(specifics));
}

std::unique_ptr<syncer::EntityChange> EntityChangeDeleteFromSpecifics(
    const sync_pb::CollaborationGroupSpecifics& specifics) {
  return syncer::EntityChange::CreateDelete(specifics.collaboration_id());
}

MATCHER_P(HasDisplayName, expected_name, "") {
  return arg.display_name == expected_name;
}

MATCHER_P(HasMemberWithGaiaId, expected_gaia_id, "") {
  for (const auto& member : arg.members) {
    if (member.gaia_id == expected_gaia_id) {
      return true;
    }
  }
  return false;
}

class MockModelObserver : public GroupDataModel::Observer {
 public:
  MockModelObserver() = default;
  ~MockModelObserver() override = default;

  MOCK_METHOD(void, OnModelLoaded, (), (override));
  MOCK_METHOD(void, OnGroupAdded, (const GroupId& group_id), (override));
  MOCK_METHOD(void, OnGroupUpdated, (const GroupId& group_id), (override));
  MOCK_METHOD(void, OnGroupDeleted, (const GroupId& group_id), (override));
};

class GroupDataModelTest : public testing::Test {
 public:
  GroupDataModelTest()
      : data_type_store_(
            syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    EXPECT_TRUE(data_sharing_dir_.CreateUniqueTempDir());
  }

  ~GroupDataModelTest() override = default;

  void SetUp() override {
    base::RunLoop run_loop;
    ON_CALL(mock_processor_, ModelReadyToSync)
        .WillByDefault(RunClosure(run_loop.QuitClosure()));

    collaboration_group_bridge_ =
        std::make_unique<CollaborationGroupSyncBridge>(
            mock_processor_.CreateForwardingProcessor(),
            syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(
                data_type_store_.get()));
    run_loop.Run();

    // Mimic that initial sync is completed with no data.
    collaboration_group_bridge_->MergeFullSyncData(
        collaboration_group_bridge_->CreateMetadataChangeList(),
        syncer::EntityChangeList());

    model_ = std::make_unique<GroupDataModel>(data_sharing_dir_.GetPath(),
                                              collaboration_group_bridge_.get(),
                                              &sdk_delegate_);
    model_->AddObserver(&observer_);
  }

  void TearDown() override {
    // Needed to ensure that `data_sharing_dir_` outlives DB tasks, that runs on
    // a dedicated sequence.
    ShutdownModel();
  }

  GroupDataModel& model() { return *model_; }

  testing::NiceMock<MockModelObserver>& model_observer() { return observer_; }

  void WaitForModelLoaded() {
    if (model_->IsModelLoaded()) {
      return;
    }
    base::RunLoop run_loop;
    EXPECT_CALL(observer_, OnModelLoaded)
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    run_loop.Run();
  }

  GroupId MimicGroupAddedServerSide(const std::string& display_name) {
    const GroupId id = sdk_delegate_.AddGroupAndReturnId(display_name);

    syncer::EntityChangeList entity_changes;
    entity_changes.push_back(EntityChangeAddFromSpecifics(MakeSpecifics(id)));
    collaboration_group_bridge_->ApplyIncrementalSyncChanges(
        collaboration_group_bridge_->CreateMetadataChangeList(),
        std::move(entity_changes));

    return id;
  }

  void WaitForGroupAdded(const GroupId& group_id) {
    base::RunLoop run_loop;
    EXPECT_CALL(observer_, OnGroupAdded(group_id))
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    run_loop.Run();
  }

  void MimicMemberAddedServerSide(const GroupId& group_id,
                                  const std::string& member_gaia_id) {
    sdk_delegate_.AddMember(group_id, member_gaia_id);

    syncer::EntityChangeList entity_changes;
    entity_changes.push_back(
        EntityChangeUpdateFromSpecifics(MakeSpecifics(group_id)));
    collaboration_group_bridge_->ApplyIncrementalSyncChanges(
        collaboration_group_bridge_->CreateMetadataChangeList(),
        std::move(entity_changes));
  }

  void WaitForGroupUpdated(const GroupId& group_id) {
    base::RunLoop run_loop;
    EXPECT_CALL(observer_, OnGroupUpdated(group_id))
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    run_loop.Run();
  }

  void MimicGroupDeletedServerSide(const GroupId& group_id) {
    sdk_delegate_.RemoveGroup(group_id);

    syncer::EntityChangeList entity_changes;
    entity_changes.push_back(
        EntityChangeDeleteFromSpecifics(MakeSpecifics(group_id)));
    collaboration_group_bridge_->ApplyIncrementalSyncChanges(
        collaboration_group_bridge_->CreateMetadataChangeList(),
        std::move(entity_changes));
  }

  void ShutdownModel() {
    base::RunLoop run_loop;
    model_->GetGroupDataStoreForTesting().SetShutdownCallbackForTesting(
        run_loop.QuitClosure());
    model_->RemoveObserver(&observer_);
    model_.reset();

    // Wait for DB shutdown tasks completion.
    run_loop.Run();
  }

  void RestartModel() {
    model_ = std::make_unique<GroupDataModel>(data_sharing_dir_.GetPath(),
                                              collaboration_group_bridge_.get(),
                                              &sdk_delegate_);
    model_->AddObserver(&observer_);
  }

 private:
  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir data_sharing_dir_;

  std::unique_ptr<syncer::DataTypeStore> data_type_store_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<CollaborationGroupSyncBridge> collaboration_group_bridge_;

  FakeDataSharingSDKDelegate sdk_delegate_;
  std::unique_ptr<GroupDataModel> model_;

  testing::NiceMock<MockModelObserver> observer_;
};

TEST_F(GroupDataModelTest, ShouldGetGroup) {
  WaitForModelLoaded();
  EXPECT_FALSE(model().GetGroup(GroupId("non-existing-group-id")).has_value());

  const std::string group_display_name = "group";
  const GroupId group_id = MimicGroupAddedServerSide(group_display_name);
  WaitForGroupAdded(group_id);

  EXPECT_THAT(model().GetGroup(group_id),
              Optional(HasDisplayName(group_display_name)));
}

TEST_F(GroupDataModelTest, ShouldGetAllGroups) {
  WaitForModelLoaded();

  EXPECT_TRUE(model().GetAllGroups().empty());

  const std::string group_display_name1 = "group1";
  const GroupId group_id1 = MimicGroupAddedServerSide(group_display_name1);
  WaitForGroupAdded(group_id1);
  EXPECT_THAT(model().GetAllGroups(),
              ElementsAre(HasDisplayName(group_display_name1)));

  const std::string group_display_name2 = "group2";
  const GroupId group_id2 = MimicGroupAddedServerSide(group_display_name2);
  WaitForGroupAdded(group_id2);
  EXPECT_THAT(model().GetAllGroups(),
              ElementsAre(HasDisplayName(group_display_name1),
                          HasDisplayName(group_display_name2)));
}

TEST_F(GroupDataModelTest, ShouldUpdateGroup) {
  WaitForModelLoaded();

  const GroupId group_id = MimicGroupAddedServerSide("group");
  WaitForGroupAdded(group_id);

  const std::string member_gaia_id = "gaia_id";
  MimicMemberAddedServerSide(group_id, member_gaia_id);
  WaitForGroupUpdated(group_id);

  EXPECT_THAT(model().GetGroup(group_id),
              Optional(HasMemberWithGaiaId(member_gaia_id)));
}

TEST_F(GroupDataModelTest, ShouldDeleteGroup) {
  WaitForModelLoaded();

  const GroupId group_id = MimicGroupAddedServerSide("group");
  WaitForGroupAdded(group_id);
  ASSERT_TRUE(model().GetGroup(group_id).has_value());

  // Unlike additions/updates deletions are handled synchronously, once
  // CollaborationGroupSyncBridge received the update - no need to wait for
  // observer call with RunLoop.
  EXPECT_CALL(model_observer(), OnGroupDeleted(group_id));
  MimicGroupDeletedServerSide(group_id);

  EXPECT_FALSE(model().GetGroup(group_id).has_value());
}

TEST_F(GroupDataModelTest, ShouldPersistDataAcrossRestart) {
  WaitForModelLoaded();

  const std::string group_display_name = "group";
  const GroupId group_id = MimicGroupAddedServerSide(group_display_name);
  WaitForGroupAdded(group_id);
  ASSERT_THAT(model().GetGroup(group_id),
              Optional(HasDisplayName(group_display_name)));

  ShutdownModel();
  RestartModel();
  WaitForModelLoaded();

  EXPECT_THAT(model().GetGroup(group_id),
              Optional(HasDisplayName(group_display_name)));
}

TEST_F(GroupDataModelTest, ShouldHandleNewGroupsAfterRestart) {
  WaitForModelLoaded();
  ShutdownModel();

  // Mimic that new group addition was only partially handled:
  // CollaborationGroupSyncBridge is still running and will persist changes, but
  // model is shut down so it can't process them.
  const std::string group_display_name = "group";
  const GroupId group_id = MimicGroupAddedServerSide(group_display_name);
  RestartModel();
  WaitForModelLoaded();

  WaitForGroupAdded(group_id);
  EXPECT_THAT(model().GetGroup(group_id),
              Optional(HasDisplayName(group_display_name)));
}

TEST_F(GroupDataModelTest, ShouldHandleUpdatesAfterRestart) {
  WaitForModelLoaded();

  const std::string group_display_name = "group";
  const GroupId group_id = MimicGroupAddedServerSide(group_display_name);
  WaitForGroupAdded(group_id);
  ASSERT_THAT(model().GetGroup(group_id),
              Optional(HasDisplayName(group_display_name)));

  // Mimic that new group addition was only partially handled:
  // CollaborationGroupSyncBridge is still running and will persist changes, but
  // model is shut down so it can't process them.
  ShutdownModel();
  const std::string member_gaia_id = "gaia_id";
  MimicMemberAddedServerSide(group_id, member_gaia_id);

  RestartModel();
  WaitForModelLoaded();

  WaitForGroupUpdated(group_id);
  EXPECT_THAT(model().GetGroup(group_id),
              Optional(HasMemberWithGaiaId(member_gaia_id)));
}

TEST_F(GroupDataModelTest, ShouldHandleDeletionsAfterRestart) {
  WaitForModelLoaded();

  const std::string group_display_name = "group";
  const GroupId group_id = MimicGroupAddedServerSide(group_display_name);
  WaitForGroupAdded(group_id);
  ASSERT_THAT(model().GetGroup(group_id),
              Optional(HasDisplayName(group_display_name)));

  ShutdownModel();
  // Mimic that deletion was only partially handled:
  // CollaborationGroupSyncBridge is still running and will persist changes, but
  // model is shut down so it can't process them.
  MimicGroupDeletedServerSide(group_id);

  RestartModel();
  WaitForModelLoaded();

  EXPECT_FALSE(model().GetGroup(group_id).has_value());
}

}  // namespace
}  // namespace data_sharing
