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
#include "components/visitedlink/browser/visitedlink_writer.h"
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

}  // namespace

ContentVisitDelegate::ContentVisitDelegate(
    content::BrowserContext* browser_context)
    : history_service_(nullptr),
      visitedlink_writer_(
          new visitedlink::VisitedLinkWriter(browser_context, this, true)) {}

ContentVisitDelegate::~ContentVisitDelegate() {
}

bool ContentVisitDelegate::Init(HistoryService* history_service) {
  DCHECK(history_service);
  history_service_ = history_service;
  return visitedlink_writer_->Init();
}

void ContentVisitDelegate::AddURL(const GURL& url) {
  visitedlink_writer_->AddURL(url);
}

void ContentVisitDelegate::AddURLs(const std::vector<GURL>& urls) {
  visitedlink_writer_->AddURLs(urls);
}

void ContentVisitDelegate::DeleteURLs(const std::vector<GURL>& urls) {
  URLIteratorFromURLs iterator(urls);
  visitedlink_writer_->DeleteURLs(&iterator);
}

void ContentVisitDelegate::DeleteAllURLs() {
  visitedlink_writer_->DeleteAllURLs();
}

void ContentVisitDelegate::RebuildTable(
    const scoped_refptr<URLEnumerator>& enumerator) {
  DCHECK(history_service_);
  std::unique_ptr<HistoryDBTask> task(new IterateUrlsDBTask(enumerator));
  history_service_->ScheduleDBTask(FROM_HERE, std::move(task), &task_tracker_);
}

}  // namespace history
