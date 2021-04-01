// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/memories_service.h"

#include <utility>

#include "base/feature_list.h"
#include "components/history_clusters/core/memories_features.h"

namespace memories {

MemoriesService::MemoriesService(
    history::HistoryService* history_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : remote_model_helper_(
          std::make_unique<MemoriesRemoteModelHelper>(url_loader_factory)) {}

MemoriesService::~MemoriesService() = default;

void MemoriesService::Shutdown() {}

MemoriesVisit& MemoriesService::GetIncompleteVisit(int64_t nav_id) {
  DCHECK(HasIncompleteVisit(nav_id));
  return GetOrCreateIncompleteVisit(nav_id);
}

MemoriesVisit& MemoriesService::GetOrCreateIncompleteVisit(int64_t nav_id) {
  return incomplete_visits_[nav_id];
}

bool MemoriesService::HasIncompleteVisit(int64_t nav_id) {
  return incomplete_visits_.count(nav_id);
}

void MemoriesService::CompleteVisitIfReady(int64_t nav_id) {
  auto& visit = GetIncompleteVisit(nav_id);
  DCHECK((visit.status.history_rows && visit.status.navigation_ended) ||
         !visit.status.navigation_end_signals);
  DCHECK(visit.status.expect_ukm_page_end_signals ||
         !visit.status.ukm_page_end_signals);
  if (visit.status.history_rows && visit.status.navigation_end_signals &&
      (visit.status.ukm_page_end_signals ||
       !visit.status.expect_ukm_page_end_signals)) {
    if (base::FeatureList::IsEnabled(memories::kMemories))
      visits_.push_back(visit);
    incomplete_visits_.erase(nav_id);
    // TODO(tommycli/manukh): Persist |visits_| to History, and take out of
    //  in-memory.
  }
}

void MemoriesService::GetMemories(MemoriesCallback callback) {
  if (visits_.empty())
    std::move(callback).Run({});
  else
    remote_model_helper_->GetMemories(visits_, std::move(callback));
}

}  // namespace memories
