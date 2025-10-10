// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_cache.h"

#include <optional>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
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

constexpr base::FilePath::CharType kPageContentAnnotationsDatabaseDirName[] =
    FILE_PATH_LITERAL("annotated_page_contents_db");

constexpr base::TimeDelta kStartupDeleteDelay = base::Seconds(25);
constexpr base::TimeDelta kPeriodicDeleteDelay = base::Days(1);

}  // namespace

PageContentCache::PageContentCache(os_crypt_async::OSCryptAsync* os_crypt_async,
                                   const base::FilePath& profile_dir)
    : database_path_(
          profile_dir.Append(kPageContentAnnotationsDatabaseDirName)),
      store_(base::ThreadPool::CreateSequencedTaskRunner(
                 {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
                  base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
             database_path_) {
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

void PageContentCache::RecordMetrics(std::set<int64_t> eligible_tab_ids) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&base::ComputeDirectorySize, database_path_),
      base::BindOnce(&PageContentCache::OnCacheSizeCalculated,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(eligible_tab_ids)));
}

void PageContentCache::OnCacheSizeCalculated(std::set<int64_t> eligible_tab_ids,
                                             int64_t total_cache_size) {
  base::UmaHistogramMemoryKB(
      "OptimizationGuide.PageContentCache.TotalCacheSize",
      total_cache_size / 1024);

  GetAllTabIds(base::BindOnce(&PageContentCache::OnReceiveAllCachedTabIds,
                              weak_ptr_factory_.GetWeakPtr(), total_cache_size,
                              std::move(eligible_tab_ids)));
}

void PageContentCache::OnReceiveAllCachedTabIds(
    int64_t total_cache_size,
    std::set<int64_t> eligible_tab_ids,
    std::vector<int64_t> cached_tab_ids) {
  int cached_tabs_count = 0;
  int stale_entries_count = 0;
  for (int64_t tab_id : cached_tab_ids) {
    if (eligible_tab_ids.count(tab_id)) {
      cached_tabs_count++;
    } else {
      stale_entries_count++;
    }
  }

  base::UmaHistogramCounts1000(
      "OptimizationGuide.PageContentCache.CachedTabsCount", cached_tabs_count);
  base::UmaHistogramCounts1000(
      "OptimizationGuide.PageContentCache.NotCachedTabsCount",
      eligible_tab_ids.size() - cached_tabs_count);
  base::UmaHistogramCounts1000(
      "OptimizationGuide.PageContentCache.StaleCacheEntriesCount",
      stale_entries_count);

  if (cached_tabs_count > 0) {
    base::UmaHistogramMemoryKB("OptimizationGuide.PageContentCache.AvgPageSize",
                               (total_cache_size / 1024) / cached_tabs_count);
  }
  if (eligible_tab_ids.size() > 0) {
    base::UmaHistogramPercentage(
        "OptimizationGuide.PageContentCache.EligibleTabsCachedPercentage",
        (cached_tabs_count * 100) / eligible_tab_ids.size());
  }
}

void PageContentCache::CachePageContent(
    int64_t tab_id,
    const GURL& url,
    const base::Time& visit_timestamp,
    const base::Time& extraction_timestamp,
    const optimization_guide::proto::PageContext& page_context) {
  if (!store_initialized_) {
    pending_tasks_.push_back(base::BindOnce(
        &PageContentCache::CachePageContent, weak_ptr_factory_.GetWeakPtr(),
        tab_id, url, visit_timestamp, extraction_timestamp, page_context));
    return;
  }
  store_.AsyncCall(&optimization_guide::PageContentStore::AddPageContent)
      .WithArgs(url, page_context, visit_timestamp, extraction_timestamp,
                std::make_optional(tab_id))
      .Then(base::BindOnce(
          [](base::WeakPtr<PageContentCache> cache, int64_t tab_id,
             bool success) {
            base::UmaHistogramBoolean(
                "OptimizationGuide.PageContentCache.AddPageContentResult",
                success);
            if (cache && success) {
              cache->observers_.Notify(&Observer::OnCachePopulated, tab_id);
            }
          },
          weak_ptr_factory_.GetWeakPtr(), tab_id));
}

void PageContentCache::RemovePageContentForTab(int64_t tab_id) {
  store_
      .AsyncCall(&optimization_guide::PageContentStore::DeletePageContentForTab)
      .WithArgs(tab_id)
      .Then(base::BindOnce(
          [](base::WeakPtr<PageContentCache> cache, int64_t tab_id,
             bool success) {
            base::UmaHistogramBoolean(
                "OptimizationGuide.PageContentCache."
                "RemovePageContentForTabResult",
                success);
            if (cache && success) {
              cache->observers_.Notify(&Observer::OnCacheRemoved, tab_id);
            }
          },
          weak_ptr_factory_.GetWeakPtr(), tab_id));
}

void PageContentCache::OnOsCryptAsyncReady(
    os_crypt_async::Encryptor encryptor) {
  store_.AsyncCall(&optimization_guide::PageContentStore::InitWithEncryptor)
      .WithArgs(std::move(encryptor))
      .Then(base::BindOnce(&PageContentCache::OnStoreInitialized,
                           weak_ptr_factory_.GetWeakPtr()));
}

void PageContentCache::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PageContentCache::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
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
