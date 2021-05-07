// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/bookmark_update_preprocessing.h"

#include <stdint.h>

#include "base/guid.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine/entity_data.h"
#include "components/sync/protocol/sync.pb.h"
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
  kMaxValue = kInferred,
};

TEST(BookmarkUpdatePreprocessingTest, ShouldPropagateUniquePosition) {
  sync_pb::SyncEntity entity;
  entity.set_originator_cache_guid(base::GenerateGUID());
  entity.set_originator_client_item_id("1");
  *entity.mutable_unique_position() =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix()).ToProto();

  EntityData entity_data;
  AdaptUniquePositionForBookmark(entity, &entity_data);

  EXPECT_TRUE(entity_data.unique_position.IsValid());
}

TEST(BookmarkUpdatePreprocessingTest,
     ShouldComputeUniquePositionFromPositionInParent) {
  sync_pb::SyncEntity entity;
  entity.set_originator_cache_guid(base::GenerateGUID());
  entity.set_originator_client_item_id("1");
  entity.set_position_in_parent(5);

  EntityData entity_data;
  AdaptUniquePositionForBookmark(entity, &entity_data);

  EXPECT_TRUE(entity_data.unique_position.IsValid());
}

TEST(BookmarkUpdatePreprocessingTest,
     ShouldComputeUniquePositionFromInsertAfterItemId) {
  sync_pb::SyncEntity entity;
  entity.set_originator_cache_guid(base::GenerateGUID());
  entity.set_originator_client_item_id("1");
  entity.set_insert_after_item_id("ITEM_ID");

  EntityData entity_data;
  AdaptUniquePositionForBookmark(entity, &entity_data);

  EXPECT_TRUE(entity_data.unique_position.IsValid());
}

// Tests that AdaptGuidForBookmark() propagates GUID in specifics if the field
// is set (even if it doesn't match the originator client item ID).
TEST(BookmarkUpdatePreprocessingTest, ShouldPropagateGuidFromSpecifics) {
  const std::string kGuidInSpecifics = base::GenerateGUID();

  sync_pb::SyncEntity entity;
  entity.set_originator_cache_guid(base::GenerateGUID());
  entity.set_originator_client_item_id(base::GenerateGUID());
  entity.mutable_specifics()->mutable_bookmark()->set_guid(kGuidInSpecifics);

  base::HistogramTester histogram_tester;
  sync_pb::EntitySpecifics specifics = entity.specifics();
  EXPECT_FALSE(AdaptGuidForBookmark(entity, &specifics));

  EXPECT_THAT(specifics.bookmark().guid(), Eq(kGuidInSpecifics));

  histogram_tester.ExpectUniqueSample("Sync.BookmarkGUIDSource2",
                                      /*sample=*/
                                      ExpectedBookmarkGuidSource::kSpecifics,
                                      /*count=*/1);
}

// Tests that AdaptGuidForBookmark() uses the originator client item ID as GUID
// when it is a valid GUID, and the GUID in specifics is not set.
TEST(BookmarkUpdatePreprocessingTest, ShouldUseOriginatorClientItemIdAsGuid) {
  const std::string kOriginatorClientItemId = base::GenerateGUID();

  sync_pb::SyncEntity entity;
  entity.set_originator_cache_guid(base::GenerateGUID());
  entity.set_originator_client_item_id(kOriginatorClientItemId);
  entity.mutable_specifics()->mutable_bookmark();

  base::HistogramTester histogram_tester;
  sync_pb::EntitySpecifics specifics = entity.specifics();
  EXPECT_TRUE(AdaptGuidForBookmark(entity, &specifics));

  EXPECT_THAT(specifics.bookmark().guid(), Eq(kOriginatorClientItemId));

  histogram_tester.ExpectUniqueSample("Sync.BookmarkGUIDSource2",
                                      /*sample=*/
                                      ExpectedBookmarkGuidSource::kValidOCII,
                                      /*count=*/1);
}

// Tests that AdaptGuidForBookmark() infers the GUID when the field in specifics
// is empty and originator client item ID is not a valid GUID.
TEST(BookmarkUpdatePreprocessingTest, ShouldInferGuid) {
  const std::string kOriginatorClientItemId = "1";

  sync_pb::SyncEntity entity;
  entity.set_originator_cache_guid(base::GenerateGUID());
  entity.set_originator_client_item_id(kOriginatorClientItemId);
  entity.mutable_specifics()->mutable_bookmark();

  base::HistogramTester histogram_tester;
  sync_pb::EntitySpecifics specifics = entity.specifics();
  EXPECT_TRUE(AdaptGuidForBookmark(entity, &specifics));

  EXPECT_TRUE(base::IsValidGUIDOutputString(specifics.bookmark().guid()));

  histogram_tester.ExpectUniqueSample("Sync.BookmarkGUIDSource2",
                                      /*sample=*/
                                      ExpectedBookmarkGuidSource::kInferred,
                                      /*count=*/1);
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

}  // namespace

}  // namespace syncer
