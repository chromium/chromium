// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/comments/comments_service_impl.h"

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

class CommentsServiceImplTest : public testing::Test {
 public:
  void SetUp() override { service_ = std::make_unique<CommentsServiceImpl>(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<CommentsServiceImpl> service_;
};

TEST_F(CommentsServiceImplTest, TestServiceConstruction) {
  EXPECT_FALSE(service_->IsEmptyService());
  EXPECT_FALSE(service_->IsInitialized());
}

TEST_F(CommentsServiceImplTest, AddComment) {
  base::test::TestFuture<bool> future;
  CommentId id = service_->AddComment(data_sharing::GroupId("collaboration_id"),
                                      GURL("http://a.com"), "content",
                                      std::nullopt, future.GetCallback());
  EXPECT_TRUE(id.is_valid());
  // Note: We don't wait on the future because the stub implementation does not
  // invoke the callback. This will be updated when the real logic is added.
}

TEST_F(CommentsServiceImplTest, EditComment) {
  base::test::TestFuture<bool> future;
  service_->EditComment(base::Uuid::GenerateRandomV4(), "new content",
                        future.GetCallback());
}

TEST_F(CommentsServiceImplTest, DeleteComment) {
  base::test::TestFuture<bool> future;
  service_->DeleteComment(base::Uuid::GenerateRandomV4(), future.GetCallback());
}

TEST_F(CommentsServiceImplTest, QueryComments) {
  base::test::TestFuture<QueryResult> future;
  service_->QueryComments({}, {}, future.GetCallback());
}

TEST_F(CommentsServiceImplTest, Observer) {
  MockCommentsObserver observer;
  service_->AddObserver(&observer, {});
  service_->RemoveObserver(&observer);
}

}  // namespace collaboration::comments
