// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_CLUSTER_VISIT_DATABASE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_CLUSTER_VISIT_DATABASE_H_

#include <vector>

#include "components/history/core/browser/history_types.h"

namespace sql {
class Database;
}  // namespace sql

namespace history {

// A database that stores cluster visits; i.e. the visit, corresponding URL, and
// context signals for the visit. A cluster visit database must also be a visit
// databases, as this joins with the visit table and could be thought of as
// inheriting from VisitDatabase. However, this inheritance is not explicit as
// things would get too complicated and have multiple inheritance.
// TODO(manukh) Merge with VisitAnnotationsDatabase.
class ClusterVisitDatabase {
 public:
  // Must call InitClusterVisitTable() before using to make sure the database is
  // initialized.
  ClusterVisitDatabase();
  ClusterVisitDatabase(const ClusterVisitDatabase&) = delete;
  ClusterVisitDatabase& operator=(const ClusterVisitDatabase&) = delete;
  virtual ~ClusterVisitDatabase();

  // Deletes the table. Returns true on success.
  bool DropClusterVisitTable();

  // Add `row` to the table.
  void AddAnnotatedVisit(const AnnotatedVisitRow& row);

  // Delete a `AnnotatedVisitRow` from the table.
  void DeleteAnnotatedVisit(VisitID visit_id);

  // Get the `max_results` most recent `AnnotatedVisitRow`s.
  std::vector<AnnotatedVisitRow> GetAnnotatedVisits(int max_results);

 protected:
  // Returns the database for the functions in this interface.
  virtual sql::Database& GetDB() = 0;

  // Called by the derived classes on initialization to make sure the tables
  // and indices are properly set up. Must be called before anything else.
  bool InitClusterVisitTable();

  // Replaces `cluster_visits` with `context_annotations`. Besides the name
  // change, the new table drops 2 columns: cluster_visit_id (obsolete) and
  // url_id (redundant); and renames 1 column:
  // cluster_visit_context_signal_bitmask to context_annotation_flags.
  bool MigrateReplaceClusterVisitsTable();
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_CLUSTER_VISIT_DATABASE_H_
