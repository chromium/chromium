// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_DEVICE_STATISTICS_REQUEST_H_
#define COMPONENTS_SYNC_TEST_FAKE_DEVICE_STATISTICS_REQUEST_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/service/device_statistics_request.h"

namespace syncer {

// A fake implementation of DeviceStatisticsRequest for testing purposes.
class FakeDeviceStatisticsRequest : public DeviceStatisticsRequest {
 public:
  FakeDeviceStatisticsRequest();
  ~FakeDeviceStatisticsRequest() override;

  // DeviceStatisticsRequest:
  void Start(base::OnceClosure callback) override;
  State GetState() const override;
  const std::vector<sync_pb::SyncEntity>& GetResults() const override;

  // Test-specific methods:
  void SimulateSuccess(std::vector<sync_pb::SyncEntity> results);
  void SimulateFailure();

  base::WeakPtr<FakeDeviceStatisticsRequest> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  State state_ = State::kNotStarted;
  base::OnceClosure callback_;
  std::vector<sync_pb::SyncEntity> results_;

  base::WeakPtrFactory<FakeDeviceStatisticsRequest> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FAKE_DEVICE_STATISTICS_REQUEST_H_
