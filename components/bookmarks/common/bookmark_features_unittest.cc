// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/common/bookmark_features.h"

#include "base/test/scoped_feature_list.h"
#include "components/bookmarks/test/bookmark_test_with_encryption_stages.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bookmarks {
namespace {

class BookmarkFeaturesWithEncryptionTest
    : public testing::TestWithParam<BookmarkEncryptionStage> {
 protected:
  BookmarkFeaturesWithEncryptionTest() {
    test::InitFeaturesForBookmarkTestEncryptionStage(feature_list_, GetParam());
  }

  bool IsWriteBothReadOnlyClearStage() {
    return GetParam() == BookmarkEncryptionStage::kWriteBothReadOnlyClear;
  }

  bool IsWriteBothReadPreferEncryptedStage() {
    return GetParam() == BookmarkEncryptionStage::kWriteBothReadPreferEncrypted;
  }

  bool IsWriteOnlyEncryptedReadPreferEncryptedStage() {
    return GetParam() ==
           BookmarkEncryptionStage::kWriteOnlyEncryptedReadPreferEncrypted;
  }

  base::test::ScopedFeatureList feature_list_;
};

TEST_P(BookmarkFeaturesWithEncryptionTest,
       ShouldWriteBookmarksToSecondaryFileOnDisk) {
  EXPECT_EQ(
      ShouldWriteBookmarksToSecondaryFileOnDisk(),
      IsWriteBothReadOnlyClearStage() || IsWriteBothReadPreferEncryptedStage());
}

TEST_P(BookmarkFeaturesWithEncryptionTest,
       ShouldVerifyBookmarksDataInSecondaryFileOnLoad) {
  EXPECT_EQ(
      ShouldVerifyBookmarksDataInSecondaryFileOnLoad(),
      IsWriteBothReadOnlyClearStage() || IsWriteBothReadPreferEncryptedStage());
}

TEST_P(BookmarkFeaturesWithEncryptionTest,
       ShouldUseEncryptedBookmarksAsPrimarySource) {
  EXPECT_EQ(ShouldUseEncryptedBookmarksAsPrimarySource(),
            IsWriteBothReadPreferEncryptedStage() ||
                IsWriteOnlyEncryptedReadPreferEncryptedStage());
}

INSTANTIATE_TEST_SUITE_P(
    BookmarkFeaturesWithEncryptionTest,
    BookmarkFeaturesWithEncryptionTest,
    ::testing::ValuesIn(test::kAllBookmarkEncryptionStages));

}  // namespace
}  // namespace bookmarks
