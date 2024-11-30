// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/partial_failure_sdk_delegate_wrapper.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "components/data_sharing/test_support/fake_data_sharing_sdk_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_sharing {
namespace {

data_sharing_pb::ReadGroupsParams CreateReadGroupsParams(
    const std::vector<GroupId>& group_ids) {
  data_sharing_pb::ReadGroupsParams read_groups_params;
  for (const GroupId& group_id : group_ids) {
    read_groups_params.add_group_ids(group_id.value());
    data_sharing_pb::ReadGroupsParams::GroupParams* group_params =
        read_groups_params.add_group_params();
    group_params->set_group_id(group_id.value());
  }
  return read_groups_params;
}

class PartialFailureSDKDelegateWrapperTest : public testing::Test {
 public:
  PartialFailureSDKDelegateWrapperTest()
      : sdk_delegate_wrapper_(&actual_sdk_delegate_) {}
  ~PartialFailureSDKDelegateWrapperTest() override = default;

  FakeDataSharingSDKDelegate& actual_sdk_delegate() {
    return actual_sdk_delegate_;
  }

  PartialFailureSDKDelegateWrapper& sdk_delegate_wrapper() {
    return sdk_delegate_wrapper_;
  }

  base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>
  ReadGroupsViaWrapper(const data_sharing_pb::ReadGroupsParams& params) {
    base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>
        read_groups_result;
    base::RunLoop run_loop;
    sdk_delegate_wrapper().ReadGroups(
        params,
        base::BindLambdaForTesting(
            [&](const base::expected<data_sharing_pb::ReadGroupsResult,
                                     absl::Status>& read_groups_result_arg) {
              read_groups_result = std::move(read_groups_result_arg);
              run_loop.Quit();
            }));
    run_loop.Run();
    return read_groups_result;
  }

 private:
  base::test::TaskEnvironment task_environment_;

  FakeDataSharingSDKDelegate actual_sdk_delegate_;
  PartialFailureSDKDelegateWrapper sdk_delegate_wrapper_;
};

TEST_F(PartialFailureSDKDelegateWrapperTest, ShouldReadSingleGroup) {
  const std::string display_name = "group_display_name";
  const GroupId group_id =
      actual_sdk_delegate().AddGroupAndReturnId(display_name);

  base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>
      read_groups_result =
          ReadGroupsViaWrapper(CreateReadGroupsParams({group_id}));

  ASSERT_TRUE(read_groups_result.has_value());
  ASSERT_EQ(read_groups_result->group_data_size(), 1);
  EXPECT_EQ(read_groups_result->group_data(0).display_name(), display_name);
}

TEST_F(PartialFailureSDKDelegateWrapperTest, ShouldReadMultipleGroups) {
  const std::string display_name1 = "group_display_name_1";
  const GroupId group_id1 =
      actual_sdk_delegate().AddGroupAndReturnId(display_name1);
  const std::string display_name2 = "group_display_name_2";
  const GroupId group_id2 =
      actual_sdk_delegate().AddGroupAndReturnId(display_name2);

  base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>
      read_groups_result =
          ReadGroupsViaWrapper(CreateReadGroupsParams({group_id1, group_id2}));

  ASSERT_TRUE(read_groups_result.has_value());
  ASSERT_EQ(read_groups_result->group_data_size(), 2);
  EXPECT_EQ(read_groups_result->group_data(0).display_name(), display_name1);
  EXPECT_EQ(read_groups_result->group_data(1).display_name(), display_name2);
}

TEST_F(PartialFailureSDKDelegateWrapperTest, ShouldHandlePartialFailure) {
  const std::string display_name1 = "group_display_name_1";
  const GroupId group_id1 =
      actual_sdk_delegate().AddGroupAndReturnId(display_name1);
  const GroupId group_id2 = GroupId("non_existent_group_id");

  base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>
      read_groups_result =
          ReadGroupsViaWrapper(CreateReadGroupsParams({group_id1, group_id2}));

  ASSERT_TRUE(read_groups_result.has_value());
  ASSERT_EQ(read_groups_result->group_data_size(), 1);
  EXPECT_EQ(read_groups_result->group_data(0).display_name(), display_name1);
  ASSERT_EQ(read_groups_result->failed_read_group_results_size(), 1);
  EXPECT_EQ(read_groups_result->failed_read_group_results(0).group_id(),
            group_id2.value());
  EXPECT_EQ(read_groups_result->failed_read_group_results(0).failure_reason(),
            data_sharing_pb::FailedReadGroupResult::GROUP_NOT_FOUND);
}

// TODO(crbug.com/377914193): add tests for full and partial transient failures
// once FakeDataSharingSDKDelegate supports faking them.

}  // namespace
}  // namespace data_sharing
