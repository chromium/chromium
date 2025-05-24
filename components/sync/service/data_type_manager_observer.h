// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_DATA_TYPE_MANAGER_OBSERVER_H_
#define COMPONENTS_SYNC_SERVICE_DATA_TYPE_MANAGER_OBSERVER_H_

#include "components/sync/service/data_type_manager.h"

namespace syncer {

// Various data type configuration events can be consumed by observing the
// DataTypeManager through this interface.
class DataTypeManagerObserver {
 public:
  virtual void OnConfigureDone(
      const DataTypeManager::ConfigureResult& result) = 0;
  virtual void OnConfigureStart() = 0;

 protected:
  virtual ~DataTypeManagerObserver() = default;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_DATA_TYPE_MANAGER_OBSERVER_H_
