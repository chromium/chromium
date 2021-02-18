// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORIES_CORE_MEMORIES_SERVICE_H_
#define COMPONENTS_MEMORIES_CORE_MEMORIES_SERVICE_H_

#include <vector>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/memories/core/visit_data.h"

namespace memories {

// This Service is the API for UIs to fetch Chrome Memories.
class MemoriesService : public KeyedService {
 public:
  MemoriesService();
  ~MemoriesService() override;

  // KeyedService:
  void Shutdown() override;

  // Adds a visit to the memories service for processing into History and
  // memory generation.
  void AddVisit(const GURL& url,
                const base::Time& time,
                const VisitContextSignals& context_signals);

 private:
  // If the Memories flag is enabled, this contains all the visits in-memory
  // during the Profile lifetime.
  // TODO(tommycli): Hide this better behind a new debug flag.
  std::vector<MemoriesVisit> visits_;

  DISALLOW_COPY_AND_ASSIGN(MemoriesService);
};

}  // namespace memories

#endif  // COMPONENTS_MEMORIES_CORE_MEMORIES_SERVICE_H_
