// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PUBLIC_CPP_FAKE_ROUTINE_CONTROLLER_H_
#define CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PUBLIC_CPP_FAKE_ROUTINE_CONTROLLER_H_

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::cros_healthd {

class FakeRoutineController : public mojom::RoutineControl {
 public:
  explicit FakeRoutineController(
      mojo::PendingReceiver<mojom::RoutineControl> pending_receiver,
      mojo::PendingRemote<mojom::RoutineObserver> observer);
  ~FakeRoutineController() override;

  FakeRoutineController(const FakeRoutineController&) = delete;
  FakeRoutineController& operator=(const FakeRoutineController&) = delete;

  // `RoutineControl`:
  void GetState(GetStateCallback callback) override;
  void Start() override;

  void SetGetStateResponse(mojom::RoutineStatePtr& state);
  bool has_start_been_called() { return start_called_; }

  mojo::Remote<mojom::RoutineObserver>* GetObserver();
  mojo::Receiver<mojom::RoutineControl>* GetReceiver();

 private:
  // Returned on a call to `GetState`.
  mojom::RoutineStatePtr get_state_response_{mojom::RoutineState::New()};
  // Set to true when `Start` is called.
  bool start_called_ = false;

  mojo::Remote<mojom::RoutineObserver> routine_observer_;
  mojo::Receiver<mojom::RoutineControl> receiver_;
};

}  // namespace ash::cros_healthd

#endif  // CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PUBLIC_CPP_FAKE_ROUTINE_CONTROLLER_H_
