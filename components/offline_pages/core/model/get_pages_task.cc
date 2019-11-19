// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/get_pages_task.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/offline_pages/core/offline_page_client_policy.h"
#include "components/offline_pages/core/offline_page_item_utils.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/gurl.h"

namespace offline_pages {
namespace {

using ReadResult = GetPagesTask::ReadResult;

#define OFFLINE_PAGE_PROJECTION                           \
  " offline_id,creation_time,file_size,last_access_time," \
  "access_count,system_download_id,file_missing_time,"    \
  "client_namespace,client_id,online_url,"                \
  "file_path,title,original_url,request_origin,digest,"   \
  "snippet,attribution"

ClientId OfflinePageClientId(const sql::Statement& statement) {
  return ClientId(statement.ColumnString(7), statement.ColumnString(8));
}

// Create an offline page item from a SQL result.
// Expects the order of columns as defined by OFFLINE_PAGE_PROJECTION macro.
OfflinePageItem MakeOfflinePageItem(const sql::Statement& statement) {
  OfflinePageItem item;
  item.offline_id = statement.ColumnInt64(0);
  item.creation_time = store_utils::FromDatabaseTime(statement.ColumnInt64(1));
  item.file_size = statement.ColumnInt64(2);
  item.last_access_time =
      store_utils::FromDatabaseTime(statement.ColumnInt64(3));
  item.access_count = statement.ColumnInt(4);
  item.system_download_id = statement.ColumnInt64(5);
  item.file_missing_time =
      store_utils::FromDatabaseTime(statement.ColumnInt64(6));
  item.client_id = OfflinePageClientId(statement);
  item.url = GURL(statement.ColumnString(9));
  item.file_path = base::FilePath(
      store_utils::FromDatabaseFilePath(statement.ColumnString(10)));
  item.title = statement.ColumnString16(11);
  item.original_url_if_different = GURL(statement.ColumnString(12));
  item.request_origin = statement.ColumnString(13);
  item.digest = statement.ColumnString(14);
  item.snippet = statement.ColumnString(15);
  item.attribution = statement.ColumnString(16);
  return item;
}

// Returns a pattern to be used in an SQLite LIKE expression to match a
// a URL ignoring the fragment. Warning: this match produces false positives,
// so the URL match must be verified by doing
// UrlWithoutFragment(a)==UrlWithoutFragment(b).
std::string RelaxedLikePattern(const GURL& url) {
  // In a LIKE expression, % matches any number of characters, and _ matches any
  // single character.
  // Replace % with _ in the URL to because _ is more restrictive.
  // Append a % to match any URL with our URL as a prefix, just in case the URL
  // being matched has a fragment.
  std::string string_to_match = UrlWithoutFragment(url).spec();
  std::replace(string_to_match.begin(), string_to_match.end(), '%', '_');
  string_to_match.push_back('%');
  return string_to_match;
}

}  // namespace

GetPagesTask::ReadResult::ReadResult() = default;
GetPagesTask::ReadResult::ReadResult(const ReadResult& other) = default;
GetPagesTask::ReadResult::~ReadResult() = default;

GetPagesTask::GetPagesTask(OfflinePageMetadataStore* store,
                           const PageCriteria& criteria,
                           MultipleOfflinePageItemCallback callback)
    : store_(store),
      criteria_(criteria),
      callback_(std::move(callback)) {
  DCHECK(store_);
  DCHECK(!callback_.is_null());
}

GetPagesTask::~GetPagesTask() = default;

void GetPagesTask::Run() {
  store_->Execute(base::BindOnce(&GetPagesTask::ReadPagesWithCriteriaSync,
                                 std::move(criteria_)),
                  base::BindOnce(&GetPagesTask::CompleteWithResult,
                                 weak_ptr_factory_.GetWeakPtr()),
                  ReadResult());
}

void GetPagesTask::CompleteWithResult(ReadResult result) {
  std::move(callback_).Run(result.pages);
  TaskComplete();
}

// Some comments on query performance as of March 2019:
// - SQLite stores data in row-oriented fashion, so there's little cost to
//   querying additional columns.
// - SQLite supports REGEXP, but it's slow, seems hardly worth using. LIKE is
//   fast.
// - Adding more simple conditions to the WHERE clause seems to hardly increase
//   runtime, so it's advantageous to add new conditions if they are likely to
//   eliminate output.
// - When a single item is returned from a query, using a WHERE clause is about
//   10x faster compared to just querying all rows and filtering the output in
//   C++.
// - The below query can process 10K rows in ~1ms (in-memory db).
// - If offline_id is in criteria, SQLite will use the index to evaluate the
//   query quickly. Otherwise, we need to read the whole table anyway. Unless
//   the db is loaded to memory, and disk access will likely dwarf any
//   other query optimizations.
ReadResult GetPagesTask::ReadPagesWithCriteriaSync(
    const PageCriteria& criteria,
    sql::Database* db) {
  ReadResult result;

  // Quick return for known empty results.
  if ((criteria.offline_ids && criteria.offline_ids.value().empty()) ||
      (criteria.client_ids && criteria.client_ids.value().empty()) ||
      (criteria.client_namespaces &&
       criteria.client_namespaces.value().empty())) {
    result.success = true;
    return result;
  }

  // Note: the WHERE clause here is a relaxed form of |criteria|, so returned
  // items must be re-checked with |MeetsCriteria|.
  static const char kSql[] =
      "SELECT " OFFLINE_PAGE_PROJECTION
      " FROM offlinepages_v1"
      " WHERE"
      " offline_id BETWEEN ? AND ?"
      " AND (? OR file_size=?)"
      " AND (? OR digest=?)"
      " AND (? OR instr(?,client_namespace)>0)"
      " AND (? OR request_origin=?)"
      " AND (? OR instr(?,client_id)>0)"
      " AND (? OR online_url LIKE ? OR original_url LIKE ?)"
      // Order by either creation_time or last_access_time, depending on
      // bound parameters.
      " ORDER BY creation_time*?+last_access_time*?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));

  int param = 0;

  if (criteria.offline_ids) {
    const std::vector<int64_t> ids = criteria.offline_ids.value();
    auto min_max = std::minmax_element(ids.begin(), ids.end());
    statement.BindInt64(param++, *min_max.first);
    statement.BindInt64(param++, *min_max.second);
  } else {
    statement.BindInt64(param++, INT64_MIN);
    statement.BindInt64(param++, INT64_MAX);
  }

  statement.BindBool(param++, !criteria.file_size);
  statement.BindInt64(param++, criteria.file_size.value_or(0));

  statement.BindBool(param++, criteria.digest.empty());
  statement.BindString(param++, criteria.digest);

  // For namespace and client_id, we use SQL's substring match function,
  // instr(), to provided an inexact match within the query. In both cases, we
  // pass SQLite a string equal to the concatenation of all possible values we
  // want to find, and then search that string for the row's namespace and
  // client_id respectively.
  std::vector<std::string> potential_namespaces =
      PotentiallyMatchingNamespaces(criteria);
  if (!potential_namespaces.empty()) {
    statement.BindBool(param++, false);
    statement.BindString(param++, base::JoinString(potential_namespaces, ""));
  } else {
    statement.BindBool(param++, true);
    statement.BindString(param++, "");
  }

  statement.BindBool(param++, criteria.request_origin.empty());
  statement.BindString(param++, criteria.request_origin);

  if (criteria.client_ids) {
    // Collect all client ids into a single string for matching in the query
    // with substring match (instr()).
    std::string concatenated_ids;
    for (const ClientId& id : criteria.client_ids.value()) {
      concatenated_ids += id.id;
    }
    statement.BindBool(param++, false);
    statement.BindString(param++, concatenated_ids);
  } else {
    statement.BindBool(param++, true);
    statement.BindString(param++, std::string());
  }

  const std::string url_pattern = !criteria.url.is_empty()
                                      ? RelaxedLikePattern(criteria.url)
                                      : std::string();

  statement.BindBool(param++, criteria.url.is_empty());
  statement.BindString(param++, url_pattern);
  statement.BindString(param++, url_pattern);

  // ORDER BY criteria.
  switch (criteria.result_order) {
    case PageCriteria::kDescendingCreationTime:
      statement.BindInt64(param++, -1);
      statement.BindInt64(param++, 0);
      break;
    case PageCriteria::kAscendingAccessTime:
      statement.BindInt64(param++, 0);
      statement.BindInt64(param++, 1);
      break;
    case PageCriteria::kDescendingAccessTime:
      statement.BindInt64(param++, 0);
      statement.BindInt64(param++, -1);
      break;
  }

  while (statement.Step()) {
    // Initially, read just the client ID to avoid creating the offline item
    // if it's filtered out.
    if (!MeetsCriteria(criteria, OfflinePageClientId(statement))) {
      continue;
    }
    OfflinePageItem item = MakeOfflinePageItem(statement);
    if (!MeetsCriteria(criteria, item))
      continue;

    result.pages.push_back(std::move(item));
    if (criteria.maximum_matches == result.pages.size())
      break;
  }

  result.success = statement.Succeeded();
  if (!result.success) {
    DLOG(ERROR) << "ReadPagesWithCriteriaSync: statement.Succeeded()=false";
    result.pages.clear();
  }
  return result;
}

}  // namespace offline_pages
