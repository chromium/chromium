// Copyright 2020 The Chromium Authors
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
constexpr size_t kMaxBytes = 1000;

using testing::_;
using testing::IsEmpty;
using testing::Pair;
using testing::UnorderedElementsAre;

MATCHER_P(HasNumEntries, num_entries, "") {
  return static_cast<int>(arg->GetNumEntries()) == num_entries;
}

// Simple implementation of CommitContribution that only implements
// GetNumEntries(), GetPostEncryptionSpecificsByteSize(), and
// HasMoreLocalChanges().
class FakeCommitContribution : public CommitContribution {
 public:
  FakeCommitContribution(size_t num_entries,
                         size_t specifics_byte_size,
                         bool has_more_local_changes)
      : num_entries_(num_entries),
        specifics_byte_size_(specifics_byte_size),
        has_more_local_changes_(has_more_local_changes) {}

  ~FakeCommitContribution() override = default;

  // CommitContribution implementation.
  void AddToCommitMessage(sync_pb::ClientToServerMessage* msg) override {}
  SyncerError ProcessCommitResponse(
      const sync_pb::ClientToServerResponse& response,
      StatusController* status) override {
    return SyncerError::Success();
  }
  void ProcessCommitFailure(SyncCommitError commit_error) override {}
  size_t GetNumEntries() const override { return num_entries_; }
  size_t GetPostEncryptionSpecificsByteSize() const override {
    return specifics_byte_size_;
  }
  bool HasMoreLocalChanges() const override { return has_more_local_changes_; }

 private:
  const size_t num_entries_;
  const size_t specifics_byte_size_;
  const bool has_more_local_changes_;
};

ACTION_P(ReturnContributionWithEntries, num_entries) {
  return std::make_unique<FakeCommitContribution>(
      num_entries,
      /*specifics_byte_size=*/0,
      /*has_more_local_changes=*/false);
}

ACTION_P(ReturnPartialContributionWithEntries, num_entries) {
  return std::make_unique<FakeCommitContribution>(
      num_entries,
      /*specifics_byte_size=*/0,
      /*has_more_local_changes=*/true);
}

ACTION_P3(ReturnContribution,
          num_entries,
          specifics_byte_size,
          has_more_local_changes) {
  return std::make_unique<FakeCommitContribution>(
      num_entries, specifics_byte_size, has_more_local_changes);
}

class MockCommitContributor : public CommitContributor {
 public:
  MockCommitContributor() = default;
  ~MockCommitContributor() override = default;
  MOCK_METHOD(std::unique_ptr<CommitContribution>,
              GetContribution,
              (size_t max_entries, size_t max_target_specifics_bytes),
              (override));
};

class CommitProcessorTest : public testing::Test {
 protected:
  CommitProcessorTest()
      : contributor_map_{{NIGORI, &nigori_contributor_},
                         {SHARING_MESSAGE, &sharing_message_contributor_},
                         {BOOKMARKS, &bookmark_contributor_},
                         {PREFERENCES, &preference_contributor_},
                         {HISTORY, &history_contributor_}},
        processor_(
            /*commit_types=*/{NIGORI, SHARING_MESSAGE, BOOKMARKS, PREFERENCES,
                              HISTORY},
            &contributor_map_) {
    EXPECT_TRUE(HighPriorityUserTypes().Has(SHARING_MESSAGE));
    EXPECT_FALSE(HighPriorityUserTypes().Has(BOOKMARKS));
    EXPECT_FALSE(HighPriorityUserTypes().Has(PREFERENCES));
    EXPECT_TRUE(LowPriorityUserTypes().Has(HISTORY));
  }

  testing::NiceMock<MockCommitContributor> nigori_contributor_;

  // A high-priority user type.
  testing::NiceMock<MockCommitContributor> sharing_message_contributor_;

  // Regular user types.
  testing::NiceMock<MockCommitContributor> bookmark_contributor_;
  testing::NiceMock<MockCommitContributor> preference_contributor_;

