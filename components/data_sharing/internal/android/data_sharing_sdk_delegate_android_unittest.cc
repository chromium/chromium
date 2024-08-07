// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/android/data_sharing_sdk_delegate_android.h"

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/data_sharing/internal/test_jni_headers/DataSharingSDKDelegateAndroidTestSupport_jni.h"
#include "components/data_sharing/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_sharing {
namespace {

const char kTestGroupName[] = "test_group_name";
const char kTestGroupId[] = "test_group_id";

class DataSharingSDKDelegateAndroidTest : public testing::Test {
 protected:
  DataSharingSDKDelegateAndroidTest() = default;

  ~DataSharingSDKDelegateAndroidTest() override = default;

  void SetUp() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    delegate_ = new DataSharingSDKDelegateAndroid(
        Java_DataSharingSDKDelegateAndroidTestSupport_createDelegateTestImpl(
            env));
  }

  data_sharing_pb::CreateGroupResult TestCreateGroup() {
    data_sharing_pb::CreateGroupResult outcome;
    base::RunLoop run_loop;
    data_sharing_pb::CreateGroupParams params;
    params.set_display_name(kTestGroupName);
    delegate_->CreateGroup(
        params, base::BindLambdaForTesting(
                    [&run_loop, &outcome](
                        const base::expected<data_sharing_pb::CreateGroupResult,
                                             absl::Status>& result) {
                      if (result.has_value()) {
                        outcome = result.value();
                      }
                      run_loop.Quit();
                    }));
    run_loop.Run();
    return outcome;
  }

  data_sharing_pb::ReadGroupsResult TestReadGroups(int groups_count) {
    data_sharing_pb::ReadGroupsResult outcome;
    base::RunLoop run_loop;
    data_sharing_pb::ReadGroupsParams params;
    for (int id = 1; id <= groups_count; id++) {
      std::string group_id = "test_group_id_" + base::NumberToString(id);
      params.add_group_ids(group_id);
    }
    delegate_->ReadGroups(
        params, base::BindLambdaForTesting(
                    [&run_loop, &outcome](
                        const base::expected<data_sharing_pb::ReadGroupsResult,
                                             absl::Status>& result) {
                      if (result.has_value()) {
                        outcome = result.value();
                      }
                      run_loop.Quit();
                    }));
    run_loop.Run();
    return outcome;
  }

  absl::Status TestAddMember() {
    absl::Status outcome;
    base::RunLoop run_loop;
    data_sharing_pb::AddMemberParams params;
    params.set_group_id(kTestGroupId);
    delegate_->AddMember(params,
                         base::BindLambdaForTesting(
                             [&run_loop, &outcome](const absl::Status& result) {
                               outcome = result;
                               run_loop.Quit();
                             }));
    run_loop.Run();
    return outcome;
  }

  absl::Status TestRemoveMember() {
    absl::Status outcome;
    base::RunLoop run_loop;
    data_sharing_pb::RemoveMemberParams params;
    params.set_group_id(kTestGroupId);
    delegate_->RemoveMember(
        params, base::BindLambdaForTesting(
                    [&run_loop, &outcome](const absl::Status& result) {
                      outcome = result;
                      run_loop.Quit();
                    }));
    run_loop.Run();
    return outcome;
  }

  absl::Status TestDeleteGroup() {
    absl::Status outcome;
    base::RunLoop run_loop;
    data_sharing_pb::DeleteGroupParams params;
    params.set_group_id(kTestGroupId);
    delegate_->DeleteGroup(
        params, base::BindLambdaForTesting(
                    [&run_loop, &outcome](const absl::Status& result) {
                      outcome = result;
                      run_loop.Quit();
                    }));
    run_loop.Run();
    return outcome;
  }

  data_sharing_pb::LookupGaiaIdByEmailResult TestLookupGaiaIdByEmail() {
    data_sharing_pb::LookupGaiaIdByEmailResult outcome;
    base::RunLoop run_loop;
    data_sharing_pb::LookupGaiaIdByEmailParams params;
    params.set_email(kTestGroupName);
    delegate_->LookupGaiaIdByEmail(
        params,
        base::BindLambdaForTesting(
            [&run_loop, &outcome](
                const base::expected<data_sharing_pb::LookupGaiaIdByEmailResult,
                                     absl::Status>& result) {
              if (result.has_value()) {
                outcome = result.value();
              }
              run_loop.Quit();
            }));
    run_loop.Run();
    return outcome;
  }

  data_sharing_pb::AddAccessTokenResult TestAddAccessToken() {
    data_sharing_pb::AddAccessTokenResult outcome;
    base::RunLoop run_loop;
    data_sharing_pb::AddAccessTokenParams params;
    params.set_group_id(kTestGroupId);
    delegate_->AddAccessToken(
        params,
        base::BindLambdaForTesting(
            [&run_loop, &outcome](
                const base::expected<data_sharing_pb::AddAccessTokenResult,
                                     absl::Status>& result) {
              if (result.has_value()) {
                outcome = result.value();
              }
              run_loop.Quit();
            }));
    run_loop.Run();
    return outcome;
  }

  base::test::TaskEnvironment task_environment_;
  raw_ptr<DataSharingSDKDelegateAndroid> delegate_;
};

TEST_F(DataSharingSDKDelegateAndroidTest, TestCreateGroup) {
  data_sharing_pb::CreateGroupResult outcome = TestCreateGroup();
  EXPECT_EQ(outcome.group_data().display_name(), kTestGroupName);
  EXPECT_EQ(outcome.group_data().group_id(), kTestGroupId);
}

TEST_F(DataSharingSDKDelegateAndroidTest, TestReadGroups) {
  int groups_count = 3;
  data_sharing_pb::ReadGroupsResult outcome = TestReadGroups(groups_count);
  EXPECT_EQ(outcome.group_data_size(), groups_count);
  for (int id = 1; id <= groups_count; id++) {
    std::string group_id = "test_group_id_" + base::NumberToString(id);
    std::string group_name = "test_group_name_" + base::NumberToString(id);
    EXPECT_EQ(outcome.group_data(id - 1).group_id(), group_id);
    EXPECT_EQ(outcome.group_data(id - 1).display_name(), group_name);
  }
}

TEST_F(DataSharingSDKDelegateAndroidTest, TestAddMember) {
  absl::Status outcome = TestAddMember();
  EXPECT_EQ(outcome, absl::OkStatus());
}

TEST_F(DataSharingSDKDelegateAndroidTest, TestRemoveMember) {
  absl::Status outcome = TestRemoveMember();
  EXPECT_EQ(outcome, absl::CancelledError());
}

TEST_F(DataSharingSDKDelegateAndroidTest, TestDeleteGroup) {
  absl::Status outcome = TestDeleteGroup();
  EXPECT_EQ(outcome, absl::OkStatus());
}

TEST_F(DataSharingSDKDelegateAndroidTest, TestLookupGaiaIdByEmail) {
  data_sharing_pb::LookupGaiaIdByEmailResult outcome =
      TestLookupGaiaIdByEmail();
  EXPECT_EQ(outcome.gaia_id(), kTestGroupName);
}

TEST_F(DataSharingSDKDelegateAndroidTest, TestAddAccessToken) {
  data_sharing_pb::AddAccessTokenResult outcome = TestAddAccessToken();
  EXPECT_EQ(outcome.group_data().group_id(), kTestGroupId);
}

}  // namespace
}  // namespace data_sharing
