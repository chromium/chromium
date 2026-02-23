// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_TEST_BOOKMARK_TEST_WITH_ENCRYPTION_STAGES_H_
#define COMPONENTS_BOOKMARKS_TEST_BOOKMARK_TEST_WITH_ENCRYPTION_STAGES_H_

#include <string>

#include "base/test/scoped_feature_list.h"
#include "components/bookmarks/common/bookmark_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bookmarks::test {

inline constexpr std::array<BookmarkEncryptionStage, 3>
    kAllBookmarkEncryptionStages = {
        BookmarkEncryptionStage::kDisabled,
        BookmarkEncryptionStage::kWriteBothReadOnlyClear};

void InitFeaturesForBookmarkTestEncryptionStage(
    base::test::ScopedFeatureList& features,
    BookmarkEncryptionStage stage);

}  // namespace bookmarks::test

#endif  // COMPONENTS_BOOKMARKS_TEST_BOOKMARK_TEST_WITH_ENCRYPTION_STAGES_H_
