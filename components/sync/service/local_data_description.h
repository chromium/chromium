// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_LOCAL_DATA_DESCRIPTION_H_
#define COMPONENTS_SYNC_SERVICE_LOCAL_DATA_DESCRIPTION_H_

#include <string>
#include <vector>

#include "components/sync/base/model_type.h"

namespace syncer {

struct LocalDataDescription {
  ModelType type = syncer::UNSPECIFIED;
  // Count of local items.
  size_t item_count = 0;
  // Preferably contains up to 3 items for preview.
  std::vector<std::string> item_preview;

  LocalDataDescription();
  LocalDataDescription(ModelType type,
                       size_t item_count,
                       const std::vector<std::string>& item_preview);
  LocalDataDescription(const LocalDataDescription&);
  LocalDataDescription& operator=(const LocalDataDescription&);
  LocalDataDescription(LocalDataDescription&&);
  LocalDataDescription& operator=(LocalDataDescription&&);
  ~LocalDataDescription();
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_LOCAL_DATA_DESCRIPTION_H_
