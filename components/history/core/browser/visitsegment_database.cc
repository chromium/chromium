// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/visitsegment_database.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "components/history/core/browser/page_usage_data.h"
#include "components/history/core/browser/segment_scorer.h"
#include "sql/statement.h"
#include "sql/transaction.h"

// The following tables are used to store url segment information.
//
// segments
//   id                 Primary key
//   name               A unique string to represent that segment. (URL derived)
//   url_id             ID of the url currently used to represent this segment.
//
// segment_usage
//   id                 Primary key
//   segment_id         Corresponding segment id
//   time_slot          time stamp identifying for what day this entry is about
//   visit_count        Number of visit in the segment
//

namespace history {

namespace {

constexpr SegmentID kEmptySegmentID = 0;

struct SegmentInfo {
  SegmentID segment_id;
  std::vector<base::Time> time_slots;
  std::vector<int> visit_counts;
};

// Visits segment_usage entries in the history database, grouped by segment ID
// and ordered by increasing segment ID.
class SegmentVisitor {
 public:
  // |statement| selects (segment_id, time_slot, visit_count) from segment_usage
  // table, ordered by segment_id.
  explicit SegmentVisitor(raw_ptr<sql::Statement> statement)
      : statement_(statement) {
    cur_segment_id_ = (statement_->is_valid() && statement_->Step())
                          ? statement_->ColumnInt64(0)
                          : kEmptySegmentID;
  }

  ~SegmentVisitor() = default;

  // Reads the next batch of segment_usage entries with a common segment ID, and
  // writes the result to |*segment_info|. Returns whether the returned entry is
  // valid. If false, clears |*segment_info|.
  bool Step(SegmentInfo* segment_info) {
    segment_info->segment_id = cur_segment_id_;
    segment_info->time_slots.clear();
    segment_info->visit_counts.clear();

    if (cur_segment_id_ == kEmptySegmentID) {
      return false;
    }

    SegmentID next_segment_id = kEmptySegmentID;
    do {
      segment_info->time_slots.push_back(statement_->ColumnTime(1));
      segment_info->visit_counts.push_back(statement_->ColumnInt(2));
      next_segment_id =
          statement_->Step() ? statement_->ColumnInt64(0) : kEmptySegmentID;
    } while (next_segment_id == cur_segment_id_);

    cur_segment_id_ = next_segment_id;
    return true;
  }

 private:
  raw_ptr<sql::Statement> statement_;

  // Look-ahead SegmentID of the segment to be retrieved for the next Step()
  // call. Indicates end of data if value is |kEmptySegmentID|.
  SegmentID cur_segment_id_;
};

}  // namespace

VisitSegmentDatabase::VisitSegmentDatabase() = default;

VisitSegmentDatabase::~VisitSegmentDatabase() = default;

bool VisitSegmentDatabase::InitSegmentTables() {
  // Segments table.
  if (!GetDB().DoesTableExist("segments")) {
    if (!GetDB().Execute("CREATE TABLE segments ("
        "id INTEGER PRIMARY KEY,"
        "name VARCHAR,"
        "url_id INTEGER NON NULL)")) {
      return false;
    }

    if (!GetDB().Execute(
        "CREATE INDEX segments_name ON segments(name)")) {
      return false;
    }
  }

  // This was added later, so we need to try to create it even if the table
  // already exists.
  if (!GetDB().Execute("CREATE INDEX IF NOT EXISTS segments_url_id ON "
                       "segments(url_id)"))
    return false;

  // Segment usage table.
  if (!GetDB().DoesTableExist("segment_usage")) {
    if (!GetDB().Execute("CREATE TABLE segment_usage ("
        "id INTEGER PRIMARY KEY,"
        "segment_id INTEGER NOT NULL,"
        "time_slot INTEGER NOT NULL,"
        "visit_count INTEGER DEFAULT 0 NOT NULL)")) {
      return false;
    }
    if (!GetDB().Execute(
        "CREATE INDEX segment_usage_time_slot_segment_id ON "
        "segment_usage(time_slot, segment_id)")) {
      return false;
    }
  }

  // Added in a later version, so we always need to try to creat this index.
  if (!GetDB().Execute("CREATE INDEX IF NOT EXISTS segments_usage_seg_id "
                       "ON segment_usage(segment_id)"))
    return false;

  return true;
}

bool VisitSegmentDatabase::DropSegmentTables() {
  // Dropping the tables will implicitly delete the indices.
  return GetDB().Execute("DROP TABLE segments") &&
         GetDB().Execute("DROP TABLE segment_usage");
}

// Note: the segment name is derived from the URL but is not a URL. It is
// a string that can be easily recreated from various URLS. Maybe this should
// be an MD5 to limit the length.
//
// static
std::string VisitSegmentDatabase::ComputeSegmentName(const GURL& url) {
  // TODO(brettw) this should probably use the registry controlled
  // domains service.
  GURL::Replacements r;
  std::string_view host = url.host_piece();

  // Strip various common prefixes in order to group the resulting hostnames
  // together and avoid duplicates.
  for (std::string_view prefix : {"www.", "m.", "mobile.", "touch."}) {
    if (host.size() > prefix.size() &&
        base::StartsWith(host, prefix, base::CompareCase::INSENSITIVE_ASCII)) {
      r.SetHostStr(host.substr(prefix.size()));
      break;
    }
  }

  // Remove other stuff we don't want.
  r.ClearUsername();
  r.ClearPassword();
  r.ClearQuery();
  r.ClearRef();
  r.ClearPort();

  // Canonicalize https to http in order to avoid duplicates.
  if (url.SchemeIs(url::kHttpsScheme))
    r.SetSchemeStr(url::kHttpScheme);

  return url.ReplaceComponents(r).spec();
}

SegmentID VisitSegmentDatabase::GetSegmentNamed(
    const std::string& segment_name) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT id FROM segments WHERE name = ?"));
  statement.BindString(0, segment_name);

