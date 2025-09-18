// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_cache.h"

#include <optional>

#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_content_annotations/core/page_content_store.h"
#include "url/gurl.h"

namespace page_content_annotations {

namespace {
constexpr base::TimeDelta kStartupDeleteDelay = base::Seconds(25);
constexpr base::TimeDelta kPeriodicDeleteDelay = base::Days(1);
}  // namespace

PageContentCache::PageContentCache(os_crypt_async::OSCryptAsync* os_crypt_async,
                                   const base::FilePath& profile_dir)
    : store_(
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
               base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
          profile_dir.Append(FILE_PATH_LITERAL("annotated_page_contents_db"))) {
  os_crypt_async->GetInstance(base::BindOnce(
      &PageContentCache::OnOsCryptAsyncReady, weak_ptr_factory_.GetWeakPtr()));

  // Run deletion task at startup and schedule it to run periodically.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PageContentCache::DeleteOldData,
                     weak_ptr_factory_.GetWeakPtr()),
      kStartupDeleteDelay);
}

PageContentCache::~PageContentCache() = default;

void PageContentCache::GetPageContentForTab(int64_t tab_id,
                                            GetPageContentCallback callback) {
  if (!store_initialized_) {
    pending_tasks_.push_back(base::BindOnce(
        &PageContentCache::GetPageContentForTab, weak_ptr_factory_.GetWeakPtr(),
        tab_id, std::move(callback)));
    return;
  }
  store_.AsyncCall(&optimization_guide::PageContentStore::GetPageContentForTab)
      .WithArgs(tab_id)
      .Then(std::move(callback));
}

void PageContentCache::GetAllTabIds(GetAllTabIdsCallback callback) {
  if (!store_initialized_) {
    pending_tasks_.push_back(base::BindOnce(&PageContentCache::GetAllTabIds,
                                            weak_ptr_factory_.GetWeakPtr(),
                                            std::move(callback)));
    return;
  }
  store_.AsyncCall(&optimization_guide::PageContentStore::GetAllTabIds)
      .Then(std::move(callback));
}

void PageContentCache::CachePageContent(
    int64_t tab_id,
    const GURL& url,
    const base::Time& visit_timestamp,
    const base::Time& extraction_timestamp,
    const optimization_guide::proto::AnnotatedPageContent& apc) {
  if (!store_initialized_) {
    pending_tasks_.push_back(base::BindOnce(
        &PageContentCache::CachePageContent, weak_ptr_factory_.GetWeakPtr(),
        tab_id, url, visit_timestamp, extraction_timestamp, apc));
    return;
  }
  store_.AsyncCall(&optimization_guide::PageContentStore::AddPageContent)
      .WithArgs(url, apc, visit_timestamp, extraction_timestamp,
                std::make_optional(tab_id))
      .Then(base::BindOnce([](bool success) {}));
}

void PageContentCache::RemovePageContentForTab(int64_t tab_id) {
  store_
      .AsyncCall(&optimization_guide::PageContentStore::DeletePageContentForTab)
      .WithArgs(tab_id)
      .Then(base::BindOnce([](bool success) {}));
}

void PageContentCache::OnOsCryptAsyncReady(
    os_crypt_async::Encryptor encryptor) {
  store_.AsyncCall(&optimization_guide::PageContentStore::InitWithEncryptor)
      .WithArgs(std::move(encryptor))
      .Then(base::BindOnce(&PageContentCache::OnStoreInitialized,
                           weak_ptr_factory_.GetWeakPtr()));
}

void PageContentCache::OnStoreInitialized() {
  CHECK(!store_initialized_);
  store_initialized_ = true;
  for (auto& task : pending_tasks_) {
    std::move(task).Run();
  }
  pending_tasks_.clear();
}

void PageContentCache::DeleteOldData() {
  const base::Time older_than =
      base::Time::Now() -
      base::Days(features::kPageContentCacheMaxCacheAgeInDays.Get());
  store_
      .AsyncCall(
          &optimization_guide::PageContentStore::DeletePageContentOlderThan)
      .WithArgs(older_than)
      .Then(base::BindOnce([](bool success) {}));

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PageContentCache::DeleteOldData,
                     weak_ptr_factory_.GetWeakPtr()),
      kPeriodicDeleteDelay);
}

}  // namespace page_content_annotations
