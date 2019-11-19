// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/mark_page_accessed_task.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/model/offline_page_model_utils.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

namespace {

#define OFFLINE_PAGES_TABLE_NAME "offlinepages_v1"

void ReportAccessHistogram(int64_t offline_id,
                           base::Time access_time,
                           sql::Database* db) {
  // Used as upper bound of PageAccessInterval histogram which is used for
  // evaluating how good the expiration period is. The expiration period of a
  // page will be longer than one year in extreme cases so it's good enough.
  const int kMinutesPerYear = base::TimeDelta::FromDays(365).InMinutes();

  static const char kSql[] =
      "SELECT client_namespace, last_access_time FROM " OFFLINE_PAGES_TABLE_NAME
      " WHERE offline_id = ?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, offline_id);
  if (statement.Step()) {
    std::string name_space = statement.ColumnString(0);
    UMA_HISTOGRAM_ENUMERATION("OfflinePages.AccessPageCount",
                              model_utils::ToNamespaceEnum(name_space));

    base::Time last_access_time =
        store_utils::FromDatabaseTime(statement.ColumnInt64(1));
    base::UmaHistogramCustomCounts(
        model_utils::AddHistogramSuffix(name_space,
                                        "OfflinePages.PageAccessInterval"),
        (access_time - last_access_time).InMinutes(), 1, kMinutesPerYear, 100);
  }
}

bool MarkPageAccessedSync(const base::Time& access_time,
                          int64_t offline_id,
                          sql::Database* db) {
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  ReportAccessHistogram(offline_id, access_time, db);

  static const char kSql[] =
      "UPDATE OR IGNORE " OFFLINE_PAGES_TABLE_NAME
      " SET last_access_time = ?, access_count = access_count + 1"
      " WHERE offline_id = ?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, store_utils::ToDatabaseTime(access_time));
  statement.BindInt64(1, offline_id);
  if (!statement.Run())
    return false;

  return transaction.Commit();
}

}  // namespace

MarkPageAccessedTask::MarkPageAccessedTask(OfflinePageMetadataStore* store,
                                           int64_t offline_id,
                                           const base::Time& access_time)
    : store_(store), offline_id_(offline_id), access_time_(access_time) {
  DCHECK(store_);
}

MarkPageAccessedTask::~MarkPageAccessedTask() {}

void MarkPageAccessedTask::Run() {
  store_->Execute(
      base::BindOnce(&MarkPageAccessedSync, access_time_, offline_id_),
      base::BindOnce(&MarkPageAccessedTask::OnMarkPageAccessedDone,
                     weak_ptr_factory_.GetWeakPtr()),
      false);
}

void MarkPageAccessedTask::OnMarkPageAccessedDone(bool result) {
  // TODO(romax): https://crbug.com/772204. Replace the DVLOG with UMA
  // collecting. If there's a need, introduce more detailed local enums
  // indicating which part failed.
  DVLOG(1) << "MarkPageAccessed returns with result: " << result;
  TaskComplete();
}

}  // namespace offline_pages
