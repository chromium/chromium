// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_service.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/uuid.h"
#include "components/dom_distiller/core/distilled_content_store.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "url/gurl.h"

namespace dom_distiller {

namespace {

ArticleEntry CreateSkeletonEntryForUrl(const GURL& url) {
  ArticleEntry skeleton;
  skeleton.entry_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  skeleton.pages.push_back(url);

  DCHECK(IsEntryValid(skeleton));
  return skeleton;
}

}  // namespace

DomDistillerService::DomDistillerService(
    std::unique_ptr<DistillerFactory> distiller_factory,
    std::unique_ptr<DistillerPageFactory> distiller_page_factory,
    std::unique_ptr<DistilledPagePrefs> distilled_page_prefs,
    std::unique_ptr<DistillerUIHandle> distiller_ui_handle)
    : content_store_(new InMemoryContentStore(kDefaultMaxNumCachedEntries)),
      distiller_factory_(std::move(distiller_factory)),
      distiller_page_factory_(std::move(distiller_page_factory)),
      distilled_page_prefs_(std::move(distilled_page_prefs)),
      distiller_ui_handle_(std::move(distiller_ui_handle)),
      weak_ptr_factory_(this) {}

DomDistillerService::~DomDistillerService() {
  // There shouldn't be any tasks pending at this point.
  DCHECK(tasks_.empty());
}

std::unique_ptr<DistillerPage> DomDistillerService::CreateDefaultDistillerPage(
    const gfx::Size& render_view_size) {
  return distiller_page_factory_->CreateDistillerPage(render_view_size);
}

std::unique_ptr<DistillerPage>
DomDistillerService::CreateDefaultDistillerPageWithHandle(
    std::unique_ptr<SourcePageHandle> handle) {
  return distiller_page_factory_->CreateDistillerPageWithHandle(
      std::move(handle));
}

std::unique_ptr<ViewerHandle> DomDistillerService::ViewUrl(
    ViewRequestDelegate* delegate,
    std::unique_ptr<DistillerPage> distiller_page,
    const GURL& url) {
  if (!url.is_valid()) {
    return nullptr;
  }

  TaskTracker* task_tracker = nullptr;
  bool was_created = GetOrCreateTaskTrackerForUrl(url, &task_tracker);
  std::unique_ptr<ViewerHandle> viewer_handle =
      task_tracker->AddViewer(delegate);
  // If a distiller is already running for one URL, don't start another.
  if (was_created) {
    task_tracker->StartDistiller(distiller_factory_.get(),
                                 std::move(distiller_page));
    task_tracker->StartBlobFetcher();
  }

  return viewer_handle;
}

bool DomDistillerService::GetOrCreateTaskTrackerForUrl(
    const GURL& url,
    TaskTracker** task_tracker) {
  *task_tracker = GetTaskTrackerForUrl(url);
  if (*task_tracker) {
    return false;
  }

  ArticleEntry skeleton_entry = CreateSkeletonEntryForUrl(url);
  *task_tracker = CreateTaskTracker(skeleton_entry);
  return true;
}

TaskTracker* DomDistillerService::GetTaskTrackerForUrl(const GURL& url) const {
  for (auto it = tasks_.begin(); it != tasks_.end(); ++it) {
    if ((*it)->HasUrl(url)) {
      return (*it).get();
    }
  }
  return nullptr;
}

TaskTracker* DomDistillerService::CreateTaskTracker(const ArticleEntry& entry) {
  TaskTracker::CancelCallback cancel_callback =
      base::BindOnce(&DomDistillerService::CancelTask, base::Unretained(this));
  tasks_.push_back(std::make_unique<TaskTracker>(
      entry, std::move(cancel_callback), content_store_.get()));
  return tasks_.back().get();
}

void DomDistillerService::CancelTask(TaskTracker* task) {
  auto it =
      base::ranges::find(tasks_, task, &std::unique_ptr<TaskTracker>::get);
  if (it != tasks_.end()) {
    it->release();
    tasks_.erase(it);
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                  task);
  }
}

DistilledPagePrefs* DomDistillerService::GetDistilledPagePrefs() {
  return distilled_page_prefs_.get();
}

DistillerUIHandle* DomDistillerService::GetDistillerUIHandle() {
  return distiller_ui_handle_.get();
}

base::WeakPtr<DomDistillerService> DomDistillerService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool DomDistillerService::HasTaskTrackerForTesting(const GURL& url) const {
  return GetTaskTrackerForUrl(url);
}

}  // namespace dom_distiller
