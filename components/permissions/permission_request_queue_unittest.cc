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
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace permissions {

class PermissionRequestQueueTest : public ::testing::Test {
 public:
  PermissionRequestQueueTest()
      : request_low1_(RequestType::kGeolocation,
                      PermissionRequestGestureType::GESTURE),
        request_low2_(RequestType::kNotifications,
                      PermissionRequestGestureType::NO_GESTURE),
        request_normal1_(RequestType::kMultipleDownloads,
                         PermissionRequestGestureType::GESTURE),
        request_normal2_(RequestType::kClipboard,
                         PermissionRequestGestureType::NO_GESTURE),
        request_pepc1_(RequestType::kCameraStream,
                       /*embedded_permission_element_initiated=*/true),
        request_pepc2_(RequestType::kGeolocation,
                       /*embedded_permission_element_initiated=*/true) {}

 protected:
  PermissionRequestQueue permission_request_queue_;
  MockPermissionRequest request_low1_;
  MockPermissionRequest request_low2_;
  MockPermissionRequest request_normal1_;
  MockPermissionRequest request_normal2_;
  MockPermissionRequest request_pepc1_;
  MockPermissionRequest request_pepc2_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PermissionRequestQueueTest, CountNumberOfRequestsInQueue) {
  EXPECT_EQ(0ul, permission_request_queue_.size());

  permission_request_queue_.Push(&request_normal1_);
  permission_request_queue_.Push(&request_normal2_);
  EXPECT_EQ(2ul, permission_request_queue_.size());

  permission_request_queue_.Pop();
  EXPECT_EQ(1ul, permission_request_queue_.size());
}

TEST_F(PermissionRequestQueueTest, CountDuplicateRequests) {
  EXPECT_EQ(0ul, permission_request_queue_.size());

  permission_request_queue_.Push(&request_normal1_);
  permission_request_queue_.Push(&request_normal1_);
  EXPECT_EQ(2ul, permission_request_queue_.size());
}

TEST_F(PermissionRequestQueueTest, CountNumberOfRequestOccurencesInQueue) {
  EXPECT_EQ(0ul, permission_request_queue_.Count(&request_normal1_));

  permission_request_queue_.Push(&request_normal1_);
  permission_request_queue_.Push(&request_normal1_);
  permission_request_queue_.Push(&request_normal2_);

  EXPECT_EQ(2ul, permission_request_queue_.Count(&request_normal1_));
}

TEST_F(PermissionRequestQueueTest, OnlyEmptyWithoutRequests) {
  EXPECT_TRUE(permission_request_queue_.IsEmpty());

  permission_request_queue_.Push(&request_normal1_);
  EXPECT_FALSE(permission_request_queue_.IsEmpty());

  permission_request_queue_.Pop();
  EXPECT_TRUE(permission_request_queue_.IsEmpty());
}

TEST_F(PermissionRequestQueueTest, ShouldFindDuplicateRequest) {
  permission_request_queue_.Push(&request_normal1_);
  permission_request_queue_.Push(&request_normal2_);

  EXPECT_EQ(&request_normal2_,
            permission_request_queue_.FindDuplicate(&request_normal2_));
}

TEST_F(PermissionRequestQueueTest, ShouldNotFindDuplicateIfNotPresent) {
  permission_request_queue_.Push(&request_normal1_);

  EXPECT_EQ(nullptr,
            permission_request_queue_.FindDuplicate(&request_normal2_));
}

TEST_F(PermissionRequestQueueTest, PeekedElementIsNextPoppedElement) {
  permission_request_queue_.Push(&request_normal1_);
  permission_request_queue_.Push(&request_normal2_);
  PermissionRequest* peekedElement = permission_request_queue_.Peek();

  EXPECT_EQ(peekedElement, permission_request_queue_.Pop());
}

TEST_F(PermissionRequestQueueTest, VerifyPushOrder) {
  permission_request_queue_.Push(&request_normal1_);
  permission_request_queue_.Push(&request_normal2_);
  permission_request_queue_.Push(&request_normal2_);

  if (!PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_EQ(permission_request_queue_.Pop(), &request_normal1_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_normal2_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_normal2_);
  } else {
    EXPECT_EQ(permission_request_queue_.Pop(), &request_normal2_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_normal2_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_normal1_);
  }
}

TEST_F(PermissionRequestQueueTest, VerifyPushOrderLowPriority) {
  permission_request_queue_.Push(&request_low1_);
  permission_request_queue_.Push(&request_normal1_);
  permission_request_queue_.Push(&request_low2_);
  permission_request_queue_.Push(&request_normal2_);

  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_EQ(permission_request_queue_.Pop(), &request_normal2_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_normal1_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_low2_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_low1_);
  } else {
    EXPECT_EQ(permission_request_queue_.Pop(), &request_low1_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_normal1_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_low2_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_normal2_);
  }
}