  // A low-priority user type.
  testing::NiceMock<MockCommitContributor> history_contributor_;

  CommitContributorMap contributor_map_;
  CommitProcessor processor_;
};

TEST_F(CommitProcessorTest, ShouldGatherNigoriOnlyContribution) {
  EXPECT_CALL(nigori_contributor_, GetContribution(kMaxEntries, kMaxBytes))
      .WillOnce(ReturnContributionWithEntries(/*num_entries=*/1));

  // No user types should be gathered and combined with NIGORI.
  EXPECT_CALL(sharing_message_contributor_, GetContribution).Times(0);
  EXPECT_CALL(bookmark_contributor_, GetContribution).Times(0);
  EXPECT_CALL(preference_contributor_, GetContribution).Times(0);
  EXPECT_CALL(history_contributor_, GetContribution).Times(0);

  EXPECT_THAT(processor_.GatherCommitContributions(
                  /*max_entries=*/kMaxEntries,
                  /*max_target_specifics_bytes=*/kMaxBytes),
              UnorderedElementsAre(Pair(NIGORI, HasNumEntries(1))));
}

TEST_F(CommitProcessorTest, ShouldGatherHighPriorityUserTypesOnlyContribution) {
  const int kNumReturnedEntries = 3;

  testing::Sequence s1, s2, s3;

  EXPECT_CALL(sharing_message_contributor_,
              GetContribution(kMaxEntries, kMaxBytes))
      .InSequence(s1, s2, s3)
      .WillOnce(ReturnContributionWithEntries(kNumReturnedEntries));

  // Non-priority user types should be gathered after the high-priority types,
  // but the relative ordering between these is unspecified.
  EXPECT_CALL(bookmark_contributor_, GetContribution).InSequence(s1);
  EXPECT_CALL(preference_contributor_, GetContribution).InSequence(s2);
  EXPECT_CALL(history_contributor_, GetContribution).InSequence(s3);

  EXPECT_THAT(processor_.GatherCommitContributions(
                  /*max_entries=*/kMaxEntries,
                  /*max_target_specifics_bytes=*/kMaxBytes),
              UnorderedElementsAre(
                  Pair(SHARING_MESSAGE, HasNumEntries(kNumReturnedEntries))));
}

TEST_F(CommitProcessorTest, ShouldGatherRegularUserTypes) {
  const int kNumReturnedBookmarks = 7;

  // High-priority types should be gathered, but no entries are produced.
  EXPECT_CALL(nigori_contributor_, GetContribution(kMaxEntries, kMaxBytes));
  EXPECT_CALL(sharing_message_contributor_,
              GetContribution(kMaxEntries, kMaxBytes));

  // Return `kNumReturnedBookmarks` bookmarks.
  EXPECT_CALL(bookmark_contributor_, GetContribution(kMaxEntries, kMaxBytes))
      .WillOnce(ReturnContributionWithEntries(kNumReturnedBookmarks));

  // Preferences should also be gathered, but no entries are produced in this
  // test. The precise argument depends on the iteration order so it's not
  // verified in this test.
  EXPECT_CALL(preference_contributor_, GetContribution);

  // Since the regular types don't exhaust `kMaxEntries`, the low-priority types
  // should also be gathered (but don't have any contributions).
  EXPECT_CALL(history_contributor_, GetContribution);

  EXPECT_THAT(processor_.GatherCommitContributions(
                  /*max_entries=*/kMaxEntries,
                  /*max_target_specifics_bytes=*/kMaxBytes),
              UnorderedElementsAre(
                  Pair(BOOKMARKS, HasNumEntries(kNumReturnedBookmarks))));
}

