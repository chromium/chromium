// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEYS_DATABASE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEYS_DATABASE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/history/core/browser/journeys/journey_row.h"
#include "url/gurl.h"

namespace sql {
class Database;
}  // namespace sql

namespace history::journeys {

// The result of looking up one journey history entry in the `visits` and
// `urls` tables.
struct JourneyHistoryEntryLookup {
  JourneyHistoryEntryLookup();
  ~JourneyHistoryEntryLookup();
  JourneyHistoryEntryLookup(const JourneyHistoryEntryLookup&);
  JourneyHistoryEntryLookup& operator=(const JourneyHistoryEntryLookup&);
  JourneyHistoryEntryLookup(JourneyHistoryEntryLookup&&) noexcept;
  JourneyHistoryEntryLookup& operator=(JourneyHistoryEntryLookup&&) noexcept;

  enum class Status {
    // The visit and its URL row were found.
    kFound,
    // No visit with `visit_time` exists in the `visits` table.
    kMissingVisit,
    // The visit exists, but its URL row is missing from the `urls` table or
    // holds an invalid URL.
    kMissingUrl,
  };

  // The entry, from `journey_history_entries`.
  std::string journey_id;
  base::Time visit_time;

  Status status = Status::kMissingVisit;

  // From the visit row. Only meaningful if `status` isn't `kMissingVisit`.
  // True when the visit originated on another device and arrived via sync.
  bool is_foreign = false;

  // From the URL row. Only set if `status` is `kFound`.
  GURL url;
  std::u16string title;
};

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
// - `journey_collections`: Child table storing the titled collections of a
//   journey, in display order.
// - `journey_collection_items`: Child table storing the items of each
//   collection as visit timestamps, in display order.
class JourneysDatabase {
 public:
  JourneysDatabase();

  JourneysDatabase(const JourneysDatabase&) = delete;
  JourneysDatabase& operator=(const JourneysDatabase&) = delete;

  virtual ~JourneysDatabase();

  // Adds or updates a batch of journeys in the database, including their
  // history entries, continuation queries and collections. A visit repeated
  // within a collection is stored once, at its first position; collection
  // items aren't checked against `history_entries`. Returns true on success.
  bool AddOrUpdateJourneys(const std::vector<JourneyRow>& journeys);

  // Deletes a batch of journeys and their associated child entries (history
  // entries, continuation queries and collections) identified by
  // `journey_ids`. Returns true on success.
  bool DeleteJourneys(const std::vector<std::string>& journey_ids);

  // Retrieves a full journey by its journey_id (including history entries,
  // continuation queries and collections). Returns std::nullopt if not found or
  // on error.
  std::optional<JourneyRow> GetJourney(const std::string& journey_id);

  // Retrieves all stored journeys, ordered by creation_time DESC.
  // Returns an empty vector on error or if none exist.
  std::vector<JourneyRow> GetAllJourneys();

  // Deletes all journeys and all associated child entries from all journeys
  // tables listed above.
  // Returns true on success.
  bool DeleteAllJourneys();

  // Looks up the history entries of the journey identified by `journey_id` in
  // the `visits` and `urls` tables using a single query. If several visits
  // share an entry's timestamp (e.g. a redirect chain), the one with the
  // largest ID is used, which is the end of the chain. The result is ordered by
  // `visit_time`. Returns std::nullopt on error.
  //
  // Requires the `visits` and `urls` tables to exist in the same database,
  // which is the case when mixed into `HistoryDatabase`.
  std::optional<std::vector<JourneyHistoryEntryLookup>>
  LookUpJourneyHistoryEntries(const std::string& journey_id);

  // Same as `LookUpJourneyHistoryEntries()`, but for all journeys at once. The
  // result is ordered by `journey_id`, then `visit_time`. Returns std::nullopt
  // on error.
  std::optional<std::vector<JourneyHistoryEntryLookup>>
  LookUpAllJourneyHistoryEntries();

 protected:
  // Creates any missing tables and indices; existing ones are left untouched.
  // So adding a table needs no migration, but changing an existing one does.
  // Returns true on success.
  bool InitJourneysTables();

  // Drops all journey tables. Used by HistoryDatabase::RecreateAllTablesButURL.
  bool DropJourneysTables();

  // Returns the database for the functions in this interface. The descendant
  // of this class implements this function to return its database.
  virtual sql::Database& GetDB() = 0;

 private:
  // Deletes the rows of `journey_id` from all child tables, i.e. every table
  // except `journeys`. Attempts all deletions even if one fails, and returns
  // true if all succeeded.
  //
  // When adding a child table, update `DeleteChildRows()`,
  // `DeleteAllJourneys()` and `DropJourneysTables()`; the schema-enumerating
  // tests in journeys_database_unittest.cc fail otherwise.
  bool DeleteChildRows(const std::string& journey_id);

  // Inserts the rows of `journey` into all child tables. Returns false on the
  // first failure.
  bool InsertChildRows(const JourneyRow& journey);
};

}  // namespace history::journeys

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEYS_DATABASE_H_
