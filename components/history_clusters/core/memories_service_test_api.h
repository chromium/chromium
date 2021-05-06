// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_MEMORIES_SERVICE_TEST_API_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_MEMORIES_SERVICE_TEST_API_H_

#include <vector>

#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/memories_service.h"

namespace history_clusters {

class MemoriesServiceTestApi {
 public:
  explicit MemoriesServiceTestApi(MemoriesService* memories_service)
      : memories_service_(memories_service) {}

  std::vector<history::AnnotatedVisit> GetVisits() const {
    return memories_service_->visits_;
  }

  MemoriesService* memories_service_;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_MEMORIES_SERVICE_TEST_API_H_
