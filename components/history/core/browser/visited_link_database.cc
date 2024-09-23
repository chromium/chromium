// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/visited_link_database.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/database_utils/url_converter.h"
#include "components/url_formatter/url_formatter.h"
#include "sql/statement.h"
#include "url/gurl.h"

// The VisitedLinkDatabase contains the following fields:
// `id` - the unique int64 ID assigned to this row.
// `link_url_id` - the unique URLID assigned to the row where this link url is
//               stored in the URLDatabase. ID is stored to avoid storing the
//               URL twice.
// `top_level_url` - the GURL of the top-level frame where the link url was
//                 visited from.
// `frame_url` - the GURL of the frame where the link was visited from.
// `visit_count` - the number of entries in the VisitDatabase corresponding to
//               this row (must exactly match the <link_url, top_level_url,
//               frame_url> partition key).
#define HISTORY_VISITED_LINK_ROW_FIELDS                                        \
  "visited_links.id, visited_links.link_url_id, visited_links.top_level_url, " \
  "visited_links.frame_url, visited_links.visit_count"

namespace history {

VisitedLinkDatabase::VisitedLinkEnumerator::VisitedLinkEnumerator()
    : initialized_(false) {}

VisitedLinkDatabase::VisitedLinkEnumerator::~VisitedLinkEnumerator() = default;

bool VisitedLinkDatabase::DropVisitedLinkTable() {
  return GetDB().Execute("DROP TABLE visited_links");
}

bool VisitedLinkDatabase::VisitedLinkEnumerator::GetNextVisitedLink(
    VisitedLinkRow& r) {
  if (statement_.Step()) {
    FillVisitedLinkRow(statement_, r);
    return true;
  }
  return false;
}

VisitedLinkDatabase::VisitedLinkDatabase() = default;

VisitedLinkDatabase::~VisitedLinkDatabase() = default;

// Convenience to fill a VisitedLinkRow. Must be in sync with the fields in
// HISTORY_VISITED_LINK_ROW_FIELDS.
void VisitedLinkDatabase::FillVisitedLinkRow(sql::Statement& s,
                                             VisitedLinkRow& i) {
  i.id = s.ColumnInt64(0);
  i.link_url_id = s.ColumnInt64(1);
  i.top_level_url = GURL(s.ColumnString(2));
  i.frame_url = GURL(s.ColumnString(3));
  i.visit_count = s.ColumnInt(4);
}

bool VisitedLinkDatabase::GetVisitedLinkRow(VisitedLinkID visited_link_id,
                                            VisitedLinkRow& info) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT " HISTORY_VISITED_LINK_ROW_FIELDS
                     " FROM visited_links WHERE id=?"));
  statement.BindInt64(0, visited_link_id);

  if (statement.Step()) {
    FillVisitedLinkRow(statement, info);
    return true;
  }
  return false;
}

VisitedLinkID VisitedLinkDatabase::GetRowForVisitedLink(
    URLID link_url_id,
    const GURL& top_level_url,
    const GURL& frame_url,
    VisitedLinkRow& info) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT " HISTORY_VISITED_LINK_ROW_FIELDS
      " FROM visited_links WHERE link_url_id=? AND top_level_url=?"
      " AND frame_url=?"));
  statement.BindInt64(0, link_url_id);
  statement.BindString(1, database_utils::GurlToDatabaseUrl(top_level_url));
  statement.BindString(2, database_utils::GurlToDatabaseUrl(frame_url));

  if (!statement.Step()) {
    return 0;  // no data
  }

  FillVisitedLinkRow(statement, info);
  return info.id;
}

bool VisitedLinkDatabase::UpdateVisitedLinkRowVisitCount(
    VisitedLinkID visited_link_id,
    int visit_count) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "UPDATE visited_links SET visit_count=? WHERE id=?"));
  statement.BindInt(0, visit_count);
  statement.BindInt64(1, visited_link_id);

  return statement.Run() && GetDB().GetLastChangeCount() > 0;
}

VisitedLinkID VisitedLinkDatabase::AddVisitedLink(URLID link_url_id,
                                                  const GURL& top_level_url,
                                                  const GURL& frame_url,
                                                  int visit_count) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO visited_links (link_url_id, top_level_url, frame_url, "
      "visit_count) VALUES (?,?,?,?)"));
  statement.BindInt64(0, link_url_id);
  statement.BindString(1, database_utils::GurlToDatabaseUrl(top_level_url));
  statement.BindString(2, database_utils::GurlToDatabaseUrl(frame_url));
  statement.BindInt(3, visit_count);

  if (!statement.Run()) {
    VLOG(0) << "Failed to add visited link " << link_url_id << " "
            << top_level_url << " " << frame_url
            << " to table history.visited_links.";
    return 0;
  }
  return GetDB().GetLastInsertRowId();
}

bool VisitedLinkDatabase::DeleteVisitedLinkRow(VisitedLinkID id) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM visited_links WHERE id = ?"));
  statement.BindInt64(0, id);
  return statement.Run();
}

bool VisitedLinkDatabase::InitVisitedLinkEnumeratorForEverything(
    VisitedLinkEnumerator& enumerator) {
  DCHECK(!enumerator.initialized_);
  enumerator.statement_.Assign(GetDB().GetUniqueStatement(
      "SELECT " HISTORY_VISITED_LINK_ROW_FIELDS " FROM visited_links"));
  enumerator.initialized_ = enumerator.statement_.is_valid();
  return enumerator.statement_.is_valid();
}

bool VisitedLinkDatabase::CreateVisitedLinkTable() {
  if (GetDB().DoesTableExist("visited_links")) {
    return true;
  }
  // Note: revise implementation for InsertOrUpdateVisitedLinkRowByID() if you
  // add any new constraints to the schema.
  static constexpr char kSql[] =
      "CREATE TABLE visited_links("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "link_url_id INTEGER NOT NULL,"
      "top_level_url LONGVARCHAR NOT NULL,"
      "frame_url LONGVARCHAR NOT NULL,"
      "visit_count INTEGER DEFAULT 0 NOT NULL)";
  if (!GetDB().Execute(kSql)) {
    return false;
  }

  // Creates the index over visited_links so we can quickly look up based on
  // visited link.
  return GetDB().Execute(
      "CREATE INDEX IF NOT EXISTS visited_links_index ON "
      "visited_links (link_url_id, top_level_url, frame_url)");
}
}  // namespace history
