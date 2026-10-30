// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/local_search_service/local_search_service.h"

#include <memory>
#include <optional>
#include <utility>

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
  auto [it, inserted] = indices_.try_emplace(index_id);
  if (inserted) {
    switch (backend) {
      case Backend::kLinearMap:
        it->second = std::make_unique<LinearMapSearch>(index_id);
        break;
      case Backend::kInvertedIndex:
        it->second = std::make_unique<InvertedIndexSearch>(index_id);
        break;
    }
    if (!it->second) {
      indices_.erase(it);
      std::move(callback).Run("Error creating an Index.");
      return;
    }
    if (reporter_remote) {
      it->second->SetReporterRemote(std::move(reporter_remote));
    }
  }

  it->second->BindReceiver(std::move(index_receiver));
  std::move(callback).Run(std::nullopt);
}

}  // namespace ash::local_search_service
