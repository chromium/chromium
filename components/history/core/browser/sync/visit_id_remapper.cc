// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/visit_id_remapper.h"

#include "components/history/core/browser/sync/history_backend_for_sync.h"
#include "components/sync/protocol/history_specifics.pb.h"

namespace history {

VisitIDRemapper::VisitIDRemapper(HistoryBackendForSync* history_backend)
    : history_backend_(history_backend) {
  DCHECK(history_backend_);
}

VisitIDRemapper::~VisitIDRemapper() = default;

void VisitIDRemapper::RegisterVisit(VisitID local_visit_id,
                                    const std::string& originator_cache_guid,
                                    VisitID originator_visit_id,
                                    VisitID originator_referring_visit_id,
                                    VisitID originator_opener_visit_id) {
  DCHECK_NE(local_visit_id, 0);
  DCHECK(!originator_cache_guid.empty());

  // If this visit came from an old client which didn't populate
  // `originator_visit_id`, then we can't remap. (In this case, the
  // `originator_referring|opener_visit_id`s should anyway be empty too.)
  if (originator_visit_id == 0) {
    return;
  }
  visits_by_originator_id_[originator_cache_guid][originator_visit_id] = {
      local_visit_id, originator_referring_visit_id,
      originator_opener_visit_id};
}

void VisitIDRemapper::RemapIDs() {
  // For each originator:
  for (const auto& [originator_cache_guid, originator_visit_id_to_visit] :
       visits_by_originator_id_) {
    // For each visit from this originator:
    for (const auto& [originator_visit_id, visit] :
         originator_visit_id_to_visit) {
      // Find the local referrer/opener IDs (if any).
      VisitID local_referrer_id = FindLocalVisitID(
          originator_cache_guid, visit.originator_referring_visit_id);
      VisitID local_opener_id = FindLocalVisitID(
          originator_cache_guid, visit.originator_opener_visit_id);
      // If either of the local IDs were found, write them to the DB.
      if (local_referrer_id != 0 || local_opener_id != 0) {
        history_backend_->UpdateVisitReferrerOpenerIDs(
            visit.local_visit_id, local_referrer_id, local_opener_id);
      }
      // TODO(crbug.com/1335055): Also do the remapping the other way around -
      // remap existing visits' originator_referring|opener_visit_ids if they
      // refer to the newly-added visit.
    }
  }
}

VisitID VisitIDRemapper::FindLocalVisitID(
    const std::string& originator_cache_guid,
    VisitID originator_visit_id) {
  if (originator_visit_id == 0) {
    return 0;
  }

  // Get all the in-memory visits for this originator.
  auto originator_it = visits_by_originator_id_.find(originator_cache_guid);
  DCHECK(originator_it != visits_by_originator_id_.end());

  std::map<VisitID, VisitInfo> originator_visits = originator_it->second;

  // Try to find the matching visit in the in-memory cache.
  auto it = originator_visits.find(originator_visit_id);
  if (it != originator_visits.end()) {
    // Found it!
    const VisitInfo& visit = it->second;
    return visit.local_visit_id;
  }

  // Didn't find it in the cache - try the DB instead.
  VisitRow row;
  if (history_backend_->GetForeignVisit(originator_cache_guid,
                                        originator_visit_id, &row)) {
    return row.visit_id;
  }

  // Didn't find it in the DB either.
  return 0;
}

}  // namespace history
