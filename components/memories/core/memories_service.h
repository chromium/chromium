// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORIES_CORE_MEMORIES_SERVICE_H_
#define COMPONENTS_MEMORIES_CORE_MEMORIES_SERVICE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_context.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/memories/core/memories_remote_model_helper.h"
#include "components/memories/core/visit_data.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace memories {

// This Service is the API for UIs to fetch Chrome Memories.
class MemoriesService : public KeyedService {
 public:
  explicit MemoriesService(
      history::HistoryService* history_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~MemoriesService() override;

  // KeyedService:
  void Shutdown() override;

  // Adds a visit to the memories service for processing into History and
  // memory generation.
  void AddVisit(const MemoriesVisit& visit);

  // Asks |remote_model_helper_| to construct memories from |visits_|.
  void GetMemories(MemoriesCallback callback);

 private:
  // If the Memories flag is enabled, this contains all the visits in-memory
  // during the Profile lifetime.
  // TODO(tommycli): Hide this better behind a new debug flag.
  std::vector<MemoriesVisit> visits_;

  // Helper service to handle communicating with the remote model. This will be
  // used for debugging only; the launch ready feature will use a local model
  // instead.
  std::unique_ptr<MemoriesRemoteModelHelper> remote_model_helper_;

  DISALLOW_COPY_AND_ASSIGN(MemoriesService);
};

}  // namespace memories

#endif  // COMPONENTS_MEMORIES_CORE_MEMORIES_SERVICE_H_