TEST_F(CommitProcessorTest, ShouldGatherLowPriorityUserTypes) {
  const int kNumReturnedHistory = 7;

  // High-priority types and regular types should be gathered, but no entries
  // are produced.
  EXPECT_CALL(nigori_contributor_, GetContribution(kMaxEntries, kMaxBytes));
  EXPECT_CALL(sharing_message_contributor_,
              GetContribution(kMaxEntries, kMaxBytes));
  EXPECT_CALL(bookmark_contributor_, GetContribution(kMaxEntries, kMaxBytes));
  EXPECT_CALL(preference_contributor_, GetContribution(kMaxEntries, kMaxBytes));

  // Return `kNumReturnedHistory` history entries.
  EXPECT_CALL(history_contributor_, GetContribution(kMaxEntries, kMaxBytes))
      .WillOnce(ReturnContributionWithEntries(kNumReturnedHistory));

  EXPECT_THAT(
      processor_.GatherCommitContributions(
          /*max_entries=*/kMaxEntries,
          /*max_target_specifics_bytes=*/kMaxBytes),
      UnorderedElementsAre(Pair(HISTORY, HasNumEntries(kNumReturnedHistory))));
}

TEST_F(CommitProcessorTest, ShouldGatherMultipleRegularUserTypes) {
  const int kNumReturnedBookmarks = 7;
  const int kNumReturnedPreferences = 8;
  static_assert(kNumReturnedBookmarks + kNumReturnedPreferences < kMaxEntries);

  // Return `kNumReturnedBookmarks` bookmarks and `kNumReturnedPreferences`
  // preferences.
  EXPECT_CALL(bookmark_contributor_, GetContribution)
      .WillOnce(ReturnContributionWithEntries(kNumReturnedBookmarks));
  EXPECT_CALL(preference_contributor_, GetContribution)
      .WillOnce(ReturnContributionWithEntries(kNumReturnedPreferences));

  // Since bookmarks+preferences don't exhaust `kMaxEntries`, the low-priority
  // types should also be gathered (but don't have any contributions).
  EXPECT_CALL(history_contributor_, GetContribution);

  EXPECT_THAT(processor_.GatherCommitContributions(
                  /*max_entries=*/kMaxEntries,
                  /*max_target_specifics_bytes=*/kMaxBytes),
              UnorderedElementsAre(
                  Pair(BOOKMARKS, HasNumEntries(kNumReturnedBookmarks)),
                  Pair(PREFERENCES, HasNumEntries(kNumReturnedPreferences))));
}

TEST_F(CommitProcessorTest, ShouldContinueGatheringHighPriorityContributions) {
  const int kNumReturnedSharingMessages = 3;

  // First, return `kMaxEntries` sharing messages with more changes left.
  EXPECT_CALL(sharing_message_contributor_,
              GetContribution(kMaxEntries, kMaxBytes))
      .WillOnce(ReturnPartialContributionWithEntries(kMaxEntries));

  // Non-priority user types shouldn't even be gathered.
  EXPECT_CALL(bookmark_contributor_, GetContribution).Times(0);
  EXPECT_CALL(preference_contributor_, GetContribution).Times(0);
  EXPECT_CALL(history_contributor_, GetContribution).Times(0);

  EXPECT_THAT(
      processor_.GatherCommitContributions(
          /*max_entries=*/kMaxEntries,
          /*max_target_specifics_bytes=*/kMaxBytes),
      UnorderedElementsAre(Pair(SHARING_MESSAGE, HasNumEntries(kMaxEntries))));

  // Now, return only `kNumReturnedSharingMessages` sharing messages (all that's
  // left).
  EXPECT_CALL(sharing_message_contributor_, GetContribution)
      .WillOnce(ReturnContributionWithEntries(kNumReturnedSharingMessages));
  // At this point, there's capacity left, so the non-priority user types should
  // also be gathered (but they don't have any contributions).
  EXPECT_CALL(bookmark_contributor_, GetContribution);
  EXPECT_CALL(preference_contributor_, GetContribution);
  EXPECT_CALL(history_contributor_, GetContribution);

  EXPECT_THAT(
      processor_.GatherCommitContributions(
          /*max_entries=*/kMaxEntries,
          /*max_target_specifics_bytes=*/kMaxBytes),
      UnorderedElementsAre(
          Pair(SHARING_MESSAGE, HasNumEntries(kNumReturnedSharingMessages))));

  // There are no contributions left, and the contributors should not even be
  // called again.
  EXPECT_CALL(sharing_message_contributor_, GetContribution).Times(0);
  EXPECT_CALL(bookmark_contributor_, GetContribution).Times(0);
  EXPECT_CALL(preference_contributor_, GetContribution).Times(0);
  EXPECT_CALL(history_contributor_, GetContribution).Times(0);

  EXPECT_THAT(processor_.GatherCommitContributions(
                  /*max_entries=*/kMaxEntries,
                  /*max_target_specifics_bytes=*/kMaxBytes),
              IsEmpty());
}

