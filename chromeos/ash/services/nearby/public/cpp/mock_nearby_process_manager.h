// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_MOCK_NEARBY_PROCESS_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_MOCK_NEARBY_PROCESS_MANAGER_H_

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "chromeos/ash/services/nearby/public/cpp/nearby_process_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace nearby {

class MockNearbyProcessManager : public NearbyProcessManager {
 public:
  class MockNearbyProcessReference : public NearbyProcessReference {
   public:
    MockNearbyProcessReference();
    MockNearbyProcessReference(const MockNearbyProcessReference&) = delete;
    MockNearbyProcessReference& operator=(const MockNearbyProcessReference&) =
        delete;
    ~MockNearbyProcessReference() override;

    MOCK_METHOD(const mojo::SharedRemote<
                    ::nearby::connections::mojom::NearbyConnections>&,
                GetNearbyConnections,
                (),
                (const, override));

    MOCK_METHOD(const mojo::SharedRemote<
                    ::ash::nearby::presence::mojom::NearbyPresence>&,
                GetNearbyPresence,
                (),
                (const, override));

    MOCK_METHOD(
        const mojo::SharedRemote<::sharing::mojom::NearbySharingDecoder>&,
        GetNearbySharingDecoder,
        (),
        (const, override));

    MOCK_METHOD(
        const mojo::SharedRemote<ash::quick_start::mojom::QuickStartDecoder>&,
        GetQuickStartDecoder,
        (),
        (const, override));
  };

  MockNearbyProcessManager();
  MockNearbyProcessManager(const MockNearbyProcessManager&) = delete;
  MockNearbyProcessManager& operator=(const MockNearbyProcessManager&) = delete;
  ~MockNearbyProcessManager() override;

  MOCK_METHOD(std::unique_ptr<NearbyProcessReference>,
              GetNearbyProcessReference,
              (NearbyProcessStoppedCallback on_process_stopped_callback),
              (override));

  MOCK_METHOD(void, ShutDownProcess, (), (override));
};

}  // namespace nearby
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_MOCK_NEARBY_PROCESS_MANAGER_H_
