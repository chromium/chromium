// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/bookmark_update_preprocessing.h"

#include <stdint.h>

#include "base/test/metrics/histogram_tester.h"
#include "base/uuid.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/protocol/unique_position.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::Eq;
using testing::IsEmpty;
using testing::Ne;

enum class ExpectedBookmarkGuidSource {
  kSpecifics = 0,
  kValidOCII = 1,
  kDeprecatedLeftEmpty = 2,
  kInferred = 3,
  kLeftEmptyPossiblyForClientTag = 4,
  kMaxValue = kLeftEmptyPossiblyForClientTag,
};

TEST(BookmarkUpdatePreprocessingTest,
     ShouldPropagateUniquePositionFromSpecifics) {
  const UniquePosition kUniquePosition =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix());

  sync_pb::SyncEntity entity;
  *entity.mutable_specifics()->mutable_bookmark()->mutable_unique_position() =
      *entity.mutable_unique_position() = kUniquePosition.ToProto();

  const bool is_preprocessed =
      AdaptUniquePositionForBookmark(entity, entity.mutable_specifics());

  EXPECT_FALSE(is_preprocessed);
  EXPECT_TRUE(
      UniquePosition::FromProto(entity.specifics().bookmark().unique_position())
          .Equals(kUniquePosition));
}

TEST(BookmarkUpdatePreprocessingTest,
     ShouldPropagateUniquePositionFromSyncEntity) {
  const UniquePosition kUniquePosition =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix());

  sync_pb::SyncEntity entity;
  *entity.mutable_unique_position() = kUniquePosition.ToProto();

  const bool is_preprocessed =
      AdaptUniquePositionForBookmark(entity, entity.mutable_specifics());

  EXPECT_TRUE(is_preprocessed);
  EXPECT_TRUE(
      UniquePosition::FromProto(entity.specifics().bookmark().unique_position())
          .Equals(kUniquePosition));
}

TEST(BookmarkUpdatePreprocessingTest,
     ShouldComputeUniquePositionFromPositionInParent) {
  sync_pb::SyncEntity entity;
  entity.set_originator_cache_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  entity.set_originator_client_item_id("1");
  entity.set_position_in_parent(5);

  sync_pb::EntitySpecifics specifics1;
  bool is_preprocessed = AdaptUniquePositionForBookmark(entity, &specifics1);
  EXPECT_TRUE(is_preprocessed);

  sync_pb::EntitySpecifics specifics2;
  entity.set_position_in_parent(6);
  is_preprocessed = AdaptUniquePositionForBookmark(entity, &specifics2);
  EXPECT_TRUE(is_preprocessed);

  EXPECT_TRUE(UniquePosition::FromProto(specifics1.bookmark().unique_position())
                  .IsValid());
  EXPECT_TRUE(UniquePosition::FromProto(specifics2.bookmark().unique_position())
                  .IsValid());
  EXPECT_TRUE(UniquePosition::FromProto(specifics1.bookmark().unique_position())
                  .LessThan(UniquePosition::FromProto(
                      specifics2.bookmark().unique_position())));
}

TEST(BookmarkUpdatePreprocessingTest,
     ShouldComputeUniquePositionFromInsertAfterItemId) {
  sync_pb::SyncEntity entity;
  entity.set_originator_cache_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  entity.set_originator_client_item_id("1");
  entity.set_insert_after_item_id("ITEM_ID");

  const bool is_preprocessed =
      AdaptUniquePositionForBookmark(entity, entity.mutable_specifics());

  EXPECT_TRUE(is_preprocessed);
  EXPECT_TRUE(
      UniquePosition::FromProto(entity.specifics().bookmark().unique_position())
          .IsValid());
}

TEST(BookmarkUpdatePreprocessingTest, ShouldFallBackToRandomUniquePosition) {
  sync_pb::SyncEntity entity;
  const bool is_preprocessed =
      AdaptUniquePositionForBookmark(entity, entity.mutable_specifics());

  EXPECT_TRUE(is_preprocessed);
  EXPECT_TRUE(
      UniquePosition::FromProto(entity.specifics().bookmark().unique_position())
          .IsValid());
}

