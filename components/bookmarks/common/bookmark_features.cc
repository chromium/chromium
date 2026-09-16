// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/common/bookmark_features.h"

#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace bookmarks {

// This feature controls the default visibility for permanent folders when
// empty. It effectively swaps in "other bookmarks" as the default-visible
// empty folder on mobile. This flag has no effect for desktop.
BASE_FEATURE(kAllBookmarksBaselineFolderVisibility,
             base::FEATURE_DISABLED_BY_DEFAULT);

// This feature enables/disables using a SHA256 checksum alongside an
// md5 checksum. It is part of the MD5 deprecation efforts and is expected
// to be removed.
BASE_FEATURE(kEnableBookmarkCodecSHA256, base::FEATURE_DISABLED_BY_DEFAULT);

// This feature governs the data format employed by
// BookmarkNodeData::WriteToPickle. When set to enabled, it invokes
// Element::ToPickle to encode each Element as an integrated unit before writing
// it into the pickle. When set to false, it calls Element::WriteToPickle to
// write the fields into the pickle in sequential order.
BASE_FEATURE(kEnableBookmarkNodeDataNewPickleFormat,
             base::FEATURE_DISABLED_BY_DEFAULT);

// This feature enables encryption of bookmarks.
BASE_FEATURE(kEncryptBookmarks, base::FEATURE_ENABLED_BY_DEFAULT);
// Define the stage of the bookmark encryption feature. This param is ignored if
// kEncryptBookmarks is disabled.
// The possible values are:
// - write_both_read_only_clear: Bookmarks are written in both clear and
// encrypted files. Only the unencrypted file is read. This is used to evaluate
// the encryption process.
// - write_both_read_prefer_encrypted: Bookmarks are written in both clear and
// encrypted files. The encrypted file is used as the source of truth when
// loading bookmarks. We'll fall back to the unencrypted file if the encrypted
// file is missing.
// - write_only_encrypted_read_prefer_encrypted: Bookmarks are written in the
// encrypted file only. The encrypted file is used as the source of truth when
// loading bookmarks. We'll fall back to the unencrypted file if the encrypted
// file is missing. After a successful load from the encrypted file, the
// clear text file is deleted.
BASE_FEATURE_PARAM(std::string,
                   kBookmarkEncryptionStageParam,
                   &kEncryptBookmarks,
                   "stage",
                   "write_both_read_only_clear");

constexpr auto kBookmarkEncryptionStageMap =
    base::MakeFixedFlatMap<std::string_view, BookmarkEncryptionStage>(
        {{"write_both_read_only_clear",
          BookmarkEncryptionStage::kWriteBothReadOnlyClear},
         {"write_both_read_prefer_encrypted",
          BookmarkEncryptionStage::kWriteBothReadPreferEncrypted},
         {"write_only_encrypted_read_prefer_encrypted",
          BookmarkEncryptionStage::kWriteOnlyEncryptedReadPreferEncrypted}});

BookmarkEncryptionStage GetBookmarkEncryptionStage() {
  if (!base::FeatureList::IsEnabled(kEncryptBookmarks)) {
    return BookmarkEncryptionStage::kDisabled;
  }
  return kBookmarkEncryptionStageMap.at(kBookmarkEncryptionStageParam.Get());
}

bool ShouldWriteBookmarksToSecondaryFileOnDisk() {
  switch (GetBookmarkEncryptionStage()) {
    case BookmarkEncryptionStage::kWriteBothReadOnlyClear:
    case BookmarkEncryptionStage::kWriteBothReadPreferEncrypted:
      return true;
    case BookmarkEncryptionStage::kDisabled:
    case BookmarkEncryptionStage::kWriteOnlyEncryptedReadPreferEncrypted:
      return false;
  }
  NOTREACHED();
}

bool ShouldVerifyBookmarksDataInSecondaryFileOnLoad() {
  switch (GetBookmarkEncryptionStage()) {
    case BookmarkEncryptionStage::kWriteBothReadOnlyClear:
    case BookmarkEncryptionStage::kWriteBothReadPreferEncrypted:
      return true;
    case BookmarkEncryptionStage::kDisabled:
    case BookmarkEncryptionStage::kWriteOnlyEncryptedReadPreferEncrypted:
      return false;
  }
  NOTREACHED();
}

bool ShouldUseEncryptedBookmarksAsPrimarySource() {
  switch (GetBookmarkEncryptionStage()) {
    case BookmarkEncryptionStage::kWriteBothReadPreferEncrypted:
    case BookmarkEncryptionStage::kWriteOnlyEncryptedReadPreferEncrypted:
      return true;
    case BookmarkEncryptionStage::kWriteBothReadOnlyClear:
    case BookmarkEncryptionStage::kDisabled:
      return false;
  }
  NOTREACHED();
}

bool ShouldDeleteClearTextBookmarksFile() {
  switch (GetBookmarkEncryptionStage()) {
    case BookmarkEncryptionStage::kWriteOnlyEncryptedReadPreferEncrypted:
      return true;
    case BookmarkEncryptionStage::kWriteBothReadOnlyClear:
    case BookmarkEncryptionStage::kWriteBothReadPreferEncrypted:
    case BookmarkEncryptionStage::kDisabled:
      return false;
  }
  NOTREACHED();
}

std::string GetBookmarkEncryptionStageNameForTesting(  // IN-TEST
    BookmarkEncryptionStage stage) {
  for (const auto& pair : kBookmarkEncryptionStageMap) {
    if (pair.second == stage) {
      return std::string(pair.first);
    }
  }
  NOTREACHED();
}

BookmarkEncryptionStage GetCurrentBookmarkEncryptionStageForTesting() {
  return GetBookmarkEncryptionStage();
}

}  // namespace bookmarks
