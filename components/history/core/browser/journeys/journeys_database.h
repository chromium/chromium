// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEYS_DATABASE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEYS_DATABASE_H_

#include <optional>
#include <string>
#include <vector>

#include "components/history/core/browser/journeys/journey_row.h"

namespace sql {
class Database;
}  // namespace sql

namespace history::journeys {

// Manages the SQLite database tables for storing journey data.
//
// Tables:
// - `journeys`: Main table storing top-level journey metadata keyed by
//   `journey_id` (GUID).
// - `journey_history_entries`: Child table linking `journey_id` to visit
//   timestamps. Timestamps are stored as microseconds since Windows epoch
//   (matching `visits.visit_time` in VisitDatabase).
// - `journey_continuation_queries`: Child table linking `journey_id` to
//   continuation queries.
class JourneysDatabase {
 public:
  JourneysDatabase();

  JourneysDatabase(const JourneysDatabase&) = delete;
  JourneysDatabase& operator=(const JourneysDatabase&) = delete;

  virtual ~JourneysDatabase();

  // Adds or updates a batch of journeys in the database, including their
  // history entries and continuation queries. Returns true on success.
  bool AddOrUpdateJourneys(const std::vector<JourneyRow>& journeys);

  // Deletes a batch of journeys and their associated child entries (history
  // entries and continuation queries) identified by `journey_ids`. Returns true
  // on success.
  bool DeleteJourneys(const std::vector<std::string>& journey_ids);

  // Retrieves a full journey by its journey_id (including history entries and
  // continuation queries). Returns std::nullopt if not found or on error.
  std::optional<JourneyRow> GetJourney(const std::string& journey_id);

  // Retrieves all stored journeys, ordered by creation_time DESC.
  // Returns an empty vector on error or if none exist.
  std::vector<JourneyRow> GetAllJourneys();

  // Deletes all journeys and all associated child entries from all 3 tables.
  // Returns true on success.
  bool DeleteAllJourneys();

 protected:
  // Initializes the database tables and indices for a fresh database. Returns
  // true on success.
  bool InitJourneysTables();

  // Drops all journey tables. Used by HistoryDatabase::RecreateAllTablesButURL.
  bool DropJourneysTables();

  // Returns the database for the functions in this interface. The descendant
  // of this class implements this function to return its database.
  virtual sql::Database& GetDB() = 0;
};

}  // namespace history::journeys

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEYS_DATABASE_H_
