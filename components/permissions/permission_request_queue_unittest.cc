// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_queue.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace permissions {

enum UiFeatureConfig { NoChip, QuietChipOnly, ChipOnly, QuietChipAndChip };

class PermissionRequestQueueTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<UiFeatureConfig> {
 public:
  PermissionRequestQueueTest()
      : request1_(RequestType::kGeolocation,
                  PermissionRequestGestureType::GESTURE),
        request2_(RequestType::kMultipleDownloads,
                  PermissionRequestGestureType::NO_GESTURE) {
    switch (GetParam()) {
      case NoChip:
        feature_list_.InitWithFeatures(
            {}, {permissions::features::kPermissionChip,
                 permissions::features::kPermissionQuietChip});
        break;
      case QuietChipOnly:
        feature_list_.InitWithFeatures(
            {permissions::features::kPermissionQuietChip},
            {permissions::features::kPermissionChip});
        break;
      case ChipOnly:
        feature_list_.InitWithFeatures(
            {permissions::features::kPermissionChip},
            {permissions::features::kPermissionQuietChip});
        break;
      case QuietChipAndChip:
        feature_list_.InitWithFeatures(
            {permissions::features::kPermissionChip,
             permissions::features::kPermissionQuietChip},
            {});
        break;
    }
  }

 protected:
  PermissionRequestQueue permission_request_queue_;
  MockPermissionRequest request1_;
  MockPermissionRequest request2_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(PermissionRequestQueueTest, CountNumberOfRequestsInQueue) {
  EXPECT_EQ(0ul, permission_request_queue_.Count());

  permission_request_queue_.Push(&request1_);
  permission_request_queue_.Push(&request2_);
  EXPECT_EQ(2ul, permission_request_queue_.Count());

  permission_request_queue_.Pop();
  EXPECT_EQ(1ul, permission_request_queue_.Count());
}

TEST_P(PermissionRequestQueueTest, CountDuplicateRequests) {
  EXPECT_EQ(0ul, permission_request_queue_.Count());

  permission_request_queue_.Push(&request1_);
  permission_request_queue_.Push(&request1_);
  EXPECT_EQ(2ul, permission_request_queue_.Count());
}

TEST_P(PermissionRequestQueueTest, CountNumberOfRequestOccurencesInQueue) {
  EXPECT_EQ(0ul, permission_request_queue_.Count(&request1_));

  permission_request_queue_.Push(&request1_);
  permission_request_queue_.Push(&request1_);
  permission_request_queue_.Push(&request2_);

  EXPECT_EQ(2ul, permission_request_queue_.Count(&request1_));
}

TEST_P(PermissionRequestQueueTest, OnlyEmptyWithoutRequests) {
  EXPECT_TRUE(permission_request_queue_.IsEmpty());

  permission_request_queue_.Push(&request1_);
  EXPECT_FALSE(permission_request_queue_.IsEmpty());

  permission_request_queue_.Pop();
  EXPECT_TRUE(permission_request_queue_.IsEmpty());
}

TEST_P(PermissionRequestQueueTest, ShouldFindDuplicateRequest) {
  permission_request_queue_.Push(&request1_);
  permission_request_queue_.Push(&request2_);

  EXPECT_EQ(&request2_, permission_request_queue_.FindDuplicate(&request2_));
}

TEST_P(PermissionRequestQueueTest, ShouldNotFindDuplicateIfNotPresent) {
  permission_request_queue_.Push(&request1_);

  EXPECT_EQ(nullptr, permission_request_queue_.FindDuplicate(&request2_));
}

TEST_P(PermissionRequestQueueTest, PeekedElementIsNextPoppedElement) {
  permission_request_queue_.Push(&request1_);
  permission_request_queue_.Push(&request2_);
  PermissionRequest* peekedElement = permission_request_queue_.Peek();

  EXPECT_EQ(peekedElement, permission_request_queue_.Pop());
}

TEST_P(PermissionRequestQueueTest, VerifyPushOrder) {
  permission_request_queue_.Push(&request1_);
  permission_request_queue_.Push(&request2_);
  permission_request_queue_.Push(&request2_);

  if (GetParam() == NoChip) {
    EXPECT_EQ(permission_request_queue_.Pop(), &request1_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request2_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request2_);
  } else {  // QuietChipOnly, ChipOnly, QuietChipAndChip
    EXPECT_EQ(permission_request_queue_.Pop(), &request2_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request2_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request1_);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PermissionRequestQueueTest,
    ::testing::Values(NoChip, QuietChipOnly, ChipOnly, QuietChipAndChip));
}  // namespace permissions
