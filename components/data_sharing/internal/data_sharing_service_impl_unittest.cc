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

  std::string AddGroupAndReturnId(const std::string& display_name) {
    data_sharing_pb::GroupData group_data;
    group_data.set_group_id(base::NumberToString(next_group_id_++));
    group_data.set_display_name(display_name);
    groups_[group_data.group_id()] = group_data;
    return group_data.group_id();
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
    NOTIMPLEMENTED();
  }

  void RemoveMember(
      const data_sharing_pb::RemoveMemberParams& params,
      base::OnceCallback<void(const absl::Status&)> callback) override {
    NOTIMPLEMENTED();
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
    NOTIMPLEMENTED();
  }

 private:
  std::map<std::string, data_sharing_pb::GroupData> groups_;
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

}  // namespace data_sharing
