// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_content_mutation.h"

#include "components/feed/core/feed_content_operation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {

namespace {

const char kContentkey[] = "ContentKey";
const char kContentValue[] = "Value";
const char kContentPrefix[] = "Content";

}  // namespace

class FeedContentMutationTest : public testing::Test {
 public:
  FeedContentMutationTest() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(FeedContentMutationTest);
};

TEST_F(FeedContentMutationTest, AppendDeleteOperation) {
  ContentMutation mutation;
  EXPECT_TRUE(mutation.Empty());

  mutation.AppendDeleteOperation(kContentkey);
  EXPECT_FALSE(mutation.Empty());

  ContentOperation operation = mutation.TakeFirstOperation();
  EXPECT_TRUE(mutation.Empty());
  EXPECT_EQ(operation.type(), ContentOperation::CONTENT_DELETE);
  EXPECT_EQ(operation.key(), kContentkey);
}

TEST_F(FeedContentMutationTest, AppendDeleteAllOperation) {
  ContentMutation mutation;
  EXPECT_TRUE(mutation.Empty());

  mutation.AppendDeleteAllOperation();
  EXPECT_FALSE(mutation.Empty());

  ContentOperation operation = mutation.TakeFirstOperation();
  EXPECT_TRUE(mutation.Empty());
  EXPECT_EQ(operation.type(), ContentOperation::CONTENT_DELETE_ALL);
}

TEST_F(FeedContentMutationTest, AppendDeleteByPrefixOperation) {
  ContentMutation mutation;
  EXPECT_TRUE(mutation.Empty());

  mutation.AppendDeleteByPrefixOperation(kContentPrefix);
  EXPECT_FALSE(mutation.Empty());

  ContentOperation operation = mutation.TakeFirstOperation();
  EXPECT_TRUE(mutation.Empty());
  EXPECT_EQ(operation.type(), ContentOperation::CONTENT_DELETE_BY_PREFIX);
  EXPECT_EQ(operation.prefix(), kContentPrefix);
}

TEST_F(FeedContentMutationTest, AppendUpsertOperation) {
  ContentMutation mutation;
  EXPECT_TRUE(mutation.Empty());

  mutation.AppendUpsertOperation(kContentkey, kContentValue);
  EXPECT_FALSE(mutation.Empty());

  ContentOperation operation = mutation.TakeFirstOperation();
  EXPECT_TRUE(mutation.Empty());
  EXPECT_EQ(operation.type(), ContentOperation::CONTENT_UPSERT);
  EXPECT_EQ(operation.key(), kContentkey);
  EXPECT_EQ(operation.value(), kContentValue);
}

}  // namespace feed