TEST_F(CommitProcessorTest, ShouldContinueGatheringRegularContributions) {
  const int kNumReturnedBookmarks = 7;

  // First, return `kMaxEntries` bookmarks with more changes left.
  EXPECT_CALL(bookmark_contributor_, GetContribution(kMaxEntries, kMaxBytes))
      .WillOnce(ReturnPartialContributionWithEntries(kMaxEntries));

  EXPECT_THAT(
      processor_.GatherCommitContributions(
          /*max_entries=*/kMaxEntries,
          /*max_target_specifics_bytes=*/kMaxBytes),
      UnorderedElementsAre(Pair(BOOKMARKS, HasNumEntries(kMaxEntries))));

  // Now, return only `kNumReturnedBookmarks` bookmarks (all that's left).
  EXPECT_CALL(bookmark_contributor_, GetContribution)
      .WillOnce(ReturnContributionWithEntries(kNumReturnedBookmarks));

  EXPECT_THAT(processor_.GatherCommitContributions(
                  /*max_entries=*/kMaxEntries,
                  /*max_target_specifics_bytes=*/kMaxBytes),
              UnorderedElementsAre(
                  Pair(BOOKMARKS, HasNumEntries(kNumReturnedBookmarks))));

  // There are no contributions left, do not return any further and do not even
  // call the contributor.
  EXPECT_CALL(bookmark_contributor_, GetContribution).Times(0);

  EXPECT_THAT(processor_.GatherCommitContributions(
                  /*max_entries=*/kMaxEntries,
                  /*max_target_specifics_bytes=*/kMaxBytes),
              IsEmpty());
}

TEST_F(CommitProcessorTest, ShouldContinueGatheringLowPriorityContributions) {
  const int kNumReturnedHistory = 7;

  // First, return `kMaxEntries` history entries with more changes left.
  EXPECT_CALL(history_contributor_, GetContribution(kMaxEntries, kMaxBytes))
      .WillOnce(ReturnPartialContributionWithEntries(kMaxEntries));

  EXPECT_THAT(processor_.GatherCommitContributions(
                  /*max_entries=*/kMaxEntries,
                  /*max_target_specifics_bytes=*/kMaxBytes),
              UnorderedElementsAre(Pair(HISTORY, HasNumEntries(kMaxEntries))));

  // Now, return only `kNumReturnedHistory` entries (all that's left).
  EXPECT_CALL(history_contributor_, GetContribution)
      .WillOnce(ReturnContributionWithEntries(kNumReturnedHistory));

  EXPECT_THAT(
      processor_.GatherCommitContributions(
          /*max_entries=*/kMaxEntries,
          /*max_target_specifics_bytes=*/kMaxBytes),
      UnorderedElementsAre(Pair(HISTORY, HasNumEntries(kNumReturnedHistory))));

  // There are no contributions left, do not return any further and do not even
  // call the contributor.
  EXPECT_CALL(history_contributor_, GetContribution).Times(0);

  EXPECT_THAT(processor_.GatherCommitContributions(
                  /*max_entries=*/kMaxEntries,
                  /*max_target_specifics_bytes=*/kMaxBytes),
              IsEmpty());
}