  if (statement.Step())
    return statement.ColumnInt64(0);
  return 0;
}

bool VisitSegmentDatabase::UpdateSegmentRepresentationURL(SegmentID segment_id,
                                                          URLID url_id) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE segments SET url_id = ? WHERE id = ?"));
  statement.BindInt64(0, url_id);
  statement.BindInt64(1, segment_id);

  return statement.Run();
}

SegmentID VisitSegmentDatabase::CreateSegment(URLID url_id,
                                              const std::string& segment_name) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO segments (name, url_id) VALUES (?,?)"));
  statement.BindString(0, segment_name);
  statement.BindInt64(1, url_id);

  if (statement.Run())
    return GetDB().GetLastInsertRowId();
  return 0;
}

bool VisitSegmentDatabase::UpdateSegmentVisitCount(SegmentID segment_id,
                                                   base::Time ts,
                                                   int amount) {
  base::Time t = ts.LocalMidnight();

  sql::Statement select(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT id, visit_count FROM segment_usage "
      "WHERE time_slot = ? AND segment_id = ?"));
  select.BindTime(0, t);
  select.BindInt64(1, segment_id);

  if (!select.is_valid())
    return false;

  if (select.Step()) {
    sql::Statement update(GetDB().GetCachedStatement(SQL_FROM_HERE,
        "UPDATE segment_usage SET visit_count = ? WHERE id = ?"));
    update.BindInt64(0, select.ColumnInt64(1) + static_cast<int64_t>(amount));
    update.BindInt64(1, select.ColumnInt64(0));

    return update.Run();
  } else if (amount > 0) {
    sql::Statement insert(GetDB().GetCachedStatement(SQL_FROM_HERE,
        "INSERT INTO segment_usage "
        "(segment_id, time_slot, visit_count) VALUES (?, ?, ?)"));
    insert.BindInt64(0, segment_id);
    insert.BindTime(1, t);
    insert.BindInt64(2, static_cast<int64_t>(amount));

    return insert.Run();
  }

  return true;
}

// Gathers the highest-ranked segments, computed in two phases.
std::vector<std::unique_ptr<PageUsageData>>
VisitSegmentDatabase::QuerySegmentUsage(
    int max_result_count,
    const base::RepeatingCallback<bool(const GURL&)>& url_filter) {
  // Phase 1: Gather all segments and compute scores.
  std::vector<std::unique_ptr<PageUsageData>> segments;
  base::Time now = base::Time::Now();

  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "SELECT segment_id, time_slot, visit_count "
                                 "FROM segment_usage ORDER BY segment_id"));
  if (!statement.is_valid())
    return std::vector<std::unique_ptr<PageUsageData>>();

  SegmentVisitor segment_visitor(&statement);
  SegmentInfo segment_info;
  std::unique_ptr<SegmentScorer> scorer =
      SegmentScorer::CreateFromFeatureFlags();
  while (segment_visitor.Step(&segment_info)) {
    DCHECK(!segment_info.time_slots.empty());
    DCHECK_EQ(segment_info.time_slots.size(), segment_info.visit_counts.size());

    std::unique_ptr<PageUsageData> segment =
        std::make_unique<PageUsageData>(segment_info.segment_id);
    segment->SetLastVisitTimeslot(*std::max_element(
        segment_info.time_slots.begin(), segment_info.time_slots.end()));
    segment->SetVisitCount(std::accumulate(segment_info.visit_counts.begin(),
                                           segment_info.visit_counts.end(), 0));
    segment->SetScore(scorer->Compute(segment_info.time_slots,
                                      segment_info.visit_counts, now));
    segments.push_back(std::move(segment));
  }

  // Order by descending scores.
  std::sort(segments.begin(), segments.end(),
            [](const std::unique_ptr<PageUsageData>& lhs,
               const std::unique_ptr<PageUsageData>& rhs) {
              return lhs->GetScore() > rhs->GetScore();
            });

  // Phase 2: Read details (url, title, etc.) for the highest-ranked segments.
  sql::Statement statement2(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT urls.url, urls.title FROM urls "
      "JOIN segments ON segments.url_id = urls.id "
      "WHERE segments.id = ?"));
  if (!statement2.is_valid())
    return std::vector<std::unique_ptr<PageUsageData>>();

  std::vector<std::unique_ptr<PageUsageData>> results;
  DCHECK_GE(max_result_count, 0);
  for (std::unique_ptr<PageUsageData>& pud : segments) {
    statement2.BindInt64(0, pud->GetID());
    if (statement2.Step()) {
      GURL url(statement2.ColumnString(0));
      if (url_filter.is_null() || url_filter.Run(url)) {
        pud->SetURL(url);
        pud->SetTitle(statement2.ColumnString16(1));
        results.push_back(std::move(pud));
        if (results.size() >= static_cast<size_t>(max_result_count))
          break;
      }
    }
    statement2.Reset(true);
  }

  return results;
}

