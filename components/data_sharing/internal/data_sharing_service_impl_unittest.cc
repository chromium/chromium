// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/data_sharing_service_impl.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/version_info/channel.h"
#include "components/data_sharing/public/data_sharing_sdk_delegate.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/protocol/data_sharing_sdk.pb.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/protocol/collaboration_group_specifics.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/test/model_type_store_test_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/status/status.h"

namespace data_sharing {

namespace {

using base::test::RunClosure;
using testing::Eq;

sync_pb::CollaborationGroupSpecifics MakeCollaborationGroupSpecifics(
    const std::string& id) {
  sync_pb::CollaborationGroupSpecifics result;
  result.set_collaboration_id(id);
  result.set_last_updated_timestamp_millis_since_unix_epoch(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
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

class MockObserver : public DataSharingService::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;

  MOCK_METHOD(void, OnGroupChanged, (const GroupData&), (override));
  MOCK_METHOD(void, OnGroupAdded, (const GroupData&), (override));
  MOCK_METHOD(void, OnGroupRemoved, (const std::string&), (override));
};

class FakeDataSharingSDKDelegate : public DataSharingSDKDelegate {
 public:
  FakeDataSharingSDKDelegate() = default;
  ~FakeDataSharingSDKDelegate() override = default;

  // Convenience methods for testing.
  std::optional<data_sharing_pb::GroupData> GetGroup(
      const std::string& group_id) {
    auto it = groups_.find(group_id);
    if (it != groups_.end()) {
      return it->second;
    }
    return std::nullopt;
  }

  void RemoveGroup(const std::string& group_id) { groups_.erase(group_id); }

  void UpdateGroup(const std::string& group_id,
                   const std::string& new_display_name) {
    auto it = groups_.find(group_id);
    ASSERT_TRUE(it != groups_.end());

    it->second.set_display_name(new_display_name);
  }

  std::string AddGroupAndReturnId(const std::string& display_name) {
    data_sharing_pb::GroupData group_data;
    group_data.set_group_id(base::NumberToString(next_group_id_++));
    group_data.set_display_name(display_name);
    groups_[group_data.group_id()] = group_data;
    return group_data.group_id();
  }

  void AddMember(const std::string& group_id,
                 const std::string& member_gaia_id) {
    auto group_it = groups_.find(group_id);
    ASSERT_TRUE(group_it != groups_.end());

    data_sharing_pb::GroupMember member;
    member.set_gaia_id(member_gaia_id);
    *group_it->second.add_members() = member;
  }

  void AddAccount(const std::string& email, const std::string& gaia_id) {
    email_to_gaia_id_[email] = gaia_id;
  }

  // DataSharingSDKDelegate implementation.
  void CreateGroup(
      const data_sharing_pb::CreateGroupParams& params,
      base::OnceCallback<
          void(const base::expected<data_sharing_pb::CreateGroupResult,
                                    absl::Status>&)> callback) override {
    std::string group_id = AddGroupAndReturnId(params.display_name());

    data_sharing_pb::CreateGroupResult result;
    *result.mutable_group_data() = groups_[group_id];

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result));
  }

