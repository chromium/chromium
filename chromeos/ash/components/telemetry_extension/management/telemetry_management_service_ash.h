// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_MANAGEMENT_TELEMETRY_MANAGEMENT_SERVICE_ASH_H_
#define CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_MANAGEMENT_TELEMETRY_MANAGEMENT_SERVICE_ASH_H_

#include <memory>

#include "chromeos/crosapi/mojom/telemetry_management_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// Implementation of the `TelemetryManagementService`, as part of the Telemetry
// Extension, allowing to control specific system configurations.
class TelemetryManagementServiceAsh
    : public crosapi::mojom::TelemetryManagementService {
 public:
  // Factory for creating instances of `TelemetryManagementServiceAsh`.
  // Provides a method for setting a testing instance.
  class Factory {
   public:
    static std::unique_ptr<crosapi::mojom::TelemetryManagementService> Create(
        mojo::PendingReceiver<crosapi::mojom::TelemetryManagementService>
            receiver);

    static void SetForTesting(Factory* test_factory);

    virtual ~Factory();

   protected:
    virtual std::unique_ptr<crosapi::mojom::TelemetryManagementService>
    CreateInstance(
        mojo::PendingReceiver<crosapi::mojom::TelemetryManagementService>
            receiver) = 0;

   private:
    static Factory* test_factory_;
  };

  TelemetryManagementServiceAsh();
  TelemetryManagementServiceAsh(const TelemetryManagementServiceAsh&) = delete;
  TelemetryManagementServiceAsh& operator=(
      const TelemetryManagementServiceAsh&) = delete;
  ~TelemetryManagementServiceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::TelemetryManagementService>
          receiver);

 private:
  // crosapi::mojom::TelemetryManagementService:
  void SetAudioGain(uint64_t node_id,
                    int32_t gain,
                    SetAudioGainCallback callback) override;
  void SetAudioVolume(uint64_t node_id,
                      int32_t volume,
                      bool is_muted,
                      SetAudioVolumeCallback callback) override;

  // Support any number of connections.
  mojo::ReceiverSet<crosapi::mojom::TelemetryManagementService> receivers_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_MANAGEMENT_TELEMETRY_MANAGEMENT_SERVICE_ASH_H_
