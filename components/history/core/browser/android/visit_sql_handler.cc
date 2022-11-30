// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/android/visit_sql_handler.h"

#include <stdint.h>

#include "base/check.h"
#include "base/time/time.h"
#include "components/history/core/browser/url_database.h"
#include "components/history/core/browser/visit_database.h"

using base::Time;

namespace history {

namespace {

// The interesting columns of this handler.
const HistoryAndBookmarkRow::ColumnID kInterestingColumns[] = {
    HistoryAndBookmarkRow::CREATED, HistoryAndBookmarkRow::VISIT_COUNT,
    HistoryAndBookmarkRow::LAST_VISIT_TIME };

} // namespace

VisitSQLHandler::VisitSQLHandler(URLDatabase* url_db, VisitDatabase* visit_db)
    : SQLHandler(kInterestingColumns, std::size(kInterestingColumns)),
      url_db_(url_db),
      visit_db_(visit_db) {}

VisitSQLHandler::~VisitSQLHandler() {
}

// The created time is updated according the given `row`.
// We simulate updating created time by
// a. Remove all visits.
// b. Insert a new visit which has visit time same as created time.
// c. Insert the number of visits according the visit count in urls table.
//
// Visit row is insertted/removed to keep consistent with urls table.
// a. If the visit count in urls table is less than the visit rows in visit
//    table, all existent visits will be removed. The new visits will be
//    insertted according the value in urls table.
// b. Otherwise, only add the increased number of visit count.
bool VisitSQLHandler::Update(const HistoryAndBookmarkRow& row,
                             const TableIDRows& ids_set) {
  for (TableIDRows::const_iterator id = ids_set.begin();
       id != ids_set.end(); ++id) {
    VisitVector visits;
    if (!visit_db_->GetVisitsForURL(id->url_id, &visits))
      return false;
    int visit_count_in_table = visits.size();
    URLRow url_row;
    if (!url_db_->GetURLRow(id->url_id, &url_row))
      return false;
    int visit_count_needed = url_row.visit_count();

    if (visit_count_needed == 0)
      return Delete(ids_set);

    // If created time is updated or new visit count is less than the current
    // one, delete all visit rows.
    if (row.is_value_set_explicitly(HistoryAndBookmarkRow::CREATED) ||
        visit_count_in_table > visit_count_needed) {
      if (!DeleteVisitsForURL(id->url_id))
        return false;
      visit_count_in_table = 0;
    }

    if (row.is_value_set_explicitly(HistoryAndBookmarkRow::CREATED) &&
        visit_count_needed > 0) {
      if (!AddVisit(id->url_id, row.created()))
        return false;
      visit_count_in_table++;
    }

    if (!AddVisitRows(id->url_id, visit_count_needed - visit_count_in_table,
                       url_row.last_visit()))
      return false;
  }
  return true;
}

bool VisitSQLHandler::Insert(HistoryAndBookmarkRow* row) {
  DCHECK(row->is_value_set_explicitly(HistoryAndBookmarkRow::URL_ID));

  URLRow url_row;
  if (!url_db_->GetURLRow(row->url_id(), &url_row))
    return false;

  int visit_count = url_row.visit_count();

  if (visit_count == 0)
    return true;

  // Add a row if the last visit time is different from created time.
  if (row->is_value_set_explicitly(HistoryAndBookmarkRow::CREATED) &&
      row->created() != url_row.last_visit() && visit_count > 0) {
    if (!AddVisit(row->url_id(), row->created()))
      return false;
    visit_count--;
  }

  if (!AddVisitRows(row->url_id(), visit_count, url_row.last_visit()))
    return false;

  return true;
}

bool VisitSQLHandler::Delete(const TableIDRows& ids_set) {
  for (TableIDRows::const_iterator ids = ids_set.begin();
       ids != ids_set.end(); ++ids) {
    DeleteVisitsForURL(ids->url_id);
  }
  return true;
}

bool VisitSQLHandler::AddVisit(URLID url_id, const Time& visit_time) {
  // TODO : Is 'ui::PAGE_TRANSITION_AUTO_BOOKMARK' proper?
  // if not, a new ui::PageTransition type will need.
  VisitRow visit_row(url_id, visit_time, /*referring_visit=*/0,
                     ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                     /*segment_id=*/0,
                     /*incremented_omnibox_typed_score=*/false,
                     /*opening_visit=*/0);
  return visit_db_->AddVisit(&visit_row, SOURCE_BROWSED);
}

bool VisitSQLHandler::AddVisitRows(URLID url_id,
                                   int visit_count,
                                   const Time& last_visit_time) {
  int64_t last_update_value = last_visit_time.ToInternalValue();
  for (int i = 0; i < visit_count; i++) {
    if (!AddVisit(url_id, Time::FromInternalValue(last_update_value - i)))
      return false;
  }
  return true;
}

bool VisitSQLHandler::DeleteVisitsForURL(URLID url_id) {
  VisitVector visits;
  if (!visit_db_->GetVisitsForURL(url_id, &visits))
    return false;

  for (VisitVector::const_iterator v = visits.begin(); v != visits.end(); ++v) {
    visit_db_->DeleteVisit(*v);
  }
  return true;
}

}  // namespace history.