bool VisitSegmentDatabase::DeleteSegmentDataOlderThan(base::Time older_than) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM segment_usage WHERE time_slot < ?"));
  statement.BindTime(0, older_than.LocalMidnight());

  return statement.Run();
}

bool VisitSegmentDatabase::DeleteSegmentForURL(URLID url_id) {
  sql::Statement delete_usage(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM segment_usage WHERE segment_id IN "
      "(SELECT id FROM segments WHERE url_id = ?)"));
  delete_usage.BindInt64(0, url_id);

  if (!delete_usage.Run())
    return false;

  sql::Statement delete_seg(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM segments WHERE url_id = ?"));
  delete_seg.BindInt64(0, url_id);

  return delete_seg.Run();
}

bool VisitSegmentDatabase::MigratePresentationIndex() {
  sql::Transaction transaction(&GetDB());
  return transaction.Begin() &&
      GetDB().Execute("DROP TABLE presentation") &&
      GetDB().Execute("CREATE TABLE segments_tmp ("
                      "id INTEGER PRIMARY KEY,"
                      "name VARCHAR,"
                      "url_id INTEGER NON NULL)") &&
      GetDB().Execute("INSERT INTO segments_tmp SELECT "
                      "id, name, url_id FROM segments") &&
      GetDB().Execute("DROP TABLE segments") &&
      GetDB().Execute("ALTER TABLE segments_tmp RENAME TO segments") &&
      transaction.Commit();
}

bool VisitSegmentDatabase::MigrateVisitSegmentNames() {
  sql::Statement select(
      GetDB().GetUniqueStatement("SELECT id, name FROM segments"));
  if (!select.is_valid())
    return false;

  bool success = true;
  while (select.Step()) {
    SegmentID id = select.ColumnInt64(0);
    std::string old_name = select.ColumnString(1);
    std::string new_name = ComputeSegmentName(GURL(old_name));
    if (new_name.empty() || old_name == new_name)
      continue;

    SegmentID to_segment_id = GetSegmentNamed(new_name);
    if (to_segment_id) {
      // `new_name` is already in use, so merge.
      success = success && MergeSegments(/*from_segment_id=*/id, to_segment_id);
    } else {
      // Trivial rename of the segment.
      success = success && RenameSegment(id, new_name);
    }
  }
  return success;
}

bool VisitSegmentDatabase::RenameSegment(SegmentID segment_id,
                                         const std::string& new_name) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "UPDATE segments SET name = ? WHERE id = ?"));
  statement.BindString(0, new_name);
  statement.BindInt64(1, segment_id);
  return statement.Run();
}

bool VisitSegmentDatabase::MergeSegments(SegmentID from_segment_id,
                                         SegmentID to_segment_id) {
  sql::Transaction transaction(&GetDB());
  if (!transaction.Begin())
    return false;

  // For each time slot where there are visits for the absorbed segment
  // (`from_segment_id`), add them to the absorbing/staying segment
  // (`to_segment_id`).
  sql::Statement select(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "SELECT time_slot, visit_count FROM "
                                 "segment_usage WHERE segment_id = ?"));
  select.BindInt64(0, from_segment_id);
  while (select.Step()) {
    base::Time ts = select.ColumnTime(0);
    int64_t visit_count = select.ColumnInt64(1);
    UpdateSegmentVisitCount(to_segment_id, ts, visit_count);
  }

  // Update all references in the visits database.
  sql::Statement update(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "UPDATE visits SET segment_id = ? WHERE segment_id = ?"));
  update.BindInt64(0, to_segment_id);
  update.BindInt64(1, from_segment_id);
  if (!update.Run())
    return false;

  // Delete old segment usage data.
  sql::Statement deletion1(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM segment_usage WHERE segment_id = ?"));
  deletion1.BindInt64(0, from_segment_id);
  if (!deletion1.Run())
    return false;

  // Delete old segment data.
  sql::Statement deletion2(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM segments WHERE id = ?"));
  deletion2.BindInt64(0, from_segment_id);
  if (!deletion2.Run())
    return false;

  return transaction.Commit();
}

}  // namespace history
