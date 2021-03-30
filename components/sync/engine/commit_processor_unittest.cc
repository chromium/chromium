// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/commit_processor.h"

#include <memory>

#include "components/sync/engine/commit_contribution.h"
#include "components/sync/engine/commit_contributor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

constexpr int kMaxEntries = 17;

using testing::_;
using testing::IsEmpty;
using testing::Pair;
using testing::UnorderedElementsAre;

MATCHER_P(HasNumEntries, num_entries, "") {
  return static_cast<int>(arg->GetNumEntries()) == num_entries;
}

// Simple implementation of CommitContribution that only implements
// GetNumEntries().
class FakeCommitContribution : public CommitContribution {
 public:
  explicit FakeCommitContribution(size_t num_entries)
      : num_entries_(num_entries) {}

  ~FakeCommitContribution() override = default;

  // CommitContribution implementation.
  void AddToCommitMessage(sync_pb::ClientToServerMessage* msg) override {}
  SyncerError ProcessCommitResponse(
      const sync_pb::ClientToServerResponse& response,
      StatusController* status) override {
    return SyncerError();
  }
  void ProcessCommitFailure(SyncCommitError commit_error) override {}
  size_t GetNumEntries() const override { return num_entries_; }

 private:
  const size_t num_entries_;
};

ACTION_P(ReturnContributionWithEntries, num_entries) {
  return std::make_unique<FakeCommitContribution>(num_entries);
}

class MockCommitContributor : public CommitContributor {
 public:
  MockCommitContributor() = default;
  ~MockCommitContributor() override = default;
  MOCK_METHOD(std::unique_ptr<CommitContribution>,
              GetContribution,
              (size_t max_entries),
              (override));
};

class CommitProcessorTest : public testing::Test {
 protected:
  CommitProcessorTest()
      : contributor_map_{{NIGORI, &nigori_contributor_},
                         {SHARING_MESSAGE, &sharing_message_contributor_},
                         {BOOKMARKS, &bookmark_contributor_},
                         {PREFERENCES, &preference_contributor_}},
        processor_(/*commit_types=*/ModelTypeSet{NIGORI, SHARING_MESSAGE,
                                                 BOOKMARKS, PREFERENCES},
                   &contributor_map_) {
    EXPECT_TRUE(PriorityUserTypes().Has(SHARING_MESSAGE));
    EXPECT_FALSE(PriorityUserTypes().Has(BOOKMARKS));
    EXPECT_FALSE(PriorityUserTypes().Has(PREFERENCES));
  }

  testing::NiceMock<MockCommitContributor> nigori_contributor_;

  // A priority user type.
  testing::NiceMock<MockCommitContributor> sharing_message_contributor_;

  // Regular user types.
  testing::NiceMock<MockCommitContributor> bookmark_contributor_;
  testing::NiceMock<MockCommitContributor> preference_contributor_;

  CommitContributorMap contributor_map_;
  CommitProcessor processor_;
};

TEST_F(CommitProcessorTest, ShouldGatherNigoriOnlyContribution) {
  EXPECT_CALL(nigori_contributor_, GetContribution(kMaxEntries))
      .WillOnce(ReturnContributionWithEntries(/*num_entries=*/1));

  // No user types should be gathered and combined with NIGORI.
  EXPECT_CALL(sharing_message_contributor_, GetContribution).Times(0);
  EXPECT_CALL(bookmark_contributor_, GetContribution).Times(0);
  EXPECT_CALL(preference_contributor_, GetContribution).Times(0);

  EXPECT_THAT(processor_.GatherCommitContributions(/*max_entries=*/kMaxEntries),
              UnorderedElementsAre(Pair(NIGORI, HasNumEntries(1))));
}

TEST_F(CommitProcessorTest, ShouldGatherPriorityUserTypesOnlyContribution) {
  const int kNumReturnedEntries = 3;

  EXPECT_CALL(sharing_message_contributor_, GetContribution(kMaxEntries))
      .WillOnce(ReturnContributionWithEntries(kNumReturnedEntries));

  // Non-priority user types shouldn't even be gathered.
  EXPECT_CALL(bookmark_contributor_, GetContribution).Times(0);
  EXPECT_CALL(preference_contributor_, GetContribution).Times(0);

  EXPECT_THAT(processor_.GatherCommitContributions(/*max_entries=*/kMaxEntries),
              UnorderedElementsAre(
                  Pair(SHARING_MESSAGE, HasNumEntries(kNumReturnedEntries))));
}

