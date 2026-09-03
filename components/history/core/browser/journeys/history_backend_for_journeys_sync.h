// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_HISTORY_BACKEND_FOR_JOURNEYS_SYNC_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_HISTORY_BACKEND_FOR_JOURNEYS_SYNC_H_

#include <string>
#include <vector>

#include "components/history/core/browser/journeys/journey_row.h"

namespace history {
class HistoryBackendObserver;
}  // namespace history

namespace history::journeys {

// Interface defining the subset of HistoryBackend required by
// JourneysSyncBridge. This is a separate interface mainly for ease of testing.
class HistoryBackendForJourneysSync {
 public:
  virtual ~HistoryBackendForJourneysSync() = default;

  // Observers -----------------------------------------------------------------

  virtual void AddObserver(HistoryBackendObserver* observer) = 0;
  virtual void RemoveObserver(HistoryBackendObserver* observer) = 0;

  // Persists or updates the given `journeys` in the local database.
  // Returns true on success, or false on database failure.
  virtual bool AddOrUpdateJourneys(const std::vector<JourneyRow>& journeys) = 0;

  // Deletes journeys matching the given `journey_ids` from the local database.
  // Returns true on success, or false on database failure.
  virtual bool DeleteJourneys(const std::vector<std::string>& journey_ids) = 0;

  // Retrieves all persisted journeys from the local database.
  virtual std::vector<JourneyRow> GetAllJourneys() = 0;

  // Deletes all journeys from the local database (called when sync is stopped
  // and local sync data is cleared). Returns true on success, or false on
  // database failure.
  virtual bool DeleteAllJourneys() = 0;
};

}  // namespace history::journeys

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_HISTORY_BACKEND_FOR_JOURNEYS_SYNC_H_
