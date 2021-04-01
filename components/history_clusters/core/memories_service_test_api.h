// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_MEMORIES_SERVICE_TEST_API_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_MEMORIES_SERVICE_TEST_API_H_

#include <vector>

#include "components/history_clusters/core/memories_service.h"
#include "components/history_clusters/core/visit_data.h"

namespace memories {

class MemoriesServiceTestApi {
 public:
  explicit MemoriesServiceTestApi(MemoriesService* memories_service)
      : memories_service_(memories_service) {}

  std::vector<MemoriesVisit> GetVisits() const {
    return memories_service_->visits_;
  }

  MemoriesService* memories_service_;
};

}  // namespace memories

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_MEMORIES_SERVICE_TEST_API_H_
