// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_VISIT_ANNOTATIONS_DATABASE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_VISIT_ANNOTATIONS_DATABASE_H_

#include <string_view>
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

  // Updates an existing row. The new information is set on the row, using the
  // VisitID as the key. The context annotations for the visit must exist.
  // Ignores failures.
  void UpdateContextAnnotationsForVisit(
      VisitID visit_id,
      const VisitContextAnnotations& visit_context_annotations);

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

  // Deletes the content & context annotations associated with `visit_id`. This
  // will also delete any associated annotations usage data. If no annotations
  // exist for the `VisitId`, this is a no-op. Ignores failures; i.e. continues
  // trying to delete from each remaining table.
  void DeleteAnnotationsForVisit(VisitID visit_id);

  // Add `clusters` to the tables. Ignores failures; i.e. continues trying to
  // add the remaining `Cluster`s. Does not try to add `clusters_and_visits`
  // entries for any `Cluster` that it failed to add.
  void AddClusters(const std::vector<Cluster>& clusters);

  // Adds a cluster with no visits with `originator_cache_guid` and
  // `originator_cluster_id` and returns the new cluster's ID.
  // `originator_cache_guid` and `originator_cluster_id` can be the respective
  // empty states if the cluster is a local cluster or the originator device
  // does not support those fields yet.
  int64_t ReserveNextClusterId(const std::string& originator_cache_guid,
                               int64_t originator_cluster_id);

  // Adds visits to the cluster with id `cluster_id`.
  void AddVisitsToCluster(int64_t cluster_id,
                          const std::vector<ClusterVisit>& visits);

  // Updates the triggerability attributes for each cluster in `clusters`.
  void UpdateClusterTriggerability(
      const std::vector<history::Cluster>& clusters);

  // Updates the cluster visit with the same visit ID as `cluster_visit` that
  // belongs to `cluster_id`.
  void UpdateClusterVisit(int64_t cluster_id,
                          const history::ClusterVisit& cluster_visit);

  // Get a `Cluster`. Does not include the cluster's `visits` or
  // `keyword_to_data_map`.
  Cluster GetCluster(int64_t cluster_id);

  // Get the most recent clusters within the constraints. The most recent visit
  // of a cluster represents the cluster's time.
  std::vector<int64_t> GetMostRecentClusterIds(base::Time inclusive_min_time,
                                               base::Time exclusive_max_time,
                                               int max_clusters);

  // Get `VisitID`s in a cluster.
  std::vector<VisitID> GetVisitIdsInCluster(int64_t cluster_id);

  // Get a `ClusterVisit`.
  ClusterVisit GetClusterVisit(VisitID visit_id);

  // Get `VisitID`s for duplicate cluster visits.
  std::vector<VisitID> GetDuplicateClusterVisitIdsForClusterVisit(
      int64_t visit_id);

  // Return the ID of the cluster containing `visit_id`. Returns 0 if `visit_id`
  // is not in a cluster.`
  int64_t GetClusterIdContainingVisit(VisitID visit_id);

  // Return the ID of the cluster that has `originator_cache_guid` and
  // `originator_cluster_id`. Returns 0 if a cluster does not have those
  // details.
  int64_t GetClusterIdForSyncedDetails(const std::string& originator_cache_guid,
                                       int64_t originator_cluster_id);

  // Return the keyword data associated with `cluster_id`.
  base::flat_map<std::u16string, ClusterKeywordData> GetClusterKeywords(
      int64_t cluster_id);

  // Sets scores of cluster visits to 0 to hide them from the webUI.
  void HideVisits(const std::vector<VisitID>& visit_ids);

  // Delete `Cluster`s from the table.
  void DeleteClusters(const std::vector<int64_t>& cluster_ids);

  // Update the interaction state of cluster visits.
  void UpdateVisitsInteractionState(
      const std::vector<VisitID>& visit_ids,
      ClusterVisit::InteractionState interaction_state);

  // Converts categories to something that can be stored in the database eg:
  // "mid1:score1,mid2:score2". As the serialized format is already being
  // synced, the implementation of these functions should not be changed.
  static std::string ConvertCategoriesToStringColumn(
      const std::vector<VisitContentModelAnnotations::Category>& categories);

  // Converts serialized categories into a vector of (`id`, `weight`) pairs. As
  // the serialized format is already being synced, the implementation of these
  // functions should not be changed.
  static std::vector<VisitContentModelAnnotations::Category>
  GetCategoriesFromStringColumn(std::string_view column_value);

  // Serializes a vector of strings into a string separated by null character
  // that can be stored in the db. As the serialized format is already being
  // synced, the implementation of these functions should not be changed.
  static std::string SerializeToStringColumn(
      const std::vector<std::string>& related_searches);

  // Converts a serialized db string separated by null character into a vector
  // of strings. As the serialized format is already being synced, the
  // implementation of these functions should not be changed.
  static std::vector<std::string> DeserializeFromStringColumn(
      std::string_view column_value);

 protected:
  // Returns the database for the functions in this interface.
  virtual sql::Database& GetDB() = 0;

  // Creates the tables used by this class if necessary. Returns true on
  // success.
  bool InitVisitAnnotationsTables();

  // Deletes all the annotations tables, returning true on success.
  bool DropVisitAnnotationsTables();

  // Called by the derived class to migrate the older clusters_and_visits table
  // by adding the interaction_state column.
  bool MigrateClustersAndVisitsAddInteractionState();

 private:
  // Return true if the clusters table's schema contains "AUTOINCREMENT".
  // false if table do not contain AUTOINCREMENT, or the table is not created.
  bool ClustersTableContainsAutoincrement();

  // Helper to create the 'clusters' table and avoid duplicating the code.
  bool CreateClustersTable();

  // Helper to create the 'clusters_and_visits' table and avoid duplicating the
  // code.
  bool CreateClustersAndVisitsTableAndIndex();
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_VISIT_ANNOTATIONS_DATABASE_H_
