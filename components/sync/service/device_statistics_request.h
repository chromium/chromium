// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_DEVICE_STATISTICS_REQUEST_H_
#define COMPONENTS_SYNC_SERVICE_DEVICE_STATISTICS_REQUEST_H_

#include <vector>

#include "base/functional/callback_forward.h"

namespace sync_pb {
class SyncEntity;
}  // namespace sync_pb

namespace syncer {

// Retrieves statistics about the syncing devices for a given account (not
// necessarily the primary account).
class DeviceStatisticsRequest {
 public:
  enum class State { kNotStarted, kInProgress, kComplete, kFailed };

  virtual ~DeviceStatisticsRequest() = default;

  // Tells the request to begin. Must only be called once. `callback` will run
  // once the request completes (i.e. GetState() will be kComplete or kFailed).
  // If the request object gets destroyed before it completes, the callback
  // will not run.
  virtual void Start(base::OnceClosure callback) = 0;

  virtual State GetState() const = 0;
  virtual const std::vector<sync_pb::SyncEntity>& GetResults() const = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_DEVICE_STATISTICS_REQUEST_H_
