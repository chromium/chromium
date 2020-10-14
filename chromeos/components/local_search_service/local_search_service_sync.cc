// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/local_search_service/local_search_service_sync.h"

#include <utility>

#include "chromeos/components/local_search_service/inverted_index_search.h"
#include "chromeos/components/local_search_service/linear_map_search.h"

namespace chromeos {
namespace local_search_service {

LocalSearchServiceSync::LocalSearchServiceSync() = default;

LocalSearchServiceSync::~LocalSearchServiceSync() = default;

IndexSync* LocalSearchServiceSync::GetIndexSync(IndexId index_id,
                                                Backend backend,
                                                PrefService* local_state) {
  auto it = indices_.find(index_id);
  if (it == indices_.end()) {
    switch (backend) {
      case Backend::kLinearMap:
        it = indices_
                 .emplace(index_id, std::make_unique<LinearMapSearch>(
                                        index_id, local_state))
                 .first;
        break;
      case Backend::kInvertedIndex:
        it = indices_
                 .emplace(index_id, std::make_unique<InvertedIndexSearch>(
                                        index_id, local_state))
                 .first;
    }
  }
  DCHECK(it != indices_.end());
  DCHECK(it->second);

  return it->second.get();
}

}  // namespace local_search_service
}  // namespace chromeos
