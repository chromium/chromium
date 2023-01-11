// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_OTA_ACTIVATOR_H_
#define CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_OTA_ACTIVATOR_H_

#include "base/functional/callback.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/cellular_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::cellular_setup {

// Activates a cellular SIM using the OTA mechanism. This class makes a single
// attempt at activation, then fires a callback on completion, regardless of
// success or failure. An OtaActivator object can only be used for one
// attempt; to perform a new activation attempt, use a separate OtaActivator
// instance.
class OtaActivator : public mojom::CarrierPortalHandler {
 public:
  OtaActivator(const OtaActivator&) = delete;
  OtaActivator& operator=(const OtaActivator&) = delete;

  ~OtaActivator() override;

  // Generates a mojo::PendingRemote<> bound to this instance. Only one
  // mojo::PendingRemote<> may be bound to a single OtaActivator instance, so
  // this function can only be called once.
  mojo::PendingRemote<mojom::CarrierPortalHandler> GenerateRemote();

 protected:
  explicit OtaActivator(base::OnceClosure on_finished_callback);

  void InvokeOnFinishedCallback();

  base::OnceClosure on_finished_callback_;
  mojo::Receiver<mojom::CarrierPortalHandler> receiver_{this};
};

}  // namespace ash::cellular_setup

#endif  // CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_OTA_ACTIVATOR_H_
