// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/data_sharing_service_impl.h"

#include <memory>
#include <optional>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/version_info/channel.h"
#include "components/data_sharing/public/data_sharing_sdk_delegate.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/data_sharing_ui_delegate.h"
#include "components/data_sharing/public/features.h"
#include "components/data_sharing/public/group_data.h"
#include "components/data_sharing/public/protocol/data_sharing_sdk.pb.h"
#include "components/data_sharing/public/protocol/group_data.pb.h"
#include "components/data_sharing/test_support/fake_data_sharing_sdk_delegate.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/protocol/collaboration_group_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "google_apis/gaia/gaia_id.h"
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

const char kGroupId[] = "/?-group_id";
const char kEncodedGroupId[] = "%2F%3F-group_id";
const char kTokenBlob[] = "/?-_token";
const char kEncodedTokenBlob[] = "%2F%3F-_token";

// Enum cases for parameterizing the tests.
enum Variant {
  kDelegateAtCreation,
  kDelegateAfter,
};

}  // namespace

class DataSharingServiceImplTest : public ::testing::TestWithParam<Variant> {
 public:
  DataSharingServiceImplTest() = default;

  ~DataSharingServiceImplTest() override { task_environment_.RunUntilIdle(); }

  void SetUp() override {
    EXPECT_TRUE(profile_dir_.CreateUniqueTempDir());

    scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    std::unique_ptr<FakeDataSharingSDKDelegate> sdk_delegate;
    if (!ShouldSetSDKLater()) {
      sdk_delegate = std::make_unique<FakeDataSharingSDKDelegate>();
      not_owned_sdk_delegate_ = sdk_delegate.get();
    }

    data_sharing_service_ = std::make_unique<DataSharingServiceImpl>(
        profile_dir_.GetPath(), std::move(test_url_loader_factory),
        identity_test_env_.identity_manager(),
        syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
        version_info::Channel::UNKNOWN, std::move(sdk_delegate),
        /*ui_delegate=*/nullptr);

    if (ShouldSetSDKLater()) {
      sdk_delegate = std::make_unique<FakeDataSharingSDKDelegate>();
      not_owned_sdk_delegate_ = sdk_delegate.get();
      data_sharing_service_->SetSDKDelegate(std::move(sdk_delegate));
    }
  }