TEST_F(CommitProcessorTest, ShouldGatherRegularUserTypes) {
  const int kNumReturnedBookmarks = 7;

  // High-priority types should be gathered, but no entries are produced. Nigori
  // is even gathered twice (once in each gathering phase).
  EXPECT_CALL(nigori_contributor_, GetContribution(kMaxEntries)).Times(2);
  EXPECT_CALL(sharing_message_contributor_, GetContribution(kMaxEntries));

  // Return |kNumReturnedBookmarks| bookmarks.
  EXPECT_CALL(bookmark_contributor_, GetContribution(kMaxEntries))
      .WillOnce(ReturnContributionWithEntries(kNumReturnedBookmarks));

  // Preferences should also be gathered, but no entries are produced in this
  // test. The precise argument depends on the iteration order so it's not
  // verified in this test.
  EXPECT_CALL(preference_contributor_, GetContribution);

  EXPECT_THAT(processor_.GatherCommitContributions(/*max_entries=*/kMaxEntries),
              UnorderedElementsAre(
                  Pair(BOOKMARKS, HasNumEntries(kNumReturnedBookmarks))));
}

TEST_F(CommitProcessorTest, ShouldGatherMultipleRegularUserTypes) {
  const int kNumReturnedBookmarks = 7;
  const int kNumReturnedPreferences = 8;

  // Return |kNumReturnedBookmarks| bookmarks and |kNumReturnedPreferences|
  // preferences.
  EXPECT_CALL(bookmark_contributor_, GetContribution)
      .WillOnce(ReturnContributionWithEntries(kNumReturnedBookmarks));
  EXPECT_CALL(preference_contributor_, GetContribution)
      .WillOnce(ReturnContributionWithEntries(kNumReturnedPreferences));

  EXPECT_THAT(processor_.GatherCommitContributions(/*max_entries=*/kMaxEntries),
              UnorderedElementsAre(
                  Pair(BOOKMARKS, HasNumEntries(kNumReturnedBookmarks)),
                  Pair(PREFERENCES, HasNumEntries(kNumReturnedPreferences))));
}

TEST_F(CommitProcessorTest, ShouldContinueGatheringPriorityContributions) {
  const int kNumReturnedSharingMessages = 3;

  // First, return |kMaxEntries| sharing messages.
  EXPECT_CALL(sharing_message_contributor_, GetContribution(kMaxEntries))
      .WillOnce(ReturnContributionWithEntries(kMaxEntries));

  // Non-priority user types shouldn't even be gathered.
  EXPECT_CALL(bookmark_contributor_, GetContribution).Times(0);
  EXPECT_CALL(preference_contributor_, GetContribution).Times(0);

  EXPECT_THAT(
      processor_.GatherCommitContributions(/*max_entries=*/kMaxEntries),
      UnorderedElementsAre(Pair(SHARING_MESSAGE, HasNumEntries(kMaxEntries))));

  // Now, return only |kNumReturnedSharingMessages| bookmarks (all that's left).
  EXPECT_CALL(sharing_message_contributor_, GetContribution)
      .WillOnce(ReturnContributionWithEntries(kNumReturnedSharingMessages));

  EXPECT_THAT(
      processor_.GatherCommitContributions(/*max_entries=*/kMaxEntries),
      UnorderedElementsAre(
          Pair(SHARING_MESSAGE, HasNumEntries(kNumReturnedSharingMessages))));

  // There are no contributions left, do not return any further and do not even
  // call the contributor.
  EXPECT_CALL(sharing_message_contributor_, GetContribution).Times(0);
  // At the same time, the other contributors should get called now (don't
  // return anything in this test).
  EXPECT_CALL(bookmark_contributor_, GetContribution).Times(1);
  EXPECT_CALL(preference_contributor_, GetContribution).Times(1);

  EXPECT_THAT(processor_.GatherCommitContributions(/*max_entries=*/kMaxEntries),
              IsEmpty());
}

