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
  std::unique_ptr<MockPermissionRequest> CreateRequest(
      std::pair<RequestType, PermissionRequestGestureType> request_params,
      base::WeakPtr<MockPermissionRequest::MockPermissionRequestState> state =
          nullptr) {
    return std::make_unique<permissions::MockPermissionRequest>(
        request_params.first, request_params.second, state);
  }

  std::unique_ptr<MockPermissionRequest> CreateRequest(
      std::pair<RequestType, bool> request_params,
      base::WeakPtr<MockPermissionRequest::MockPermissionRequestState> state =
          nullptr) {
    return std::make_unique<permissions::MockPermissionRequest>(
        GURL(MockPermissionRequest::kDefaultOrigin), request_params.first,
        request_params.second, state);
  }

  PermissionRequestQueue permission_request_queue_;
  std::pair<RequestType, PermissionRequestGestureType> request_low1_;
  std::pair<RequestType, PermissionRequestGestureType> request_low2_;
  std::pair<RequestType, PermissionRequestGestureType> request_normal1_;
  std::pair<RequestType, PermissionRequestGestureType> request_normal2_;
  std::pair<RequestType, bool> request_pepc1_;
  std::pair<RequestType, bool> request_pepc2_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PermissionRequestQueueTest, CountNumberOfRequestsInQueue) {
  EXPECT_EQ(0ul, permission_request_queue_.size());

  permission_request_queue_.Push(CreateRequest(request_normal1_));
  permission_request_queue_.Push(CreateRequest(request_normal2_));
  EXPECT_EQ(2ul, permission_request_queue_.size());

  permission_request_queue_.Pop();
  EXPECT_EQ(1ul, permission_request_queue_.size());
}

TEST_F(PermissionRequestQueueTest, CountDuplicateRequests) {
  EXPECT_EQ(0ul, permission_request_queue_.size());

  permission_request_queue_.Push(CreateRequest(request_normal1_));
  permission_request_queue_.Push(CreateRequest(request_normal1_));
  EXPECT_EQ(2ul, permission_request_queue_.size());
}

TEST_F(PermissionRequestQueueTest, OnlyEmptyWithoutRequests) {
  EXPECT_TRUE(permission_request_queue_.IsEmpty());

  permission_request_queue_.Push(CreateRequest(request_normal1_));
  EXPECT_FALSE(permission_request_queue_.IsEmpty());

  permission_request_queue_.Pop();
  EXPECT_TRUE(permission_request_queue_.IsEmpty());
}

TEST_F(PermissionRequestQueueTest, VerifyContains) {
  auto request1 = CreateRequest(request_normal1_);
  auto request2 = CreateRequest(request_normal2_);

  auto* request1_ptr = request1.get();
  auto* request2_ptr = request2.get();

  EXPECT_FALSE(permission_request_queue_.Contains(request1_ptr));
  EXPECT_FALSE(permission_request_queue_.Contains(request2_ptr));

  permission_request_queue_.Push(std::move(request1));

  EXPECT_TRUE(permission_request_queue_.Contains(request1_ptr));
  EXPECT_FALSE(permission_request_queue_.Contains(request2_ptr));

  permission_request_queue_.Push(std::move(request2));

  EXPECT_TRUE(permission_request_queue_.Contains(request1_ptr));
  EXPECT_TRUE(permission_request_queue_.Contains(request2_ptr));

  auto popped_request = permission_request_queue_.Pop();

  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_EQ(popped_request.get(), request2_ptr);
    EXPECT_TRUE(permission_request_queue_.Contains(request1_ptr));
    EXPECT_FALSE(permission_request_queue_.Contains(request2_ptr));
  } else {
    EXPECT_EQ(popped_request.get(), request1_ptr);
    EXPECT_FALSE(permission_request_queue_.Contains(request1_ptr));
    EXPECT_TRUE(permission_request_queue_.Contains(request2_ptr));
  }
}

TEST_F(PermissionRequestQueueTest, ShouldFindDuplicateRequest) {
  auto request_normal1 = CreateRequest(request_normal1_);
  auto request_normal2 = CreateRequest(request_normal2_);
  auto request_normal2_ptr = request_normal2.get();
  permission_request_queue_.Push(std::move(request_normal1));
  permission_request_queue_.Push(std::move(request_normal2));

  EXPECT_EQ(request_normal2_ptr, permission_request_queue_.FindDuplicate(
                                     CreateRequest(request_normal2_).get()));
}

