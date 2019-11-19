// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_journal_mutation.h"

#include "components/feed/core/feed_journal_operation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {

namespace {

const char kJournalName1[] = "Journal1";
const char kJournalName2[] = "Journal2";
const uint8_t kJournalValue[] = {
    8,   1,   18,  40,  70,  69,  65,   84,  85,  82,  69, 58, 58,
    115, 116, 111, 114, 105, 101, 115,  46,  102, 58,  58, 45, 56,
    57,  48,  56,  51,  51,  54,  49,   56,  52,  53,  48, 48, 54,
    55,  56,  48,  51,  57,  24,  -119, -10, -71, -35, 5};

}  // namespace

class FeedJournalMutationTest : public testing::Test {
 public:
  FeedJournalMutationTest() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(FeedJournalMutationTest);
};

TEST_F(FeedJournalMutationTest, AddAppendOperation) {
  JournalMutation mutation(kJournalName1);
  EXPECT_EQ(mutation.journal_name(), kJournalName1);
  EXPECT_TRUE(mutation.Empty());

  std::vector<uint8_t> bytes_vector(
      kJournalValue,
      kJournalValue + sizeof(kJournalValue) / sizeof(kJournalValue[0]));

  mutation.AddAppendOperation(
      std::string(bytes_vector.begin(), bytes_vector.end()));
  EXPECT_FALSE(mutation.Empty());

  JournalOperation operation = mutation.TakeFirstOperation();
  EXPECT_TRUE(mutation.Empty());
  EXPECT_EQ(operation.type(), JournalOperation::JOURNAL_APPEND);
  EXPECT_EQ(operation.value(),
            std::string(bytes_vector.begin(), bytes_vector.end()));
}

TEST_F(FeedJournalMutationTest, AddCopyOperation) {
  JournalMutation mutation(kJournalName1);
  EXPECT_EQ(mutation.journal_name(), kJournalName1);
  EXPECT_TRUE(mutation.Empty());

  mutation.AddCopyOperation(kJournalName2);
  EXPECT_FALSE(mutation.Empty());

  JournalOperation operation = mutation.TakeFirstOperation();
  EXPECT_TRUE(mutation.Empty());
  EXPECT_EQ(operation.type(), JournalOperation::JOURNAL_COPY);
  EXPECT_EQ(operation.to_journal_name(), kJournalName2);
}

TEST_F(FeedJournalMutationTest, AddDeleteOperation) {
  JournalMutation mutation(kJournalName1);
  EXPECT_EQ(mutation.journal_name(), kJournalName1);
  EXPECT_TRUE(mutation.Empty());

  mutation.AddDeleteOperation();
  EXPECT_FALSE(mutation.Empty());

  JournalOperation operation = mutation.TakeFirstOperation();
  EXPECT_TRUE(mutation.Empty());
  EXPECT_EQ(operation.type(), JournalOperation::JOURNAL_DELETE);
}

}  // namespace feed
