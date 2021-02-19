// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memories/core/memories_service.h"
#include "base/feature_list.h"
#include "components/memories/core/memories_features.h"

namespace memories {

MemoriesService::MemoriesService() = default;

MemoriesService::~MemoriesService() = default;

void MemoriesService::Shutdown() {}

void MemoriesService::AddVisit(const GURL& url,
                               const base::Time& time,
                               const VisitContextSignals& context_signals) {
  if (!base::FeatureList::IsEnabled(memories::kMemories))
    return;

  MemoriesVisit visit;
  visit.url = url;
  visit.visit_time = time;
  visit.context_signals = context_signals;

  // TODO(tommycli/manukh): It sure seems like we need to get the History
  // visit_id. Probably we need to plumb the navigation ID from the caller of
  // this function and ask HistoryService.
  visits_.push_back(visit);

  // TODO(tommycli/manukh): Persist to History, and take out of in-memory.
}

}  // namespace memories