 protected:
  // Returns whether the SDK delegate should be set after the DataSharingService
  // creation.
  bool ShouldSetSDKLater() {
    switch (GetParam()) {
      case kDelegateAtCreation:
        return false;
      case kDelegateAfter:
        return true;
    }
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir profile_dir_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<DataSharingServiceImpl> data_sharing_service_;
  raw_ptr<FakeDataSharingSDKDelegate> not_owned_sdk_delegate_;
};

TEST_P(DataSharingServiceImplTest, ConstructionAndEmptyServiceCheck) {
  EXPECT_FALSE(data_sharing_service_->IsEmptyService());
}

TEST_P(DataSharingServiceImplTest, GetDataSharingNetworkLoader) {
  EXPECT_TRUE(data_sharing_service_->GetDataSharingNetworkLoader());
}

TEST_P(DataSharingServiceImplTest, ShouldCreateGroup) {
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

  ASSERT_TRUE(not_owned_sdk_delegate_->GetGroup(outcome->group_token.group_id)
                  .has_value());
  EXPECT_THAT(not_owned_sdk_delegate_->GetGroup(outcome->group_token.group_id)
                  ->display_name(),
              Eq(display_name));
}

TEST_P(DataSharingServiceImplTest, ShouldDeleteGroup) {
  // TODO(crbug.com/301390275): add a version of this test for unhappy path.
  const GroupId group_id =
      not_owned_sdk_delegate_->AddGroupAndReturnId("display_name");
  std::optional<data_sharing_pb::GroupData> group_data_pb =
      not_owned_sdk_delegate_->GetGroup(group_id);
  ASSERT_TRUE(group_data_pb.has_value());

  EXPECT_FALSE(data_sharing_service_->IsLeavingOrDeletingGroup(group_id));

  base::RunLoop run_loop;
  base::MockOnceCallback<void(DataSharingService::PeopleGroupActionOutcome)>
      callback;
  EXPECT_CALL(callback,
              Run(DataSharingService::PeopleGroupActionOutcome::kSuccess))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  data_sharing_service_->DeleteGroup(group_id, callback.Get());
  run_loop.Run();

  EXPECT_FALSE(not_owned_sdk_delegate_->GetGroup(group_id).has_value());

  // The group should still be available for lookup through a specific API.
  // The GroupDataModel informs the service about deletions, so we invoke that
  // method here to mimic that behavior.
  std::optional<GroupData> group_data = std::make_optional<GroupData>();
  group_data->group_token = GroupToken(group_id, "");
  group_data->display_name = group_data_pb->display_name();
  data_sharing_service_->OnGroupDeleted(group_id, group_data, base::Time());
  std::optional<GroupData> retrieved_group_data =
      data_sharing_service_->GetPossiblyRemovedGroup(group_id);
  ASSERT_TRUE(retrieved_group_data.has_value());
  EXPECT_EQ(retrieved_group_data->group_token.group_id,
            group_data->group_token.group_id);
  EXPECT_EQ(retrieved_group_data->display_name, group_data->display_name);
  EXPECT_TRUE(data_sharing_service_->IsLeavingOrDeletingGroup(group_id));
}

TEST_P(DataSharingServiceImplTest, ShouldReadGroup) {
  // TODO(crbug.com/382036119): tested API is deprecated, removed this test once
  // there are no callers.
  const std::string display_name = "display_name";
  const GroupId group_id =
      not_owned_sdk_delegate_->AddGroupAndReturnId(display_name);

  DataSharingService::GroupDataOrFailureOutcome outcome;
  base::RunLoop run_loop;
  data_sharing_service_->ReadGroupDeprecated(
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
  EXPECT_THAT(outcome->group_token.group_id, Eq(group_id));
}

TEST_P(DataSharingServiceImplTest, ShouldReadNewGroup) {
  const std::string display_name = "display_name";

  const GroupId group_id =
      not_owned_sdk_delegate_->AddGroupAndReturnId(display_name);
  const GroupToken group_token = GroupToken(group_id, "access_token");
  DataSharingService::GroupDataOrFailureOutcome outcome;
  base::RunLoop run_loop;
  data_sharing_service_->ReadNewGroup(
      group_token,
      base::BindLambdaForTesting(
          [&run_loop, &outcome](
              const DataSharingService::GroupDataOrFailureOutcome& result) {
            outcome = result;
            run_loop.Quit();
          }));
  run_loop.Run();

  ASSERT_TRUE(outcome.has_value());
  EXPECT_THAT(outcome->display_name, Eq(display_name));
  EXPECT_THAT(outcome->group_token.group_id, Eq(group_id));
}

TEST_P(DataSharingServiceImplTest, ReadNewGroupFailure) {
  const std::string display_name = "display_name";
  const GroupToken group_token =
      GroupToken(GroupId("missing_id"), "access_token");
  DataSharingService::GroupDataOrFailureOutcome outcome;
  base::RunLoop run_loop;
  data_sharing_service_->ReadNewGroup(
      group_token,
      base::BindLambdaForTesting(
          [&run_loop, &outcome](
              const DataSharingService::GroupDataOrFailureOutcome& result) {
            outcome = result;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_EQ(outcome.error(),
            DataSharingService::PeopleGroupActionFailure::kPersistentFailure);
}

TEST_P(DataSharingServiceImplTest, ShouldInviteMember) {
  // TODO(crbug.com/301390275): add a version of this test for unhappy paths.
  const GroupId group_id =
      not_owned_sdk_delegate_->AddGroupAndReturnId("display_name");

  const std::string email = "user@gmail.com";
  const GaiaId gaia_id("123456789");
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
  EXPECT_THAT(group->members(0).gaia_id(), Eq(gaia_id.ToString()));
}

TEST_P(DataSharingServiceImplTest, ShouldRemoveMember) {
  // TODO(crbug.com/301390275): add a version of this test for unhappy paths.
  const GroupId group_id =
      not_owned_sdk_delegate_->AddGroupAndReturnId("display_name");

  const std::string email = "user@gmail.com";
  const GaiaId gaia_id("123456789");
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

TEST_P(DataSharingServiceImplTest, ShouldLeaveGroup) {
  const GroupId group_id =
      not_owned_sdk_delegate_->AddGroupAndReturnId("display_name");
  EXPECT_FALSE(data_sharing_service_->IsLeavingOrDeletingGroup(group_id));

  const std::string email = "user@gmail.com";
  const GaiaId gaia_id("123456789");
  not_owned_sdk_delegate_->SetUserGaiaId(gaia_id);
  not_owned_sdk_delegate_->AddAccount(email, gaia_id);
  not_owned_sdk_delegate_->AddMember(group_id, gaia_id);

  base::RunLoop run_loop;
  base::MockOnceCallback<void(DataSharingService::PeopleGroupActionOutcome)>
      callback;
  EXPECT_CALL(callback,
              Run(DataSharingService::PeopleGroupActionOutcome::kSuccess))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  data_sharing_service_->LeaveGroup(group_id, callback.Get());
  run_loop.Run();

  auto group = not_owned_sdk_delegate_->GetGroup(group_id);
  ASSERT_TRUE(group.has_value());
  EXPECT_TRUE(group->members().empty());
  EXPECT_TRUE(data_sharing_service_->IsLeavingOrDeletingGroup(group_id));
}

TEST_P(DataSharingServiceImplTest, ShouldNotifyOnSyncBridgeUpdateTypeChanged) {
  data_sharing_service_->OnSyncBridgeUpdateTypeChanged(
      SyncBridgeUpdateType::kDisableSync);
}

TEST_P(DataSharingServiceImplTest, GetDataSharingUrl) {
  GroupData group_data = GroupData();
  group_data.group_token =
      GroupToken(data_sharing::GroupId(kGroupId), kTokenBlob);
  GURL url = GURL(data_sharing::features::kDataSharingURL.Get() +
                  "?g=" + kEncodedGroupId + "&t=" + kEncodedTokenBlob);

  std::unique_ptr<GURL> result_url =
      data_sharing_service_->GetDataSharingUrl(group_data);

  // Verify valid path.
  EXPECT_TRUE(result_url);
  EXPECT_EQ(url, *result_url);

  // Verify invalid group data.
  result_url = data_sharing_service_->GetDataSharingUrl(GroupData());
  EXPECT_FALSE(result_url);
}

TEST_P(DataSharingServiceImplTest, AddGroupDataForTesting) {
  data_sharing::GroupId group_id = data_sharing::GroupId(kGroupId);

  const GaiaId gaia_id("gaia_id");
  const std::string display_name = "Invitee Display Name";
  const std::string email = "invitee@mail.com";
  const MemberRole role = MemberRole::kInvitee;
  const GURL avatar_url = GURL("chrome://newtab");
  const std::string given_name = "Invitee Given Name";
  const std::string access_token = "fake_access_token";

  const GaiaId gaia_id2("gaia_id2");
  const std::string display_name2 = "Former Member Display Name";
  const std::string email2 = "former_member@mail.com";
  const MemberRole role2 = MemberRole::kFormerMember;
  const GURL avatar_url2 = GURL("chrome://newtab");
  const std::string given_name2 = "Former Member Given Name";

  GroupMember group_member =
      GroupMember(gaia_id, display_name, email, role, avatar_url, given_name);
  GroupMember former_group_member = GroupMember(
      gaia_id2, display_name2, email2, role2, avatar_url2, given_name2);
  GroupData group_data = GroupData(group_id, display_name, {group_member},
                                   {former_group_member}, access_token);

  data_sharing_service_->AddGroupDataForTesting(std::move(group_data));

  std::optional<GroupData> returned_group_data =
      data_sharing_service_->ReadGroup(group_id);

  EXPECT_EQ(returned_group_data->members.size(), 1u);
  EXPECT_EQ(returned_group_data->display_name, display_name);
  EXPECT_EQ(returned_group_data->group_token.group_id, group_id);
  EXPECT_EQ(returned_group_data->group_token.access_token, access_token);
  EXPECT_EQ(returned_group_data->members[0].gaia_id, gaia_id);
  EXPECT_EQ(returned_group_data->members[0].display_name, display_name);
  EXPECT_EQ(returned_group_data->members[0].email, email);
  EXPECT_EQ(returned_group_data->members[0].role, role);
  EXPECT_EQ(returned_group_data->members[0].avatar_url, avatar_url);
  EXPECT_EQ(returned_group_data->members[0].given_name, given_name);

  EXPECT_EQ(returned_group_data->former_members.size(), 1u);
  EXPECT_EQ(returned_group_data->former_members[0].gaia_id, gaia_id2);
  EXPECT_EQ(returned_group_data->former_members[0].display_name, display_name2);
  EXPECT_EQ(returned_group_data->former_members[0].email, email2);
  EXPECT_EQ(returned_group_data->former_members[0].role, role2);
  EXPECT_EQ(returned_group_data->former_members[0].avatar_url, avatar_url2);
  EXPECT_EQ(returned_group_data->former_members[0].given_name, given_name2);
}

INSTANTIATE_TEST_SUITE_P(DataSharingServiceImplTestInstantiation,
                         DataSharingServiceImplTest,
                         ::testing::Values(Variant::kDelegateAtCreation,
                                           Variant::kDelegateAfter));

}  // namespace data_sharing