TEST_F(CommitProcessorTest,
       ShouldContinueGatheringRegularContributionsIfMatchingMaxEntries) {
  // Return `kMaxEntries` bookmarks.
  EXPECT_CALL(bookmark_contributor_, GetContribution(kMaxEntries, kMaxBytes))
      .WillOnce(ReturnContributionWithEntries(kMaxEntries));

  EXPECT_THAT(
      processor_.GatherCommitContributions(
          /*max_entries=*/kMaxEntries,
          /*max_target_specifics_bytes=*/kMaxBytes),
      UnorderedElementsAre(Pair(BOOKMARKS, HasNumEntries(kMaxEntries))));

  // There are no contributions left, do not return any further.
  // GetContribution() should however get called since `PREFERENCES` (another
  // regular type) was not queried yet in the first pass.
  EXPECT_CALL(bookmark_contributor_, GetContribution);

  EXPECT_THAT(processor_.GatherCommitContributions(
                  /*max_entries=*/kMaxEntries,
                  /*max_target_specifics_bytes=*/kMaxBytes),
              IsEmpty());
}

TEST_F(CommitProcessorTest, ShouldGatherInPriorityOrder) {
  const int kNumReturnedSharingMessages = 3;
  const int kNumReturnedBookmarks1 = kMaxEntries - kNumReturnedSharingMessages;
  const int kNumReturnedBookmarks2 = 4;
  const int kNumReturnedHistory = 5;

  // A high-priority type, a regular type, and a low-priority type all have
  // non-zero contributions.
  testing::InSequence s;
  // First pass: High-priority, and parts of regular.
  EXPECT_CALL(sharing_message_contributor_,
              GetContribution(kMaxEntries, kMaxBytes))
      .WillOnce(ReturnContributionWithEntries(kNumReturnedSharingMessages))
      .RetiresOnSaturation();
  EXPECT_CALL(bookmark_contributor_, GetContribution)
      .WillOnce(ReturnPartialContributionWithEntries(kNumReturnedBookmarks1));
  // Second pass: Remaining regular, and low-priority.
  EXPECT_CALL(bookmark_contributor_, GetContribution)
      .WillOnce(ReturnContributionWithEntries(kNumReturnedBookmarks2));
  EXPECT_CALL(history_contributor_, GetContribution)
      .WillOnce(ReturnContributionWithEntries(kNumReturnedHistory));

  // The first call should return the high-priority types, and as much of the
  // regular-priority types as still fits in the budget.
  EXPECT_THAT(
      processor_.GatherCommitContributions(
          /*max_entries=*/kMaxEntries,
          /*max_target_specifics_bytes=*/kMaxBytes),
      UnorderedElementsAre(
          Pair(SHARING_MESSAGE, HasNumEntries(kNumReturnedSharingMessages)),
          Pair(BOOKMARKS, HasNumEntries(kNumReturnedBookmarks1))));

  // Processor has gathered all contributions for SHARING_MESSAGE previously, no
  // further call should happen.
  EXPECT_CALL(sharing_message_contributor_,
              GetContribution(kMaxEntries, kMaxBytes))
      .Times(0);

  // The second call should return the remaining regular types as well as the
  // low-priority types.
  EXPECT_THAT(processor_.GatherCommitContributions(
                  /*max_entries=*/kMaxEntries,
                  /*max_target_specifics_bytes=*/kMaxBytes),
              UnorderedElementsAre(
                  Pair(BOOKMARKS, HasNumEntries(kNumReturnedBookmarks2)),
                  Pair(HISTORY, HasNumEntries(kNumReturnedHistory))));

  // All contributions were gathered; no further calls should happen.
  EXPECT_CALL(bookmark_contributor_, GetContribution(kMaxEntries, kMaxBytes))
      .Times(0);
  EXPECT_CALL(preference_contributor_, GetContribution(kMaxEntries, kMaxBytes))
      .Times(0);
  EXPECT_CALL(history_contributor_, GetContribution(kMaxEntries, kMaxBytes))
      .Times(0);

  EXPECT_THAT(processor_.GatherCommitContributions(
                  /*max_entries=*/kMaxEntries,
                  /*max_target_specifics_bytes=*/kMaxBytes),
              IsEmpty());
}

