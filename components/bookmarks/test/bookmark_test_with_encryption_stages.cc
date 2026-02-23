// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/test/bookmark_test_with_encryption_stages.h"

#include "base/test/scoped_feature_list.h"
#include "components/bookmarks/common/bookmark_features.h"

namespace bookmarks::test {

void InitFeaturesForBookmarkTestEncryptionStage(
    base::test::ScopedFeatureList& features,
    BookmarkEncryptionStage stage) {
  if (stage != BookmarkEncryptionStage::kDisabled) {
    features.InitAndEnableFeatureWithParameters(
        kEncryptBookmarks,
        {{"stage", GetBookmarkEncryptionStageNameForTesting(stage)}});
  } else {
    features.InitAndDisableFeature(kEncryptBookmarks);
  }
}

}  // namespace bookmarks::test