  void ReadGroups(const data_sharing_pb::ReadGroupsParams& params,
                  base::OnceCallback<void(
                      const base::expected<data_sharing_pb::ReadGroupsResult,
                                           absl::Status>&)> callback) override {
    data_sharing_pb::ReadGroupsResult result;
    for (const auto& group_id : params.group_ids()) {
      if (groups_.find(group_id) != groups_.end()) {
        *result.add_group_data() = groups_[group_id];
      }
    }

    if (result.group_data().empty()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), base::unexpected(absl::Status(
                                                  absl::StatusCode::kNotFound,
                                                  "Groups not found"))));
      return;
    }

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result));
  }

  void AddMember(
      const data_sharing_pb::AddMemberParams& params,
      base::OnceCallback<void(const absl::Status&)> callback) override {
    auto group_it = groups_.find(params.group_id());
    absl::Status status = absl::OkStatus();
    if (group_it != groups_.end()) {
      data_sharing_pb::GroupMember member;
      member.set_gaia_id(params.member_gaia_id());
      *group_it->second.add_members() = member;
    } else {
      status = absl::Status(absl::StatusCode::kNotFound, "Group not found");
    }

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), status));
  }

  void RemoveMember(
      const data_sharing_pb::RemoveMemberParams& params,
      base::OnceCallback<void(const absl::Status&)> callback) override {
    auto group_it = groups_.find(params.group_id());
    if (group_it == groups_.end()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback),
                                    absl::Status(absl::StatusCode::kNotFound,
                                                 "Group not found")));
      return;
    }

    absl::Status status = absl::OkStatus();
    auto* group_members = group_it->second.mutable_members();
    auto member_it =
        std::find_if(group_members->begin(), group_members->end(),
                     [&params](const data_sharing_pb::GroupMember& member) {
                       return member.gaia_id() == params.member_gaia_id();
                     });
    if (member_it != group_members->end()) {
      group_members->erase(member_it);
    } else {
      status = absl::Status(absl::StatusCode::kNotFound, "Member not found");
    }

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), status));
  }

  void DeleteGroup(
      const data_sharing_pb::DeleteGroupParams& params,
      base::OnceCallback<void(const absl::Status&)> callback) override {
    absl::Status status = absl::OkStatus();
    if (groups_.find(params.group_id()) != groups_.end()) {
      groups_.erase(params.group_id());
    } else {
      status = absl::Status(absl::StatusCode::kNotFound, "Group not found");
    }

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), status));
  }

  void LookupGaiaIdByEmail(
      const data_sharing_pb::LookupGaiaIdByEmailParams& params,
      base::OnceCallback<
          void(const base::expected<data_sharing_pb::LookupGaiaIdByEmailResult,
                                    absl::Status>&)> callback) override {
    auto it = email_to_gaia_id_.find(params.email());
    if (it != email_to_gaia_id_.end()) {
      data_sharing_pb::LookupGaiaIdByEmailResult result;
      result.set_gaia_id(it->second);

      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), result));
      return;
    }

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       base::unexpected(absl::Status(
                           absl::StatusCode::kNotFound, "Account not found"))));
  }

 private:
  std::map<std::string, data_sharing_pb::GroupData> groups_;
  std::map<std::string, std::string> email_to_gaia_id_;
  int next_group_id_ = 0;
};

}  // namespace

class DataSharingServiceImplTest : public testing::Test {
 public:
  DataSharingServiceImplTest() = default;

  ~DataSharingServiceImplTest() override = default;

