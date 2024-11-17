// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_metadata_store_test_util.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "components/offline_pages/core/model/add_page_task.h"
#include "components/offline_pages/core/model/get_pages_task.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

int64_t GetPageCountSync(sql::Database* db) {
  static const char kSql[] = "SELECT count(*) FROM offlinepages_v1";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  if (statement.Step()) {
    return statement.ColumnInt64(0);
  }
  return 0UL;
}

}  // namespace

OfflinePageMetadataStoreTestUtil::OfflinePageMetadataStoreTestUtil()
    : store_ptr_(nullptr) {}

OfflinePageMetadataStoreTestUtil::~OfflinePageMetadataStoreTestUtil() = default;

void OfflinePageMetadataStoreTestUtil::BuildStore() {
  if (!temp_directory_.IsValid() && !temp_directory_.CreateUniqueTempDir()) {
    DVLOG(1) << "temp_directory_ not created";
    return;
  }

  store_ = std::make_unique<OfflinePageMetadataStore>(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      temp_directory_.GetPath());
  store_ptr_ = store_.get();
}

void OfflinePageMetadataStoreTestUtil::BuildStoreInMemory() {
  store_ = std::make_unique<OfflinePageMetadataStore>(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  store_ptr_ = store_.get();
}

void OfflinePageMetadataStoreTestUtil::DeleteStore() {
  store_ptr_ = nullptr;
  store_.reset();
}

std::unique_ptr<OfflinePageMetadataStore>
OfflinePageMetadataStoreTestUtil::ReleaseStore() {
  return std::move(store_);
}

void OfflinePageMetadataStoreTestUtil::InsertItem(const OfflinePageItem& page) {
  base::RunLoop run_loop;
  AddPageResult result;
  auto task = std::make_unique<AddPageTask>(
      store(), page, base::BindLambdaForTesting([&](AddPageResult cb_result) {
        result = cb_result;
        run_loop.Quit();
      }));
  task->Execute(base::DoNothing());
  run_loop.Run();
  EXPECT_EQ(AddPageResult::SUCCESS, result);
}

int64_t OfflinePageMetadataStoreTestUtil::GetPageCount() {
  base::RunLoop run_loop;
  int64_t count = 0;
  store()->Execute(
      base::BindOnce(&GetPageCountSync),
      base::BindOnce(base::BindLambdaForTesting([&](int64_t cb_count) {
        count = cb_count;
        run_loop.Quit();
      })),
      int64_t());
  run_loop.Run();
  return count;
}

std::unique_ptr<OfflinePageItem>
OfflinePageMetadataStoreTestUtil::GetPageByOfflineId(int64_t offline_id) {
  base::RunLoop run_loop;
  PageCriteria criteria;
  criteria.offline_ids = std::vector<int64_t>{offline_id};
  OfflinePageItem* page = nullptr;
  auto task = std::make_unique<GetPagesTask>(
      store(), criteria,
      base::BindLambdaForTesting(
          [&](const std::vector<OfflinePageItem>& cb_pages) {
            if (!cb_pages.empty()) {
              page = new OfflinePageItem(cb_pages[0]);
            }
            run_loop.Quit();
          }));
  task->Execute(base::DoNothing());
  run_loop.Run();
  return base::WrapUnique<OfflinePageItem>(page);
}

}  // namespace offline_pages