TEST_F(CommitProcessorTest, ShouldContinueGatheringRegularContributions) {
  const int kNumReturnedBookmarks = 7;

  // First, return |kMaxEntries| bookmarks.
  EXPECT_CALL(bookmark_contributor_, GetContribution(kMaxEntries))
      .WillOnce(ReturnContributionWithEntries(kMaxEntries));

  EXPECT_THAT(
      processor_.GatherCommitContributions(/*max_entries=*/kMaxEntries),
      UnorderedElementsAre(Pair(BOOKMARKS, HasNumEntries(kMaxEntries))));

  // Now, return only |kNumReturnedBookmarks| bookmarks (all that's left).
  EXPECT_CALL(bookmark_contributor_, GetContribution)
      .WillOnce(ReturnContributionWithEntries(kNumReturnedBookmarks));

  EXPECT_THAT(processor_.GatherCommitContributions(/*max_entries=*/kMaxEntries),
              UnorderedElementsAre(
                  Pair(BOOKMARKS, HasNumEntries(kNumReturnedBookmarks))));

  // There are no contributions left, do not return any further and do not even
  // call the contributor.
  EXPECT_CALL(bookmark_contributor_, GetContribution).Times(0);

  EXPECT_THAT(processor_.GatherCommitContributions(/*max_entries=*/kMaxEntries),
              IsEmpty());
}

TEST_F(CommitProcessorTest,
       ShouldContinueGatheringRegularContributionsIfMatchingMaxEntries) {
  // Return |kMaxEntries| bookmarks.
  EXPECT_CALL(bookmark_contributor_, GetContribution(kMaxEntries))
      .WillOnce(ReturnContributionWithEntries(kMaxEntries));

  EXPECT_THAT(
      processor_.GatherCommitContributions(/*max_entries=*/kMaxEntries),
      UnorderedElementsAre(Pair(BOOKMARKS, HasNumEntries(kMaxEntries))));

  // There are no contributions left, do not return any further.
  // GetContribution() should however get called since |processor| cannot tell
  // that there are no left.
  EXPECT_CALL(bookmark_contributor_, GetContribution);

  EXPECT_THAT(processor_.GatherCommitContributions(/*max_entries=*/kMaxEntries),
              IsEmpty());
}

TEST_F(CommitProcessorTest, ShouldGatherFirstPriorityThenOtherUserTypes) {
  const int kNumReturnedSharingMessages = 3;
  const int kNumReturnedBookmarks = 7;
  const int kNumReturnedPreferences = 8;

  // All three types have non-zero contributions.
  EXPECT_CALL(sharing_message_contributor_, GetContribution(kMaxEntries))
      .WillOnce(ReturnContributionWithEntries(kNumReturnedSharingMessages))
      .RetiresOnSaturation();
  EXPECT_CALL(bookmark_contributor_, GetContribution)
      .WillOnce(ReturnContributionWithEntries(kNumReturnedBookmarks));
  EXPECT_CALL(preference_contributor_, GetContribution)
      .WillOnce(ReturnContributionWithEntries(kNumReturnedPreferences));

  // The first call should return only the priority types.
  EXPECT_THAT(
      processor_.GatherCommitContributions(/*max_entries=*/kMaxEntries),
      UnorderedElementsAre(
          Pair(SHARING_MESSAGE, HasNumEntries(kNumReturnedSharingMessages))));

  // Processor has gathered all contributions for SHARING_MESSAGE previously, no
  // further call should happen.
  EXPECT_CALL(sharing_message_contributor_, GetContribution(kMaxEntries))
      .Times(0);

  // The second call should return all the other types.
  EXPECT_THAT(processor_.GatherCommitContributions(/*max_entries=*/kMaxEntries),
              UnorderedElementsAre(
                  Pair(BOOKMARKS, HasNumEntries(kNumReturnedBookmarks)),
                  Pair(PREFERENCES, HasNumEntries(kNumReturnedPreferences))));
}

}  // namespace

}  // namespace syncer