TEST_F(PermissionRequestQueueTest, ShouldNotFindDuplicateIfNotPresent) {
  permission_request_queue_.Push(CreateRequest(request_normal1_));

  EXPECT_EQ(nullptr, permission_request_queue_.FindDuplicate(
                         CreateRequest(request_normal2_).get()));
}

TEST_F(PermissionRequestQueueTest, PeekedElementIsNextPoppedElement) {
  permission_request_queue_.Push(CreateRequest(request_normal1_));
  permission_request_queue_.Push(CreateRequest(request_normal2_));
  PermissionRequest* peekedElement = permission_request_queue_.Peek();

  EXPECT_EQ(peekedElement, permission_request_queue_.Pop().get());
}

TEST_F(PermissionRequestQueueTest, VerifyPushOrder) {
  permission_request_queue_.Push(CreateRequest(request_normal1_));
  permission_request_queue_.Push(CreateRequest(request_normal2_));
  permission_request_queue_.Push(CreateRequest(request_normal2_));

  if (!PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_normal1_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_normal2_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_normal2_.first);
  } else {
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_normal2_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_normal2_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_normal1_.first);
  }
}

TEST_F(PermissionRequestQueueTest, VerifyPushOrderLowPriority) {
  permission_request_queue_.Push(CreateRequest(request_low1_));
  permission_request_queue_.Push(CreateRequest(request_normal1_));
  permission_request_queue_.Push(CreateRequest(request_low2_));
  permission_request_queue_.Push(CreateRequest(request_normal2_));

  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_normal2_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_normal1_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_low2_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_low1_.first);
  } else {
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_low1_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_normal1_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_low2_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_normal2_.first);
  }
}

TEST_F(PermissionRequestQueueTest, VerifyPushFrontOrder) {
  permission_request_queue_.PushFront(CreateRequest(request_pepc1_));
  permission_request_queue_.PushFront(CreateRequest(request_low1_));
  permission_request_queue_.PushFront(CreateRequest(request_normal1_));
  permission_request_queue_.PushFront(CreateRequest(request_normal2_));
  permission_request_queue_.PushFront(CreateRequest(request_low2_));

  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_pepc1_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_normal2_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_normal1_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_low2_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_low1_.first);
  } else {
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_pepc1_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_low2_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_normal2_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_normal1_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_low1_.first);
  }
}

TEST_F(PermissionRequestQueueTest, VerifyPushBackOrder) {
  permission_request_queue_.PushBack(CreateRequest(request_low1_));
  permission_request_queue_.PushBack(CreateRequest(request_pepc1_));
  permission_request_queue_.PushBack(CreateRequest(request_normal1_));
  permission_request_queue_.PushBack(CreateRequest(request_normal2_));
  permission_request_queue_.PushBack(CreateRequest(request_low2_));
  permission_request_queue_.PushBack(CreateRequest(request_pepc2_));

  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_pepc1_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_pepc2_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_normal1_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_normal2_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_low1_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_low2_.first);
  } else {
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_pepc1_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_pepc2_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_low1_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_normal1_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_normal2_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_low2_.first);
  }
}

TEST_F(PermissionRequestQueueTest, PEPCPushesOtherRequests) {
  permission_request_queue_.Push(CreateRequest(request_low1_));
  permission_request_queue_.Push(CreateRequest(request_normal1_));
  permission_request_queue_.Push(CreateRequest(request_pepc1_));

  if (PermissionUtil::DoesPlatformSupportChip()) {
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_pepc1_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_normal1_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_low1_.first);
  } else {
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_pepc1_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_low1_.first);
    EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
              request_normal1_.first);
  }
}

TEST_F(PermissionRequestQueueTest, PEPCNotPushedByOtherRequests) {
  permission_request_queue_.Push(CreateRequest(request_pepc1_));
  permission_request_queue_.Push(CreateRequest(request_normal1_));

  EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
            request_pepc1_.first);
  EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
            request_normal1_.first);
}

TEST_F(PermissionRequestQueueTest, PEPCDoesNotPushOtherPEPCRequests) {
  permission_request_queue_.Push(CreateRequest(request_pepc1_));
  permission_request_queue_.Push(CreateRequest(request_normal1_));
  permission_request_queue_.Push(CreateRequest(request_pepc2_));

  EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
            request_pepc1_.first);
  EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
            request_pepc2_.first);
  EXPECT_EQ(permission_request_queue_.Pop()->request_type(),
            request_normal1_.first);
}

}  // namespace permissions
