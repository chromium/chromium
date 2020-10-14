// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_SYNC_H_
#define CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_SYNC_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "chromeos/components/local_search_service/shared_structs.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace chromeos {

namespace local_search_service {

class IndexSync;

// LocalSearchServiceSync creates and owns content-specific Indices. Clients can
// call it |GetIndexSync| method to get an IndexSync for a given index id.
class LocalSearchServiceSync : public KeyedService {
 public:
  LocalSearchServiceSync();
  ~LocalSearchServiceSync() override;
  LocalSearchServiceSync(const LocalSearchServiceSync&) = delete;
  LocalSearchServiceSync& operator=(const LocalSearchServiceSync&) = delete;

  IndexSync* GetIndexSync(IndexId index_id,
                          Backend backend,
                          PrefService* local_state);

 private:
  std::map<IndexId, std::unique_ptr<IndexSync>> indices_;
};

}  // namespace local_search_service
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_SYNC_H_
