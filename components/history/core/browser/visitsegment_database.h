// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_VISITSEGMENT_DATABASE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_VISITSEGMENT_DATABASE_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "components/history/core/browser/history_types.h"

namespace sql {
class Database;
}

namespace history {

class PageUsageData;

// Tracks pages used for the most visited view.
class VisitSegmentDatabase {
 public:
  // Must call InitSegmentTables before using any other part of this class.
  VisitSegmentDatabase();

  VisitSegmentDatabase(const VisitSegmentDatabase&) = delete;
  VisitSegmentDatabase& operator=(const VisitSegmentDatabase&) = delete;

  virtual ~VisitSegmentDatabase();

  // Compute a segment name given a URL. The segment name is currently the
  // source url spec less some information such as query strings.
  static std::string ComputeSegmentName(const GURL& url);

  // Returns the ID of the segment with the corresponding name, or 0 if there
  // is no segment with that name.
  SegmentID GetSegmentNamed(const std::string& segment_name);

  // Update the segment identified by `out_segment_id` with the provided URL ID.
  // The URL identifies the page that will now represent the segment. If url_id
  // is non zero, it is assumed to be the row id of `url`.
  bool UpdateSegmentRepresentationURL(SegmentID segment_id,
                                      URLID url_id);

  // Create a segment for the provided URL ID with the given name. Returns the
  // ID of the newly created segment, or 0 on failure.
  SegmentID CreateSegment(URLID url_id, const std::string& segment_name);

  // Update the segment visit count by the provided amount. Return true on
  // success.
  bool UpdateSegmentVisitCount(SegmentID segment_id, base::Time ts, int amount);

  // Returns the highest-scored segments up to `max_result_count`. If
  // `url_filter` is non-null, then only URLs for which it returns true will be
  // included.
  std::vector<std::unique_ptr<PageUsageData>> QuerySegmentUsage(
      int max_result_count,
      const base::RepeatingCallback<bool(const GURL&)>& url_filter);

  // Deletes all segment data older than `older_than`.
  bool DeleteSegmentDataOlderThan(base::Time older_than);

  // Delete the segment currently using the provided url for representation.
  // This will also delete any associated segment usage data.
  bool DeleteSegmentForURL(URLID url_id);

 protected:
  // Returns the database for the functions in this interface.
  virtual sql::Database& GetDB() = 0;

  // Creates the tables used by this class if necessary. Returns true on
  // success.
  bool InitSegmentTables();

  // Deletes all the segment tables, returning true on success.
  bool DropSegmentTables();

  // Removes the 'pres_index' column from the segments table and the
  // presentation table is removed entirely.
  bool MigratePresentationIndex();

  // Runs ComputeSegmentName() to recompute 'name'. If multiple segments have
  // the same name, they are merged by:
  // 1. Choosing one arbitrary `segment_id` and updating all references.
  // 2. Merging duplicate `segment_usage` entries (add up visit counts).
  // 3. Deleting old data for the absorbed segment.
  bool MigrateVisitSegmentNames();

 private:
  // Updates the `name` column for a single segment. Returns true on success.
  bool RenameSegment(SegmentID segment_id, const std::string& new_name);
  // Merges two segments such that data is aggregated, all former references to
  // `from_segment_id` are updated to `to_segment_id` and `from_segment_id` is
  // deleted. Returns true on success.
  bool MergeSegments(SegmentID from_segment_id, SegmentID to_segment_id);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_VISITSEGMENT_DATABASE_H_
