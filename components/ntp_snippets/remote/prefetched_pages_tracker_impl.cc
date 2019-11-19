// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/prefetched_pages_tracker_impl.h"

#include <utility>

#include "base/bind.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/page_criteria.h"

using offline_pages::OfflinePageItem;
using offline_pages::OfflinePageModel;
using offline_pages::PageCriteria;

namespace ntp_snippets {

namespace {

bool IsOfflineItemPrefetchedPage(const OfflinePageItem& offline_page_item) {
  return offline_page_item.client_id.name_space ==
         offline_pages::kSuggestedArticlesNamespace;
}

}  // namespace

PrefetchedPagesTrackerImpl::PrefetchedPagesTrackerImpl(
    OfflinePageModel* offline_page_model)
    : initialized_(false), offline_page_model_(offline_page_model) {
  DCHECK(offline_page_model_);
}

PrefetchedPagesTrackerImpl::~PrefetchedPagesTrackerImpl() {
  offline_page_model_->RemoveObserver(this);
}

bool PrefetchedPagesTrackerImpl::IsInitialized() const {
  return initialized_;
}

void PrefetchedPagesTrackerImpl::Initialize(
    base::OnceCallback<void()> callback) {
  if (IsInitialized()) {
    std::move(callback).Run();
  } else {
    initialization_completed_callbacks_.push_back(std::move(callback));
    // The call to get pages might be already in flight, started by previous
    // calls to this method. In this case, there is at least one callback
    // already waiting.
    if (initialization_completed_callbacks_.size() == 1) {
      PageCriteria criteria;
      criteria.client_namespaces =
          std::vector<std::string>{offline_pages::kSuggestedArticlesNamespace};
      offline_page_model_->GetPagesWithCriteria(
          criteria,
          base::BindOnce(&PrefetchedPagesTrackerImpl::OfflinePagesLoaded,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

bool PrefetchedPagesTrackerImpl::PrefetchedOfflinePageExists(
    const GURL& url) const {
  DCHECK(initialized_);
  DCHECK(prefetched_url_counts_.count(url) == 0 ||
         prefetched_url_counts_.find(url)->second > 0);
  // It is enough to check existence of an item (instead of its count), because
  // the mapping does not contain zero counts.
  return prefetched_url_counts_.count(url) == 1;
}

void PrefetchedPagesTrackerImpl::OfflinePageModelLoaded(
    OfflinePageModel* model) {
  // Ignored. Offline Page model delayes our requests until it is loaded.
}

void PrefetchedPagesTrackerImpl::OfflinePageAdded(
    OfflinePageModel* model,
    const OfflinePageItem& added_page) {
  if (IsOfflineItemPrefetchedPage(added_page)) {
    AddOfflinePage(added_page);
  }
}

void PrefetchedPagesTrackerImpl::OfflinePageDeleted(
    const offline_pages::OfflinePageItem& deleted_page) {
  auto offline_id_it = offline_id_to_url_mapping_.find(deleted_page.offline_id);

  if (offline_id_it == offline_id_to_url_mapping_.end()) {
    // We did not know about this page, thus, nothing to delete.
    return;
  }

  auto url_it = prefetched_url_counts_.find(offline_id_it->second);
  DCHECK(url_it != prefetched_url_counts_.end());
  --url_it->second;
  if (url_it->second == 0) {
    prefetched_url_counts_.erase(url_it);
  }
  offline_id_to_url_mapping_.erase(offline_id_it);
}

void PrefetchedPagesTrackerImpl::OfflinePagesLoaded(
    const std::vector<OfflinePageItem>& all_prefetched_offline_pages) {
  for (const OfflinePageItem& item : all_prefetched_offline_pages) {
    DCHECK(IsOfflineItemPrefetchedPage(item));
    AddOfflinePage(item);
  }

  initialized_ = true;
  offline_page_model_->AddObserver(this);
  for (auto& callback : initialization_completed_callbacks_) {
    std::move(callback).Run();
  }
  initialization_completed_callbacks_.clear();
}

void PrefetchedPagesTrackerImpl::AddOfflinePage(
    const OfflinePageItem& offline_page_item) {
  const GURL& url = offline_page_item.GetOriginalUrl();
  DCHECK(prefetched_url_counts_.count(url) == 0 ||
         prefetched_url_counts_.find(url)->second > 0);
  ++prefetched_url_counts_[url];
  offline_id_to_url_mapping_[offline_page_item.offline_id] = url;
}

}  // namespace ntp_snippets
