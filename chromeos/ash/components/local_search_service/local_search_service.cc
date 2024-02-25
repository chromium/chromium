// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/local_search_service/local_search_service.h"
#include "chromeos/ash/components/local_search_service/inverted_index_search.h"
#include "chromeos/ash/components/local_search_service/linear_map_search.h"

namespace ash::local_search_service {

LocalSearchService::LocalSearchService(
    mojo::PendingReceiver<mojom::LocalSearchService> receiver)
    : receiver_(this, std::move(receiver)) {}
LocalSearchService::~LocalSearchService() = default;

void LocalSearchService::BindIndex(
    IndexId index_id,
    Backend backend,
    mojo::PendingReceiver<mojom::Index> index_receiver,
    mojo::PendingRemote<mojom::SearchMetricsReporter> reporter_remote,
    LocalSearchService::BindIndexCallback callback) {
  auto it = indices_.find(index_id);
  if (it == indices_.end()) {
    switch (backend) {
      case Backend::kLinearMap:
        it = indices_
                 .emplace(index_id, std::make_unique<LinearMapSearch>(index_id))
                 .first;
        break;
      case Backend::kInvertedIndex:
        it = indices_
                 .emplace(index_id,
                          std::make_unique<InvertedIndexSearch>(index_id))
                 .first;
    }
    if (!it->second) {
      std::move(callback).Run("Returned Index is null.");
      return;
    }
    if (reporter_remote)
      it->second->SetReporterRemote(std::move(reporter_remote));
  }

  if (it == indices_.end()) {
    std::move(callback).Run("Error creating an Index.");
    return;
  }

  it->second->BindReceiver(std::move(index_receiver));
  std::move(callback).Run(std::nullopt);
}

}  // namespace ash::local_search_service