  void SetUp() override {
    scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    std::unique_ptr<FakeDataSharingSDKDelegate> sdk_delegate =
        std::make_unique<FakeDataSharingSDKDelegate>();
    not_owned_sdk_delegate_ = sdk_delegate.get();

    data_sharing_service_ = std::make_unique<DataSharingServiceImpl>(
        std::move(test_url_loader_factory),
        identity_test_env_.identity_manager(),
        syncer::ModelTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
        version_info::Channel::UNKNOWN, std::move(sdk_delegate));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<DataSharingServiceImpl> data_sharing_service_;
  raw_ptr<FakeDataSharingSDKDelegate> not_owned_sdk_delegate_;
};

TEST_F(DataSharingServiceImplTest, ConstructionAndEmptyServiceCheck) {
  EXPECT_FALSE(data_sharing_service_->IsEmptyService());
}

TEST_F(DataSharingServiceImplTest, GetDataSharingNetworkLoader) {
  EXPECT_TRUE(data_sharing_service_->GetDataSharingNetworkLoader());
}

TEST_F(DataSharingServiceImplTest, ShouldCreateGroup) {
  // TODO(crbug.com/301390275): add a version of this test for unhappy path.
  const std::string display_name = "display_name";

  DataSharingService::GroupDataOrFailureOutcome outcome;
  base::RunLoop run_loop;
  data_sharing_service_->CreateGroup(
      display_name,
      base::BindLambdaForTesting(
          [&run_loop, &outcome](
              const DataSharingService::GroupDataOrFailureOutcome& result) {
            outcome = result;
            run_loop.Quit();
          }));
  run_loop.Run();

  ASSERT_TRUE(outcome.has_value());
  EXPECT_THAT(outcome->display_name, Eq(display_name));

  ASSERT_TRUE(not_owned_sdk_delegate_->GetGroup(outcome->group_id).has_value());
  EXPECT_THAT(
      not_owned_sdk_delegate_->GetGroup(outcome->group_id)->display_name(),
      Eq(display_name));
}

TEST_F(DataSharingServiceImplTest, ShouldDeleteGroup) {
  // TODO(crbug.com/301390275): add a version of this test for unhappy path.
  const std::string group_id =
      not_owned_sdk_delegate_->AddGroupAndReturnId("display_name");
  ASSERT_TRUE(not_owned_sdk_delegate_->GetGroup(group_id).has_value());

  base::RunLoop run_loop;
  base::MockOnceCallback<void(DataSharingService::PeopleGroupActionOutcome)>
      callback;
  EXPECT_CALL(callback,
              Run(DataSharingService::PeopleGroupActionOutcome::kSuccess))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  data_sharing_service_->DeleteGroup(group_id, callback.Get());
  run_loop.Run();

  EXPECT_FALSE(not_owned_sdk_delegate_->GetGroup(group_id).has_value());
}

TEST_F(DataSharingServiceImplTest, ShouldReadGroup) {
  // TODO(crbug.com/301390275): add a version of this test for unhappy path.
  const std::string display_name = "display_name";
  const std::string group_id =
      not_owned_sdk_delegate_->AddGroupAndReturnId(display_name);

  DataSharingService::GroupDataOrFailureOutcome outcome;
  base::RunLoop run_loop;
  data_sharing_service_->ReadGroup(
      group_id,
      base::BindLambdaForTesting(
          [&run_loop, &outcome](
              const DataSharingService::GroupDataOrFailureOutcome& result) {
            outcome = result;
            run_loop.Quit();
          }));
  run_loop.Run();

  ASSERT_TRUE(outcome.has_value());
  EXPECT_THAT(outcome->display_name, Eq(display_name));
  EXPECT_THAT(outcome->group_id, Eq(group_id));
}

TEST_F(DataSharingServiceImplTest, ShouldReadAllGroups) {
  // TODO(crbug.com/301390275): add a version of this test for unhappy path.
  // Delegate stores 2 groups.
  const std::string display_name1 = "group1";
  const std::string group_id1 =
      not_owned_sdk_delegate_->AddGroupAndReturnId(display_name1);
  const std::string display_name2 = "group2";
  const std::string group_id2 =
      not_owned_sdk_delegate_->AddGroupAndReturnId(display_name2);

  // Mimics initial sync for collaboration group datatype with the same two
  // groups.
  auto* collaboration_group_bridge =
      data_sharing_service_->GetCollaborationGroupSyncBridgeForTesting();

  syncer::EntityChangeList entity_changes;
  entity_changes.push_back(
      EntityChangeAddFromSpecifics(MakeCollaborationGroupSpecifics(group_id1)));
  entity_changes.push_back(
      EntityChangeAddFromSpecifics(MakeCollaborationGroupSpecifics(group_id2)));

  collaboration_group_bridge->MergeFullSyncData(
      collaboration_group_bridge->CreateMetadataChangeList(),
      std::move(entity_changes));

  // Verify that DataSharingService reads the same groups.
  DataSharingService::GroupsDataSetOrFailureOutcome outcome;
  base::RunLoop run_loop;
  data_sharing_service_->ReadAllGroups(base::BindLambdaForTesting(
      [&run_loop, &outcome](
          const DataSharingService::GroupsDataSetOrFailureOutcome& result) {
        outcome = result;
        run_loop.Quit();
      }));
  run_loop.Run();

  ASSERT_TRUE(outcome.has_value());
  EXPECT_THAT(outcome->size(), Eq(2));
  const GroupData& group1 = *outcome->begin();
  const GroupData& group2 = *(++outcome->begin());
  EXPECT_THAT(group1.display_name, Eq(display_name1));
  EXPECT_THAT(group1.group_id, Eq(group_id1));
  EXPECT_THAT(group2.display_name, Eq(display_name2));
  EXPECT_THAT(group2.group_id, Eq(group_id2));
}

TEST_F(DataSharingServiceImplTest, ShouldInviteMember) {
  // TODO(crbug.com/301390275): add a version of this test for unhappy paths.
  const std::string group_id =
      not_owned_sdk_delegate_->AddGroupAndReturnId("display_name");

  const std::string email = "user@gmail.com";
  const std::string gaia_id = "123456789";
  not_owned_sdk_delegate_->AddAccount(email, gaia_id);

  base::RunLoop run_loop;
  base::MockOnceCallback<void(DataSharingService::PeopleGroupActionOutcome)>
      callback;
  EXPECT_CALL(callback,
              Run(DataSharingService::PeopleGroupActionOutcome::kSuccess))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  data_sharing_service_->InviteMember(group_id, email, callback.Get());
  run_loop.Run();

  auto group = not_owned_sdk_delegate_->GetGroup(group_id);
  ASSERT_TRUE(group.has_value());
  ASSERT_THAT(group->members().size(), Eq(1));
  EXPECT_THAT(group->members(0).gaia_id(), Eq(gaia_id));
}

TEST_F(DataSharingServiceImplTest, ShouldRemoveMember) {
  // TODO(crbug.com/301390275): add a version of this test for unhappy paths.
  const std::string group_id =
      not_owned_sdk_delegate_->AddGroupAndReturnId("display_name");

  const std::string email = "user@gmail.com";
  const std::string gaia_id = "123456789";
  not_owned_sdk_delegate_->AddAccount(email, gaia_id);
  not_owned_sdk_delegate_->AddMember(group_id, gaia_id);

  base::RunLoop run_loop;
  base::MockOnceCallback<void(DataSharingService::PeopleGroupActionOutcome)>
      callback;
  EXPECT_CALL(callback,
              Run(DataSharingService::PeopleGroupActionOutcome::kSuccess))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  data_sharing_service_->RemoveMember(group_id, email, callback.Get());
  run_loop.Run();

  auto group = not_owned_sdk_delegate_->GetGroup(group_id);
  ASSERT_TRUE(group.has_value());
  EXPECT_TRUE(group->members().empty());
}

TEST_F(DataSharingServiceImplTest, ShouldNotifyObserverOnGroupAddition) {
  const std::string display_name = "display_name";
  const std::string group_id =
      not_owned_sdk_delegate_->AddGroupAndReturnId(display_name);

  base::RunLoop run_loop;
  testing::NiceMock<MockObserver> observer;
  data_sharing_service_->AddObserver(&observer);

  EXPECT_CALL(observer, OnGroupAdded(HasDisplayName(display_name)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  // Mimics initial sync for collaboration group datatype, this should trigger
  // OnGroupAdded() notification.
  auto* collaboration_group_bridge =
      data_sharing_service_->GetCollaborationGroupSyncBridgeForTesting();

  syncer::EntityChangeList entity_changes;
  entity_changes.push_back(
      EntityChangeAddFromSpecifics(MakeCollaborationGroupSpecifics(group_id)));

  collaboration_group_bridge->MergeFullSyncData(
      collaboration_group_bridge->CreateMetadataChangeList(),
      std::move(entity_changes));

  run_loop.Run();
}

TEST_F(DataSharingServiceImplTest, ShouldNotifyObserverOnGroupRemoval) {
  const std::string group_id =
      not_owned_sdk_delegate_->AddGroupAndReturnId("display_name");
  auto* collaboration_group_bridge =
      data_sharing_service_->GetCollaborationGroupSyncBridgeForTesting();
  // Mimics initial sync for collaboration group datatype.
  {
    syncer::EntityChangeList entity_changes;
    entity_changes.push_back(EntityChangeAddFromSpecifics(
        MakeCollaborationGroupSpecifics(group_id)));

    collaboration_group_bridge->MergeFullSyncData(
        collaboration_group_bridge->CreateMetadataChangeList(),
        std::move(entity_changes));
  }

  // Mimics group removal.
  testing::NiceMock<MockObserver> observer;
  data_sharing_service_->AddObserver(&observer);
  EXPECT_CALL(observer, OnGroupRemoved(group_id));

  not_owned_sdk_delegate_->RemoveGroup(group_id);
  {
    syncer::EntityChangeList entity_changes;
    entity_changes.push_back(EntityChangeDeleteFromSpecifics(
        MakeCollaborationGroupSpecifics(group_id)));

    collaboration_group_bridge->ApplyIncrementalSyncChanges(
        collaboration_group_bridge->CreateMetadataChangeList(),
        std::move(entity_changes));
  }
}

TEST_F(DataSharingServiceImplTest, ShouldNotifyObserverOnGroupChange) {
  const std::string group_id =
      not_owned_sdk_delegate_->AddGroupAndReturnId("display_name");
  auto* collaboration_group_bridge =
      data_sharing_service_->GetCollaborationGroupSyncBridgeForTesting();
  // Mimics initial sync for collaboration group datatype.
  {
    syncer::EntityChangeList entity_changes;
    entity_changes.push_back(EntityChangeAddFromSpecifics(
        MakeCollaborationGroupSpecifics(group_id)));

    collaboration_group_bridge->MergeFullSyncData(
        collaboration_group_bridge->CreateMetadataChangeList(),
        std::move(entity_changes));
  }

  // Mimics group update.
  base::RunLoop run_loop;
  testing::NiceMock<MockObserver> observer;
  data_sharing_service_->AddObserver(&observer);

  const std::string new_display_name = "new_display_name";
  EXPECT_CALL(observer, OnGroupChanged(HasDisplayName(new_display_name)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  not_owned_sdk_delegate_->UpdateGroup(group_id, new_display_name);
  {
    syncer::EntityChangeList entity_changes;
    entity_changes.push_back(EntityChangeUpdateFromSpecifics(
        MakeCollaborationGroupSpecifics(group_id)));

    collaboration_group_bridge->ApplyIncrementalSyncChanges(
        collaboration_group_bridge->CreateMetadataChangeList(),
        std::move(entity_changes));
  }
  run_loop.Run();
}

}  // namespace data_sharing
