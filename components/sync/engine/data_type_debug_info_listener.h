// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_DATA_TYPE_DEBUG_INFO_LISTENER_H_
#define COMPONENTS_SYNC_ENGINE_DATA_TYPE_DEBUG_INFO_LISTENER_H_

#include <vector>

#include "base/time/time.h"
#include "components/sync/base/model_type.h"

namespace syncer {

struct DataTypeConfigurationStats {
  DataTypeConfigurationStats();
  DataTypeConfigurationStats(const DataTypeConfigurationStats& other);
  ~DataTypeConfigurationStats();

  // The datatype that was configured.
  ModelType model_type;

  // Waiting time before downloading starts.
  base::TimeDelta download_wait_time;

  // Time spent on downloading data for first-sync data types.
  base::TimeDelta download_time;

  // Types configured before this type.
  ModelTypeSet high_priority_types_configured_before;
  ModelTypeSet same_priority_types_configured_before;
};

// Interface for the sync internals to listen to external sync events.
class DataTypeDebugInfoListener {
 public:
  // Notify the listener that configuration of data types has completed.
  virtual void OnDataTypeConfigureComplete(
      const std::vector<DataTypeConfigurationStats>& configuration_stats) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_DATA_TYPE_DEBUG_INFO_LISTENER_H_
