// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/visuals_availability_task.h"

#include "base/functional/bind.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

namespace {
VisualsAvailability VisualsAvailableSync(int64_t offline_id,
                                         sql::Database* db) {
  static const char kSql[] =
      "SELECT length(thumbnail)>0,length(favicon)>0 FROM page_thumbnails"
      " WHERE offline_id=?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, offline_id);
  if (!statement.Step())
    return {false, false};

  return {statement.ColumnBool(0), statement.ColumnBool(1)};
}

}  // namespace

VisualsAvailabilityTask::VisualsAvailabilityTask(
    OfflinePageMetadataStore* store,
    int64_t offline_id,
    VisualsAvailableCallback exists_callback)
    : store_(store),
      offline_id_(offline_id),
      exists_callback_(std::move(exists_callback)) {}

VisualsAvailabilityTask::~VisualsAvailabilityTask() = default;

void VisualsAvailabilityTask::Run() {
  store_->Execute(base::BindOnce(VisualsAvailableSync, offline_id_),
                  base::BindOnce(&VisualsAvailabilityTask::OnVisualsAvailable,
                                 weak_ptr_factory_.GetWeakPtr()),
                  {false, false});
}

void VisualsAvailabilityTask::OnVisualsAvailable(
    VisualsAvailability availability) {
  TaskComplete();
  std::move(exists_callback_).Run(std::move(availability));
}

}  // namespace offline_pages
