// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_TETHER_AVAILABILITY_OPERATION_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_TETHER_AVAILABILITY_OPERATION_H_

#include "base/containers/flat_map.h"
#include "chromeos/ash/components/tether/tether_availability_operation.h"

namespace ash::tether {

class FakeTetherAvailabilityOperation : public TetherAvailabilityOperation {
 public:
  class Initializer : public TetherAvailabilityOperation::Initializer {
   public:
    Initializer();
    ~Initializer() override;

    // TetherAvailabilityOperation::Initializer:
    std::unique_ptr<TetherAvailabilityOperation> Initialize(
        const TetherHost& tether_host,
        TetherAvailabilityOperation::
            OnTetherAvailabilityOperationFinishedCallback callback) override;

    void send_result(const TetherHost& tether_host,
                     std::optional<ScannedDeviceInfo> result);
    bool has_active_operation_for_device(const TetherHost& tether_host);

   private:
    void OnOperationDestroyed(const TetherHost& tether_host);

    base::flat_map<std::string,
                   TetherAvailabilityOperation::
                       OnTetherAvailabilityOperationFinishedCallback>
        pending_callbacks_;

    base::WeakPtrFactory<FakeTetherAvailabilityOperation::Initializer>
        weak_ptr_factory_{this};
  };

  FakeTetherAvailabilityOperation(const TetherHost& tether_host,
                                  base::OnceClosure on_destroyed_callback);
  ~FakeTetherAvailabilityOperation() override;

 private:
  base::OnceClosure on_destroyed_callback_;
};

}  // namespace ash::tether

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_TETHER_AVAILABILITY_OPERATION_H_
