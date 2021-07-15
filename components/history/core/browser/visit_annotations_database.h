// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_VISIT_ANNOTATIONS_DATABASE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_VISIT_ANNOTATIONS_DATABASE_H_

#include <vector>

#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"

namespace sql {
class Database;
}  // namespace sql

namespace history {

struct VisitContentAnnotations;

// A database that stores visit content & context annotations. A
// `VisitAnnotationsDatabase` must also be a `VisitDatabase`, as this joins with
// the `visits` table. The `content_annotations` and `context_annotations` use
// `visit_id` as their primary key; each row in the `visits` table will be
// associated with 0 or 1 rows in each annotation table.
class VisitAnnotationsDatabase {
 public:
  // Must call `InitAnnotationsTables()` before using any other part of this
  // class.
  VisitAnnotationsDatabase();
  VisitAnnotationsDatabase(const VisitAnnotationsDatabase&) = delete;
  VisitAnnotationsDatabase& operator=(const VisitAnnotationsDatabase&) = delete;
  virtual ~VisitAnnotationsDatabase();

  // Adds a line to the content annotations table with the given information.
  // Ignores failures.
  void AddContentAnnotationsForVisit(
      VisitID visit_id,
      const VisitContentAnnotations& visit_content_annotations);

  // Adds a line to the context annotation table with the given information.
  // Ignores failures.
  void AddContextAnnotationsForVisit(
      VisitID visit_id,
      const VisitContextAnnotations& visit_context_annotations);

  // Updates an existing row. The new information is set on the row, using the
  // VisitID as the key. The content annotations for the visit must exist.
  // Ignores failures.
  void UpdateContentAnnotationsForVisit(
      VisitID visit_id,
      const VisitContentAnnotations& visit_content_annotations);

  // Query for a `VisitContentAnnotations` given `visit_id`. If it's found and
  // valid, this method returns true, and `out_content_annotations` is filled.
  // Otherwise, this returns false, and `out_content_annotations` is unchanged.
  bool GetContentAnnotationsForVisit(
      VisitID visit_id,
      VisitContentAnnotations* out_content_annotations);

  // Query for a `VisitContextAnnotations` given `visit_id`. If it's found and
  // valid, this method returns true, and `out_context_annotations` is filled.
  // Otherwise, this returns false, and `out_context_annotations` is unchanged.
  bool GetContextAnnotationsForVisit(
      VisitID visit_id,
      VisitContextAnnotations* out_context_annotations);

  // Get recent `AnnotatedVisit`s' IDs. Does not return visits without
  // annotations.
  std::vector<VisitID> GetRecentAnnotatedVisitIds(base::Time minimum_time,
                                                  int max_results);

  // Get all `AnnotatedVisitRow`s except unclustered visits. Does not return
  // duplicates if a visit is in multiple `Cluster`s.
  std::vector<AnnotatedVisitRow> GetClusteredAnnotatedVisits(int max_results);

  // Gets all the context annotation rows for testing.
  std::vector<AnnotatedVisitRow> GetAllContextAnnotationsForTesting();

  // Deletes the content & context annotations associated with `visit_id`. This
  // will also delete any associated annotations usage data. If no annotations
  // exist for the `VisitId`, this is a no-op. Ignores failures; i.e. continues
  // trying to delete from each remaining table.
  void DeleteAnnotationsForVisit(VisitID visit_id);

  // Add `clusters` to the tables. Ignores failures; i.e. continues trying to
  // add the remaining `Cluster`s. Does not try to add `clusters_and_visits`
  // entries for any `Cluster` that it failed to add.
  void AddClusters(const std::vector<Cluster>& clusters);

  // Get the `max_results` most recent `ClusterRow`s.
  std::vector<ClusterRow> GetClusters(int max_results);

  // Get recent `Cluster`s' IDs newer than `minimum_time`.
  std::vector<int64_t> GetRecentClusterIds(base::Time minimum_time);

  // Get the `max_results` newest `VisitID`s in a cluster.
  std::vector<VisitID> GetVisitIdsInCluster(int64_t cluster_id,
                                            int max_results);

 protected:
  // Returns the database for the functions in this interface.
  virtual sql::Database& GetDB() = 0;

  // Creates the tables used by this class if necessary. Returns true on
  // success.
  bool InitVisitAnnotationsTables();

  // Deletes all the annotations tables, returning true on success.
  bool DropVisitAnnotationsTables();

  // Called by the derived classes to migrate the older visits table's
  // floc_allowed (for historical reasons named "publicly_routable" in the
  // schema) column to the content_annotations table, from a BOOLEAN filed to
  // a bit masking INTEGER filed.
  bool MigrateFlocAllowedToAnnotationsTable();

  // Replaces `cluster_visits` with `context_annotations`. Besides the name
  // change, the new table drops 2 columns: cluster_visit_id (obsolete) and
  // url_id (redundant); and renames 1 column:
  // cluster_visit_context_signal_bitmask to context_annotation_flags.
  bool MigrateReplaceClusterVisitsTable();
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_VISIT_ANNOTATIONS_DATABASE_H_