TEST_F(PermissionRequestQueueTest, VerifyPushFrontOrder) {
  permission_request_queue_.PushFront(&request_pepc1_);
  permission_request_queue_.PushFront(&request_low1_);
  permission_request_queue_.PushFront(&request_normal1_);
  permission_request_queue_.PushFront(&request_normal2_);
  permission_request_queue_.PushFront(&request_low2_);

  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_EQ(permission_request_queue_.Pop(), &request_pepc1_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_normal2_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_normal1_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_low2_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_low1_);
  } else {
    EXPECT_EQ(permission_request_queue_.Pop(), &request_pepc1_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_low2_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_normal2_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_normal1_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_low1_);
  }
}

TEST_F(PermissionRequestQueueTest, VerifyPushBackOrder) {
  permission_request_queue_.PushBack(&request_low1_);
  permission_request_queue_.PushBack(&request_pepc1_);
  permission_request_queue_.PushBack(&request_normal1_);
  permission_request_queue_.PushBack(&request_normal2_);
  permission_request_queue_.PushBack(&request_low2_);
  permission_request_queue_.PushBack(&request_pepc2_);

  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_EQ(permission_request_queue_.Pop(), &request_pepc1_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_pepc2_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_normal1_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_normal2_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_low1_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_low2_);
  } else {
    EXPECT_EQ(permission_request_queue_.Pop(), &request_pepc1_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_pepc2_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_low1_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_normal1_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_normal2_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_low2_);
  }
}

TEST_F(PermissionRequestQueueTest, PEPCPushesOtherRequests) {
  permission_request_queue_.Push(&request_low1_);
  permission_request_queue_.Push(&request_normal1_);
  permission_request_queue_.Push(&request_pepc1_);

  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_EQ(permission_request_queue_.Pop(), &request_pepc1_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_normal1_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_low1_);
  } else {
    EXPECT_EQ(permission_request_queue_.Pop(), &request_pepc1_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_low1_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request_normal1_);
  }
}

TEST_F(PermissionRequestQueueTest, PEPCNotPushedByOtherRequests) {
  permission_request_queue_.Push(&request_pepc1_);
  permission_request_queue_.Push(&request_normal1_);

  EXPECT_EQ(permission_request_queue_.Pop(), &request_pepc1_);
  EXPECT_EQ(permission_request_queue_.Pop(), &request_normal1_);
}

TEST_F(PermissionRequestQueueTest, PEPCDoesNotPushOtherPEPCRequests) {
  permission_request_queue_.Push(&request_pepc1_);
  permission_request_queue_.Push(&request_normal1_);
  permission_request_queue_.Push(&request_pepc2_);

  EXPECT_EQ(permission_request_queue_.Pop(), &request_pepc1_);
  EXPECT_EQ(permission_request_queue_.Pop(), &request_pepc2_);
  EXPECT_EQ(permission_request_queue_.Pop(), &request_normal1_);
}

}  // namespace permissions
