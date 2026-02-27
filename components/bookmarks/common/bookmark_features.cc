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
BASE_FEATURE(kEncryptBookmarks, base::FEATURE_DISABLED_BY_DEFAULT);
// Define the stage of the bookmark encryption feature. This param is ignored if
// kEncryptBookmarks is disabled.
// The possible values are:
// - write_both_read_only_clear: Bookmarks are written in both clear and
// encrypted files. Only the unencrypted file is read. This is used to evaluate
// the encryption process.
// Do not use this feature directly, instead use helper functions defined above.
// TODO(crbug.com/435317726): Support other stages as we implement them:
// -write both, read encrypted,
// -write & read encrypted only; fallback to unencrypted file if needed,
// -write & read encrypted only; unencrypted file is deleted.
const base::FeatureParam<std::string> kBookmarkEncryptionStageParam{
    &kEncryptBookmarks, "stage", "write_both_read_only_clear"};

constexpr auto kBookmarkEncryptionStageMap =
    base::MakeFixedFlatMap<std::string_view, BookmarkEncryptionStage>(
        {{"write_both_read_only_clear",
          BookmarkEncryptionStage::kWriteBothReadOnlyClear}});

BookmarkEncryptionStage GetBookmarkEncryptionStage() {
  if (!base::FeatureList::IsEnabled(kEncryptBookmarks)) {
    return BookmarkEncryptionStage::kDisabled;
  }
  return kBookmarkEncryptionStageMap.at(kBookmarkEncryptionStageParam.Get());
}

bool ShouldWriteEncryptedBookmarksToDisk() {
  switch (GetBookmarkEncryptionStage()) {
    case BookmarkEncryptionStage::kWriteBothReadOnlyClear:
      return true;
    case BookmarkEncryptionStage::kDisabled:
      return false;
  }
  NOTREACHED();
}

bool ShouldVerifyEncryptedBookmarksDataOnLoad() {
  switch (GetBookmarkEncryptionStage()) {
    case BookmarkEncryptionStage::kWriteBothReadOnlyClear:
      return true;
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

}  // namespace bookmarks