TEST_F(CommitProcessorTest, ShouldRespectMaxBytesAcrossPhases) {
  const int kNumReturnedSharingMessages = 2;
  const size_t kSharingMessageBytes = 400;
  const int kNumReturnedBookmarks1 = 3;
  const size_t kBookmarkBytes1 = kMaxBytes - kSharingMessageBytes;
  const int kNumReturnedBookmarks2 = 2;
  const size_t kBookmarkBytes2 = 300;
  const int kNumReturnedHistory = 1;
  const size_t kHistoryBytes = 200;

  testing::InSequence s;
  // First pass: High-priority uses 400 bytes, regular uses remaining 600 bytes.
  EXPECT_CALL(sharing_message_contributor_,
              GetContribution(kMaxEntries, kMaxBytes))
      .WillOnce(ReturnContribution(kNumReturnedSharingMessages,
                                   kSharingMessageBytes,
                                   /*has_more_local_changes=*/false))
      .RetiresOnSaturation();
  EXPECT_CALL(bookmark_contributor_,
              GetContribution(kMaxEntries - kNumReturnedSharingMessages,
                              kMaxBytes - kSharingMessageBytes))
      .WillOnce(ReturnContribution(kNumReturnedBookmarks1, kBookmarkBytes1,
                                   /*has_more_local_changes=*/true));
  // Second pass: Remaining regular, and low-priority.
  EXPECT_CALL(bookmark_contributor_, GetContribution(kMaxEntries, kMaxBytes))
      .WillOnce(ReturnContribution(kNumReturnedBookmarks2, kBookmarkBytes2,
                                   /*has_more_local_changes=*/false));
  EXPECT_CALL(preference_contributor_,
              GetContribution(kMaxEntries - kNumReturnedBookmarks2,
                              kMaxBytes - kBookmarkBytes2));
  EXPECT_CALL(history_contributor_,
              GetContribution(kMaxEntries - kNumReturnedBookmarks2,
                              kMaxBytes - kBookmarkBytes2))
      .WillOnce(ReturnContribution(kNumReturnedHistory, kHistoryBytes,
                                   /*has_more_local_changes=*/false));

  EXPECT_THAT(
      processor_.GatherCommitContributions(
          /*max_entries=*/kMaxEntries,
          /*max_target_specifics_bytes=*/kMaxBytes),
      UnorderedElementsAre(
          Pair(SHARING_MESSAGE, HasNumEntries(kNumReturnedSharingMessages)),
          Pair(BOOKMARKS, HasNumEntries(kNumReturnedBookmarks1))));

  EXPECT_THAT(processor_.GatherCommitContributions(
                  /*max_entries=*/kMaxEntries,
                  /*max_target_specifics_bytes=*/kMaxBytes),
              UnorderedElementsAre(
                  Pair(BOOKMARKS, HasNumEntries(kNumReturnedBookmarks2)),
                  Pair(HISTORY, HasNumEntries(kNumReturnedHistory))));

  EXPECT_THAT(processor_.GatherCommitContributions(
                  /*max_entries=*/kMaxEntries,
                  /*max_target_specifics_bytes=*/kMaxBytes),
              IsEmpty());
}

