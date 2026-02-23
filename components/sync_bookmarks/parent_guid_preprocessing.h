// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_PARENT_GUID_PREPROCESSING_H_
#define COMPONENTS_SYNC_BOOKMARKS_PARENT_GUID_PREPROCESSING_H_

#include <string>

#include "components/sync/engine/commit_and_get_updates_types.h"

namespace sync_bookmarks {

class SyncedBookmarkTracker;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SyncBookmarkParentGuidSource)
enum class ParentGuidSource {
  kMissing = 0,
  kFallbackUnresolvable = 1,
  kFoundInSpecifics = 2,
  kFallbackFoundInTracker = 3,
  kFallbackFoundInUpdates = 4,
  kMaxValue = kFallbackFoundInUpdates,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:SyncBookmarkParentGuidSource)

// Clients before M94 did not populate the parent GUID in specifics
// (|BookmarkSpecifics.parent_guid|, so this function tries to populate the
// missing values in |updates| such that it resembles how modern clients would
// populate specifics (including |parent_guid|). To do so, it leverages the
// information in |updates| itself (if the parent is included) and, if |tracker|
// is non-null, the information available in tracked entities. |updates| must
// not be null. |tracker| may be null,
void PopulateParentGuidInSpecifics(const SyncedBookmarkTracker* tracker,
                                   syncer::UpdateResponseDataList* updates);

std::string GetGuidForSyncIdInUpdatesForTesting(
    const syncer::UpdateResponseDataList& updates,
    const std::string& sync_id);

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_PARENT_GUID_PREPROCESSING_H_
