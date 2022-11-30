// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_SYNC_VISIT_ID_REMAPPER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_SYNC_VISIT_ID_REMAPPER_H_

#include <map>
#include <string>

#include "base/containers/flat_map.h"
#include "components/history/core/browser/history_types.h"

namespace history {

class HistoryBackendForSync;

// VisitIDs are only unique per-device. Therefore, every visit has two sets of
// IDs: The local ones and the "originator" ones (i.e. the ones assigned on the
// device where the visit originally came from). This means that the incoming
// referring_visit and opener_visit IDs need to be remapped into local IDs so
// that "links" in the visit database work correctly.
// VisitIDRemapper is responsible for this remapping. It collects all visits
// that arrived within one Sync update and populates their referring/opener IDs
// based on the corresponding originator IDs.
// For efficiency, it tries to perform the remapping in-memory first, since most
// commonly the referred-to visits should arrive together. Only if the in-memory
// lookup fails does it query the HistoryBackend.
// Note that originator and local IDs are stored in separate columns in the DB,
// so in case no matching local ID can be found, the corresponding DB column
// will simply remain empty (and might be populated by a later remapping).
class VisitIDRemapper {
 public:
  // `history_backend` must not be null, and must outlive this object.
  explicit VisitIDRemapper(HistoryBackendForSync* history_backend);
  ~VisitIDRemapper();

  // Registers a visit for remapping.
  void RegisterVisit(VisitID local_visit_id,
                     const std::string& originator_cache_guid,
                     VisitID originator_visit_id,
                     VisitID originator_referring_visit_id,
                     VisitID originator_opener_visit_id);

  // Performs the remapping of all visits previously registered, and writes the
  // results into the HistoryBackend.
  void RemapIDs();

 private:
  struct VisitInfo {
    VisitID local_visit_id;
    VisitID originator_referring_visit_id;
    VisitID originator_opener_visit_id;
  };

  VisitID FindLocalVisitID(
      const std::string& originator_cache_guid,
      const base::flat_map<VisitID, VisitInfo>& visits_by_originator_id,
      VisitID originator_visit_id);

  const raw_ptr<HistoryBackendForSync> history_backend_;

  // All of the visits with to-be-remapped IDs, indexed by originator cache
  // guid, and paired with their originator visit ID.
  std::map<std::string, std::vector<std::pair<VisitID, VisitInfo>>> visits_;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_SYNC_VISIT_ID_REMAPPER_H_
