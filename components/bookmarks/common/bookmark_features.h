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
// returns true if bookmarks should be written to a secondary file on disk.
// If ShouldUseEncryptedBookmarksAsPrimarySource returns false, the encrypted
// bookmarks file will be written to this secondary file, otherwise the
// unencrypted bookmarks file will.
bool ShouldWriteBookmarksToSecondaryFileOnDisk();

// Based on kEncryptBookmarks and kBookmarkEncryptionStageParam, this function
// returns true if bookmarks data in the secondary file should be verified when
// loading bookmarks data.
// If ShouldUseEncryptedBookmarksAsPrimarySource returns false, data in
// secondary file are encrypted and needs to be verified against the unencrypted
// data one.
// If ShouldUseEncryptedBookmarksAsPrimarySource returns true, data in
// secondary file are unencrypted and needs to be verified against the encrypted
// data one.
bool ShouldVerifyBookmarksDataInSecondaryFileOnLoad();

// Based on kEncryptBookmarks and kBookmarkEncryptionStageParam, this function
// returns true if encrypted bookmarks use as the primary source of truth.
// When true, encrypted bookmarks should be read / written first.
bool ShouldUseEncryptedBookmarksAsPrimarySource();

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
  kWriteBothReadPreferEncrypted,
};

// Return the name of the given bookmark encryption stage that should be used to
// initialize kBookmarkEncryptionStageParam in tests.
std::string GetBookmarkEncryptionStageNameForTesting(
    BookmarkEncryptionStage stage);

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_COMMON_BOOKMARK_FEATURES_H_
