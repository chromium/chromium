// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/content/browser/content_visit_delegate.h"

#include <utility>

#include "base/check.h"
#include "base/memory/ref_counted.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/visitedlink/browser/partitioned_visitedlink_writer.h"
#include "components/visitedlink/browser/visitedlink_writer.h"
#include "components/visitedlink/core/visited_link.h"
#include "net/base/schemeful_site.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace history {
namespace {

// URLIterator from std::vector<GURL>
class URLIteratorFromURLs : public visitedlink::VisitedLinkWriter::URLIterator {
 public:
  explicit URLIteratorFromURLs(const std::vector<GURL>& urls)
      : itr_(urls.begin()), end_(urls.end()) {}

  URLIteratorFromURLs(const URLIteratorFromURLs&) = delete;
  URLIteratorFromURLs& operator=(const URLIteratorFromURLs&) = delete;

  // visitedlink::VisitedLinkWriter::URLIterator implementation.
  const GURL& NextURL() override { return *(itr_++); }
  bool HasNextURL() const override { return itr_ != end_; }

 private:
  std::vector<GURL>::const_iterator itr_;
  std::vector<GURL>::const_iterator end_;
};

// Creates a VisitedLinkIterator from std::vector<VisitedLink>. Allows us to
// efficiently delete a list of VisitedLinks from the partitioned hashtable.
class VisitedLinkIteratorFromLinks
    : public visitedlink::PartitionedVisitedLinkWriter::VisitedLinkIterator {
 public:
  explicit VisitedLinkIteratorFromLinks(const std::vector<VisitedLink>& links)
      : itr_(links.begin()), end_(links.end()) {}

  VisitedLinkIteratorFromLinks(const VisitedLinkIteratorFromLinks&) = delete;
  VisitedLinkIteratorFromLinks& operator=(const VisitedLinkIteratorFromLinks&) =
      delete;

  // visitedlink::PartitionedVisitedLinkWriter::VisitedLinkIterator
  // implementation.
  const VisitedLink& NextVisitedLink() override { return *(itr_++); }
  bool HasNextVisitedLink() const override { return itr_ != end_; }

 private:
  std::vector<VisitedLink>::const_iterator itr_;
  std::vector<VisitedLink>::const_iterator end_;
};

// IterateUrlsDBTask bridge HistoryBackend::URLEnumerator to
// visitedlink::VisitedLinkDelegate::URLEnumerator.
class IterateUrlsDBTask : public HistoryDBTask {
 public:
  explicit IterateUrlsDBTask(const scoped_refptr<
      visitedlink::VisitedLinkDelegate::URLEnumerator>& enumerator);

  IterateUrlsDBTask(const IterateUrlsDBTask&) = delete;
  IterateUrlsDBTask& operator=(const IterateUrlsDBTask&) = delete;

  ~IterateUrlsDBTask() override;

 private:
  // Implementation of HistoryDBTask.
  bool RunOnDBThread(HistoryBackend* backend, HistoryDatabase* db) override;
  void DoneRunOnMainThread() override;

  scoped_refptr<visitedlink::VisitedLinkDelegate::URLEnumerator> enumerator_;
};

IterateUrlsDBTask::IterateUrlsDBTask(const scoped_refptr<
    visitedlink::VisitedLinkDelegate::URLEnumerator>& enumerator)
    : enumerator_(enumerator) {
}

IterateUrlsDBTask::~IterateUrlsDBTask() {
}

bool IterateUrlsDBTask::RunOnDBThread(HistoryBackend* backend,
                                      HistoryDatabase* db) {
  bool success = false;
  if (db) {
    HistoryDatabase::URLEnumerator iter;
    if (db->InitURLEnumeratorForEverything(&iter)) {
      URLRow row;
      while (iter.GetNextURL(&row))
        enumerator_->OnURL(row.url());
      success = true;
    }
  }
  enumerator_->OnComplete(success);
  return true;
}

void IterateUrlsDBTask::DoneRunOnMainThread() {
}

// IterateVisitedLinkDBTask bridges HistoryBackend::VisitedLinkEnumerator to
// VisitedLinkDelegate::VisitedLinkEnumerator.
class IterateVisitedLinkDBTask : public HistoryDBTask {
 public:
  explicit IterateVisitedLinkDBTask(
      const scoped_refptr<
          visitedlink::VisitedLinkDelegate::VisitedLinkEnumerator>& enumerator);

