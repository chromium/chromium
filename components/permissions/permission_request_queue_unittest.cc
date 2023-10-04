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
      : request1_(RequestType::kGeolocation,
                  PermissionRequestGestureType::GESTURE),
        request2_(RequestType::kMultipleDownloads,
                  PermissionRequestGestureType::NO_GESTURE) {
  }

 protected:
  PermissionRequestQueue permission_request_queue_;
  MockPermissionRequest request1_;
  MockPermissionRequest request2_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PermissionRequestQueueTest, CountNumberOfRequestsInQueue) {
  EXPECT_EQ(0ul, permission_request_queue_.Count());

  permission_request_queue_.Push(&request1_);
  permission_request_queue_.Push(&request2_);
  EXPECT_EQ(2ul, permission_request_queue_.Count());

  permission_request_queue_.Pop();
  EXPECT_EQ(1ul, permission_request_queue_.Count());
}

TEST_F(PermissionRequestQueueTest, CountDuplicateRequests) {
  EXPECT_EQ(0ul, permission_request_queue_.Count());

  permission_request_queue_.Push(&request1_);
  permission_request_queue_.Push(&request1_);
  EXPECT_EQ(2ul, permission_request_queue_.Count());
}

TEST_F(PermissionRequestQueueTest, CountNumberOfRequestOccurencesInQueue) {
  EXPECT_EQ(0ul, permission_request_queue_.Count(&request1_));

  permission_request_queue_.Push(&request1_);
  permission_request_queue_.Push(&request1_);
  permission_request_queue_.Push(&request2_);

  EXPECT_EQ(2ul, permission_request_queue_.Count(&request1_));
}

TEST_F(PermissionRequestQueueTest, OnlyEmptyWithoutRequests) {
  EXPECT_TRUE(permission_request_queue_.IsEmpty());

  permission_request_queue_.Push(&request1_);
  EXPECT_FALSE(permission_request_queue_.IsEmpty());

  permission_request_queue_.Pop();
  EXPECT_TRUE(permission_request_queue_.IsEmpty());
}

TEST_F(PermissionRequestQueueTest, ShouldFindDuplicateRequest) {
  permission_request_queue_.Push(&request1_);
  permission_request_queue_.Push(&request2_);

  EXPECT_EQ(&request2_, permission_request_queue_.FindDuplicate(&request2_));
}

TEST_F(PermissionRequestQueueTest, ShouldNotFindDuplicateIfNotPresent) {
  permission_request_queue_.Push(&request1_);

  EXPECT_EQ(nullptr, permission_request_queue_.FindDuplicate(&request2_));
}

TEST_F(PermissionRequestQueueTest, PeekedElementIsNextPoppedElement) {
  permission_request_queue_.Push(&request1_);
  permission_request_queue_.Push(&request2_);
  PermissionRequest* peekedElement = permission_request_queue_.Peek();

  EXPECT_EQ(peekedElement, permission_request_queue_.Pop());
}

TEST_F(PermissionRequestQueueTest, VerifyPushOrder) {
  permission_request_queue_.Push(&request1_);
  permission_request_queue_.Push(&request2_);
  permission_request_queue_.Push(&request2_);

  if (!PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_EQ(permission_request_queue_.Pop(), &request1_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request2_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request2_);
  } else {
    EXPECT_EQ(permission_request_queue_.Pop(), &request2_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request2_);
    EXPECT_EQ(permission_request_queue_.Pop(), &request1_);
  }
}

}  // namespace permissions