// Tests that AdaptGuidForBookmark() propagates GUID in specifics if the field
// is set (even if it doesn't match the originator client item ID).
TEST(BookmarkUpdatePreprocessingTest, ShouldPropagateGuidFromSpecifics) {
  const std::string kGuidInSpecifics =
      base::Uuid::GenerateRandomV4().AsLowercaseString();

  sync_pb::SyncEntity entity;
  entity.set_originator_cache_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  entity.set_originator_client_item_id(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  entity.mutable_specifics()->mutable_bookmark()->set_guid(kGuidInSpecifics);

  base::HistogramTester histogram_tester;
  sync_pb::EntitySpecifics specifics = entity.specifics();
  AdaptGuidForBookmark(entity, &specifics);

  EXPECT_THAT(specifics.bookmark().guid(), Eq(kGuidInSpecifics));

  histogram_tester.ExpectUniqueSample("Sync.BookmarkGUIDSource2",
                                      /*sample=*/
                                      ExpectedBookmarkGuidSource::kSpecifics,
                                      /*expected_bucket_count=*/1);
}

// Tests that AdaptGuidForBookmark() uses the originator client item ID as GUID
// when it is a valid GUID, and the GUID in specifics is not set.
TEST(BookmarkUpdatePreprocessingTest, ShouldUseOriginatorClientItemIdAsGuid) {
  const std::string kOriginatorClientItemId =
      base::Uuid::GenerateRandomV4().AsLowercaseString();

  sync_pb::SyncEntity entity;
  entity.set_originator_cache_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  entity.set_originator_client_item_id(kOriginatorClientItemId);
  entity.mutable_specifics()->mutable_bookmark();

  base::HistogramTester histogram_tester;
  sync_pb::EntitySpecifics specifics = entity.specifics();
  AdaptGuidForBookmark(entity, &specifics);

  EXPECT_THAT(specifics.bookmark().guid(), Eq(kOriginatorClientItemId));

  histogram_tester.ExpectUniqueSample("Sync.BookmarkGUIDSource2",
                                      /*sample=*/
                                      ExpectedBookmarkGuidSource::kValidOCII,
                                      /*expected_bucket_count=*/1);
}

// Tests that AdaptGuidForBookmark() infers the GUID when the field in specifics
// is empty and originator client item ID is not a valid GUID.
TEST(BookmarkUpdatePreprocessingTest, ShouldInferGuid) {
  const std::string kOriginatorClientItemId = "1";

  sync_pb::SyncEntity entity;
  entity.set_originator_cache_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  entity.set_originator_client_item_id(kOriginatorClientItemId);
  entity.mutable_specifics()->mutable_bookmark();

  base::HistogramTester histogram_tester;
  sync_pb::EntitySpecifics specifics = entity.specifics();
  AdaptGuidForBookmark(entity, &specifics);

  EXPECT_TRUE(
      base::Uuid::ParseLowercase(specifics.bookmark().guid()).is_valid());

  histogram_tester.ExpectUniqueSample("Sync.BookmarkGUIDSource2",
                                      /*sample=*/
                                      ExpectedBookmarkGuidSource::kInferred,
                                      /*expected_bucket_count=*/1);
}

TEST(BookmarkUpdatePreprocessingTest,
     ShouldNotInferGuidIfNoOriginatorInformation) {
  const std::string kOriginatorClientItemId = "1";

  sync_pb::SyncEntity entity;
  entity.mutable_specifics()->mutable_bookmark();

  base::HistogramTester histogram_tester;
  sync_pb::EntitySpecifics specifics = entity.specifics();
  AdaptGuidForBookmark(entity, &specifics);

  EXPECT_FALSE(specifics.bookmark().has_guid());

  histogram_tester.ExpectUniqueSample(
      "Sync.BookmarkGUIDSource2",
      /*sample=*/
      ExpectedBookmarkGuidSource::kLeftEmptyPossiblyForClientTag,
      /*expected_bucket_count=*/1);
}

// Tests that inferred GUIDs are computed deterministically.
TEST(BookmarkUpdatePreprocessingTest, ShouldInferDeterministicGuid) {
  EXPECT_THAT(InferGuidForLegacyBookmarkForTesting("cacheguid1", "1"),
              Eq(InferGuidForLegacyBookmarkForTesting("cacheguid1", "1")));
  EXPECT_THAT(InferGuidForLegacyBookmarkForTesting("cacheguid1", "2"),
              Eq(InferGuidForLegacyBookmarkForTesting("cacheguid1", "2")));
}

// Tests that unique GUIDs are produced if either of the two involved fields
// changes.
TEST(BookmarkUpdatePreprocessingTest, ShouldInferDistictGuids) {
  EXPECT_THAT(InferGuidForLegacyBookmarkForTesting("cacheguid1", "1"),
              Ne(InferGuidForLegacyBookmarkForTesting("cacheguid1", "2")));
  EXPECT_THAT(InferGuidForLegacyBookmarkForTesting("cacheguid1", "1"),
              Ne(InferGuidForLegacyBookmarkForTesting("cacheguid2", "1")));
}

TEST(BookmarkUpdatePreprocessingTest, ShouldUseTypeInSpecifics) {
  sync_pb::SyncEntity entity;
  sync_pb::EntitySpecifics specifics;

  *specifics.mutable_bookmark() =
      sync_pb::BookmarkSpecifics::default_instance();
  specifics.mutable_bookmark()->set_type(sync_pb::BookmarkSpecifics::FOLDER);
  AdaptTypeForBookmark(entity, &specifics);
  EXPECT_THAT(specifics.bookmark().type(),
              Eq(sync_pb::BookmarkSpecifics::FOLDER));

  *specifics.mutable_bookmark() =
      sync_pb::BookmarkSpecifics::default_instance();
  specifics.mutable_bookmark()->set_type(sync_pb::BookmarkSpecifics::URL);
  AdaptTypeForBookmark(entity, &specifics);
  EXPECT_THAT(specifics.bookmark().type(), Eq(sync_pb::BookmarkSpecifics::URL));

  // Even if SyncEntity says otherwise, specifics should prevail.
  entity.set_folder(true);
  *specifics.mutable_bookmark() =
      sync_pb::BookmarkSpecifics::default_instance();
  specifics.mutable_bookmark()->set_type(sync_pb::BookmarkSpecifics::URL);
  AdaptTypeForBookmark(entity, &specifics);
  EXPECT_THAT(specifics.bookmark().type(), Eq(sync_pb::BookmarkSpecifics::URL));
}

TEST(BookmarkUpdatePreprocessingTest,
     ShouldInferTypeFromFolderFieldInSyncEntity) {
  sync_pb::SyncEntity entity;
  sync_pb::EntitySpecifics specifics;

  *specifics.mutable_bookmark() =
      sync_pb::BookmarkSpecifics::default_instance();
  entity.set_folder(true);
  AdaptTypeForBookmark(entity, &specifics);
  EXPECT_THAT(specifics.bookmark().type(),
              Eq(sync_pb::BookmarkSpecifics::FOLDER));

  *specifics.mutable_bookmark() =
      sync_pb::BookmarkSpecifics::default_instance();
  entity.set_folder(false);
  AdaptTypeForBookmark(entity, &specifics);
  EXPECT_THAT(specifics.bookmark().type(), Eq(sync_pb::BookmarkSpecifics::URL));
}

TEST(BookmarkUpdatePreprocessingTest,
     ShouldInferTypeFromPresenceOfUrlInSpecifics) {
  sync_pb::SyncEntity entity;
  sync_pb::EntitySpecifics specifics;

  *specifics.mutable_bookmark() =
      sync_pb::BookmarkSpecifics::default_instance();
  AdaptTypeForBookmark(entity, &specifics);
  EXPECT_THAT(specifics.bookmark().type(),
              Eq(sync_pb::BookmarkSpecifics::FOLDER));

  *specifics.mutable_bookmark() =
      sync_pb::BookmarkSpecifics::default_instance();
  specifics.mutable_bookmark()->set_url("http://foo.com");
  AdaptTypeForBookmark(entity, &specifics);
  EXPECT_THAT(specifics.bookmark().type(), Eq(sync_pb::BookmarkSpecifics::URL));
}

}  // namespace

}  // namespace syncer