  IterateVisitedLinkDBTask(const IterateVisitedLinkDBTask&) = delete;
  IterateVisitedLinkDBTask& operator=(const IterateVisitedLinkDBTask&) = delete;

  ~IterateVisitedLinkDBTask() override;

 private:
  // Implementation of HistoryDBTask.
  bool RunOnDBThread(HistoryBackend* backend, HistoryDatabase* db) override;
  void DoneRunOnMainThread() override;

  scoped_refptr<visitedlink::VisitedLinkDelegate::VisitedLinkEnumerator>
      enumerator_;
};

IterateVisitedLinkDBTask::IterateVisitedLinkDBTask(
    const scoped_refptr<
        visitedlink::VisitedLinkDelegate::VisitedLinkEnumerator>& enumerator)
    : enumerator_(enumerator) {}

IterateVisitedLinkDBTask::~IterateVisitedLinkDBTask() = default;

bool IterateVisitedLinkDBTask::RunOnDBThread(HistoryBackend* backend,
                                             HistoryDatabase* db) {
  // Begin iterating through the VisitedLinkDatabase.
  bool success = false;
  if (db) {
    HistoryDatabase::VisitedLinkEnumerator iter;
    if (db->InitVisitedLinkEnumeratorForEverything(iter)) {
      VisitedLinkRow row;
      while (iter.GetNextVisitedLink(row)) {
        URLRow url_info;
        // We must obtain the link url from the ID we're given.
        if (db->GetURLRow(row.link_url_id, &url_info)) {
          net::SchemefulSite top_level_site(row.top_level_url);
          url::Origin frame_origin = url::Origin::Create(row.frame_url);
          enumerator_->OnVisitedLink(url_info.url(), top_level_site,
                                     frame_origin);
        }
      }
      success = true;
    }
  }
  enumerator_->OnVisitedLinkComplete(success);
  return true;
}

void IterateVisitedLinkDBTask::DoneRunOnMainThread() {}

}  // namespace

ContentVisitDelegate::ContentVisitDelegate(
    content::BrowserContext* browser_context)
    : history_service_(nullptr) {
  // To keep the partitioned visited links experiments performant, we will only
  // construct and initialize one writer (partitioned or unpartitioned) at a
  // time. Callers of either `partitioned_writer_` or `visitedlink_writer_`
  // should ensure the pointer is not null before use.
  if (base::FeatureList::IsEnabled(
          blink::features::kPartitionVisitedLinkDatabase) ||
      base::FeatureList::IsEnabled(
          blink::features::kPartitionVisitedLinkDatabaseWithSelfLinks)) {
    partitioned_writer_ =
        std::make_unique<visitedlink::PartitionedVisitedLinkWriter>(
            browser_context, this);
  } else {
    visitedlink_writer_ = std::make_unique<visitedlink::VisitedLinkWriter>(
        browser_context, this, true);
  }
}

ContentVisitDelegate::~ContentVisitDelegate() {
}

bool ContentVisitDelegate::Init(HistoryService* history_service) {
  DCHECK(history_service);
  history_service_ = history_service;
  // To keep the partitioned visited links experiments performant, we will only
  // construct and initialize one writer (partitioned or unpartitioned) at a
  // time. Callers of either `partitioned_writer_` or `visitedlink_writer_`
  // should ensure the pointer is not null before use.
  if (base::FeatureList::IsEnabled(
          blink::features::kPartitionVisitedLinkDatabase) ||
      base::FeatureList::IsEnabled(
          blink::features::kPartitionVisitedLinkDatabaseWithSelfLinks)) {
    DCHECK(partitioned_writer_);
    return partitioned_writer_->Init();
  }
  DCHECK(visitedlink_writer_);
  return visitedlink_writer_->Init();
}

