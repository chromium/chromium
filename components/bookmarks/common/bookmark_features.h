// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_COMMON_BOOKMARK_FEATURES_H_
#define COMPONENTS_BOOKMARKS_COMMON_BOOKMARK_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace bookmarks {

BASE_DECLARE_FEATURE(kAllBookmarksBaselineFolderVisibility);

// TODO(crbug.com/426243026): Complete MD5 -> SHA256 migration.
BASE_DECLARE_FEATURE(kEnableBookmarkCodecSHA256);

BASE_DECLARE_FEATURE(kEnableBookmarkNodeDataNewPickleFormat);

// Based on kEncryptBookmarks and kBookmarkEncryptionStageParam, this function
// returns true if encrypted bookmarks should be written to disk.
bool ShouldWriteEncryptedBookmarksToDisk();

// Based on kEncryptBookmarks and kBookmarkEncryptionStageParam, this function
// returns true if encrypted bookmarks data should be verified when loading
// unencrypted bookmarks data.
bool ShouldVerifyEncryptedBookmarksDataOnLoad();

// Flag to enable bookmark encryption. If false, no encryption will be performed
// on bookmarks. If true, usage of encryption will be determined by
// kBookmarkEncryptionStageParam.
// Do not use this feature directly, instead use helper functions defined above.
BASE_DECLARE_FEATURE(kEncryptBookmarks);

// Represents the stage of the bookmark encryption feature.
// Exposed publicly for testing purposes.
enum class BookmarkEncryptionStage {
  kDisabled,
  kWriteBothReadOnlyClear,
};

// Return the name of the given bookmark encryption stage that should be used to
// initialize kBookmarkEncryptionStageParam in tests.
std::string GetBookmarkEncryptionStageNameForTesting(
    BookmarkEncryptionStage stage);

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_COMMON_BOOKMARK_FEATURES_H_
