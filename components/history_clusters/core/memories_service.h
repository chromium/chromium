// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_MEMORIES_SERVICE_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_MEMORIES_SERVICE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "components/history/core/browser/history_service.h"
#include "components/history_clusters/core/memories_remote_model_helper.h"
#include "components/history_clusters/core/visit_data.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

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

  // TODO(manukh) |MemoriesService| should be responsible for constructing the
  //  MemoriesVisit rather than exposing these methods which are used by
  //  |HistoryClustersTabHelper| to construct the visits.
  // Gets an incomplete visit after DCHECKing it exists; this saves the call
  // sites the effort.
  MemoriesVisit& GetIncompleteVisit(int64_t nav_id);
  // Gets or creates an incomplete visit.
  MemoriesVisit& GetOrCreateIncompleteVisit(int64_t nav_id);
  // Returns whether an incomplete visit exists.
  // TODO(manukh): Merge |HasIncompleteVisit()| and |GetIncompleteVisit()|.
  bool HasIncompleteVisit(int64_t nav_id);
  // Completes the visit if the expected metrics have been recorded. Incomplete
  // visit references retrieved prior will no longer be valid.
  void CompleteVisitIfReady(int64_t nav_id);

  // Asks |remote_model_helper_| to construct memories from |visits_|.
  void GetMemories(MemoriesCallback callback);

 private:
  friend class MemoriesServiceTestApi;

  // If the Memories flag is enabled, this contains all the visits in-memory
  // during the Profile lifetime.
  // TODO(tommycli): Hide this better behind a new debug flag.
  std::vector<MemoriesVisit> visits_;
  // A visit is constructed stepwise. Visits are initially placed in
  // |incomplete_visits_| and moved to |visits_| once completed.
  std::map<int64_t, MemoriesVisit> incomplete_visits_;

  // Helper service to handle communicating with the remote model. This will be
  // used for debugging only; the launch ready feature will use a local model
  // instead.
  std::unique_ptr<MemoriesRemoteModelHelper> remote_model_helper_;

  DISALLOW_COPY_AND_ASSIGN(MemoriesService);
};

}  // namespace memories

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_MEMORIES_SERVICE_H_
