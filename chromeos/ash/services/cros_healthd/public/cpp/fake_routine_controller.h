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

  mojo::Remote<mojom::RoutineObserver>* GetObserver();

 private:
  mojo::Remote<mojom::RoutineObserver> routine_observer_;
  mojo::Receiver<mojom::RoutineControl> receiver_;
};

}  // namespace ash::cros_healthd

#endif  // CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PUBLIC_CPP_FAKE_ROUTINE_CONTROLLER_H_