TEST_F(CommitProcessorTest, ShouldRespectSingleByteMaxBytes) {
  testing::InSequence s;
  // First pass: High-priority has nothing, regular type BOOKMARKS contributes
  // 1 entry, which already exceeds `max_target_specifics_bytes=1` and stops
  // before PREFERENCES.
  EXPECT_CALL(sharing_message_contributor_,
              GetContribution(kMaxEntries, /*max_target_specifics_bytes=*/1));
  EXPECT_CALL(bookmark_contributor_,
              GetContribution(kMaxEntries, /*max_target_specifics_bytes=*/1))
      .WillOnce(ReturnContribution(
          /*num_entries=*/1,
          /*specifics_byte_size=*/100,
          /*has_more_local_changes=*/false));
  // Second pass: BOOKMARKS has nothing left, PREFERENCES (the last regular
  // type) contributes 1 entry, which also exceeds
  // `max_target_specifics_bytes=1` and stops before querying HISTORY.
  EXPECT_CALL(bookmark_contributor_,
              GetContribution(kMaxEntries, /*max_target_specifics_bytes=*/1));
  EXPECT_CALL(preference_contributor_,
              GetContribution(kMaxEntries, /*max_target_specifics_bytes=*/1))
      .WillOnce(ReturnContribution(
          /*num_entries=*/1,
          /*specifics_byte_size=*/100,
          /*has_more_local_changes=*/false));
  // Third pass: Regular types now have nothing left, so regular phase
  // completes and low-priority types are queried.
  EXPECT_CALL(bookmark_contributor_,
              GetContribution(kMaxEntries, /*max_target_specifics_bytes=*/1));
  EXPECT_CALL(preference_contributor_,
              GetContribution(kMaxEntries, /*max_target_specifics_bytes=*/1));
  EXPECT_CALL(history_contributor_,
              GetContribution(kMaxEntries, /*max_target_specifics_bytes=*/1));

  EXPECT_THAT(
      processor_.GatherCommitContributions(/*max_entries=*/kMaxEntries,
                                           /*max_target_specifics_bytes=*/1),
      UnorderedElementsAre(Pair(BOOKMARKS, HasNumEntries(1))));

  EXPECT_THAT(
      processor_.GatherCommitContributions(/*max_entries=*/kMaxEntries,
                                           /*max_target_specifics_bytes=*/1),
      UnorderedElementsAre(Pair(PREFERENCES, HasNumEntries(1))));

  EXPECT_THAT(
      processor_.GatherCommitContributions(/*max_entries=*/kMaxEntries,
                                           /*max_target_specifics_bytes=*/1),
      IsEmpty());
}

TEST_F(CommitProcessorTest,
       ShouldStopWhenContributionHasMoreLocalChangesBelowLimits) {
  // Even if the returned contribution has fewer entries than `kMaxEntries` and
  // fewer bytes than `kMaxBytes` (e.g. due to pre-encryption byte limit
  // scaling), `HasMoreLocalChanges() == true` should stop gathering and keep
  // the current phase active for the next call.
  EXPECT_CALL(sharing_message_contributor_,
              GetContribution(kMaxEntries, kMaxBytes))
      .WillOnce(ReturnContribution(/*num_entries=*/1,
                                   /*specifics_byte_size=*/500,
                                   /*has_more_local_changes=*/true));
  EXPECT_CALL(bookmark_contributor_, GetContribution).Times(0);
  EXPECT_CALL(preference_contributor_, GetContribution).Times(0);
  EXPECT_CALL(history_contributor_, GetContribution).Times(0);

  EXPECT_THAT(processor_.GatherCommitContributions(
                  /*max_entries=*/kMaxEntries,
                  /*max_target_specifics_bytes=*/kMaxBytes),
              UnorderedElementsAre(Pair(SHARING_MESSAGE, HasNumEntries(1))));

  // Second pass should still query `SHARING_MESSAGE`.
  EXPECT_CALL(sharing_message_contributor_,
              GetContribution(kMaxEntries, kMaxBytes))
      .WillOnce(ReturnContribution(/*num_entries=*/1,
                                   /*specifics_byte_size=*/200,
                                   /*has_more_local_changes=*/false));
  EXPECT_CALL(bookmark_contributor_, GetContribution);
  EXPECT_CALL(preference_contributor_, GetContribution);
  EXPECT_CALL(history_contributor_, GetContribution);

  EXPECT_THAT(processor_.GatherCommitContributions(
                  /*max_entries=*/kMaxEntries,
                  /*max_target_specifics_bytes=*/kMaxBytes),
              UnorderedElementsAre(Pair(SHARING_MESSAGE, HasNumEntries(1))));
}

}  // namespace

}  // namespace syncer
