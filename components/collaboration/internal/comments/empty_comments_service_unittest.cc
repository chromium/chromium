// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/comments/empty_comments_service.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/uuid.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace collaboration::comments {

class MockCommentsObserver : public CommentsObserver {
 public:
  MOCK_METHOD(void, OnServiceInitialized, (), (override));
  MOCK_METHOD(void, OnCommentsAdded, (const std::vector<Comment>&), (override));
  MOCK_METHOD(void,
              OnCommentsModified,
              (const std::vector<Comment>&, const std::vector<Comment>&),
              (override));
  MOCK_METHOD(void,
              OnCommentsDeleted,
              (const std::vector<CommentId>&),
              (override));
  MOCK_METHOD(void, OnCommentsInvalidated, (), (override));
};

class EmptyCommentsServiceTest : public testing::Test {
 public:
  void SetUp() override { service_ = std::make_unique<EmptyCommentsService>(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<EmptyCommentsService> service_;
};

TEST_F(EmptyCommentsServiceTest, TestServiceConstruction) {
  EXPECT_TRUE(service_->IsEmptyService());
  EXPECT_TRUE(service_->IsInitialized());
}

}  // namespace collaboration::comments