void ContentVisitDelegate::AddURL(const GURL& url) {
  // Not all callers of AddURL will have partitioning disabled. We should
  // only add URLs when the unpartitioned table is available.
  if (visitedlink_writer_) {
    visitedlink_writer_->AddURL(url);
  }
}

void ContentVisitDelegate::AddURLs(const std::vector<GURL>& urls) {
  // Not all callers of AddURLs will have partitioning disabled. We should
  // only add URLs when the unpartitioned table is available.
  if (visitedlink_writer_) {
    visitedlink_writer_->AddURLs(urls);
  }
}

void ContentVisitDelegate::DeleteURLs(const std::vector<GURL>& urls) {
  // Not all callers of DeleteURLs will have partitioning disabled. We should
  // only delete URLs when the unpartitioned table is available.
  if (visitedlink_writer_) {
    URLIteratorFromURLs iterator(urls);
    visitedlink_writer_->DeleteURLs(&iterator);
  }
}

void ContentVisitDelegate::DeleteAllURLs() {
  // Not all callers of DeleteAllURLs will have partitioning disabled. We should
  // only delete URLs when the unpartitioned table is available.
  if (visitedlink_writer_) {
    visitedlink_writer_->DeleteAllURLs();
  }
}

void ContentVisitDelegate::AddVisitedLink(const VisitedLink& link) {
  // Not all callers of AddVisitedLink will have partitioning enabled. We should
  // only add visited links when the partitioned table is available.
  if (partitioned_writer_) {
    partitioned_writer_->AddVisitedLink(link);
  }
}

void ContentVisitDelegate::DeleteVisitedLinks(
    const std::vector<VisitedLink>& links) {
  // Not all callers of DeleteVisitedLinks will have partitioning enabled. We
  // should only delete visited links when the partitioned table is available.
  if (partitioned_writer_) {
    VisitedLinkIteratorFromLinks iterator(links);
    partitioned_writer_->DeleteVisitedLinks(&iterator);
  }
}

void ContentVisitDelegate::DeleteAllVisitedLinks() {
  // Not all callers of DeleteAllVisitedLinks will have partitioning enabled. We
  // should only delete visited links when the partitioned table is available.
  if (partitioned_writer_) {
    partitioned_writer_->DeleteAllVisitedLinks();
  }
}

std::optional<uint64_t> ContentVisitDelegate::GetOrAddOriginSalt(
    const url::Origin& origin) {
  // This function should only be called when `kPartitionVisitedLinkDatabase` or
  // `kPartitionVisitedLinkDatabaseWithSelfLinks` is enabled, meaning
  // partitioned_writer_ should be constructed and init.
  if (partitioned_writer_) {
    return partitioned_writer_->GetOrAddOriginSalt(origin);
  }
  return std::nullopt;
}

void ContentVisitDelegate::RebuildTable(
    const scoped_refptr<URLEnumerator>& enumerator) {
  DCHECK(history_service_);
  std::unique_ptr<HistoryDBTask> task(new IterateUrlsDBTask(enumerator));
  history_service_->ScheduleDBTask(FROM_HERE, std::move(task), &task_tracker_);
}

void ContentVisitDelegate::BuildVisitedLinkTable(
    const scoped_refptr<VisitedLinkEnumerator>& enumerator) {
  DCHECK(history_service_);
  std::unique_ptr<HistoryDBTask> task(new IterateVisitedLinkDBTask(enumerator));
  history_service_->ScheduleDBTask(FROM_HERE, std::move(task), &task_tracker_);
}

}  // namespace history
