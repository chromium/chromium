// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cellular_setup/cellular_setup_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/services/cellular_setup/ota_activator_impl.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace ash::cellular_setup {

// static
void CellularSetupImpl::CreateAndBindToReciever(
    mojo::PendingReceiver<mojom::CellularSetup> receiver) {
  mojo::MakeSelfOwnedReceiver(base::WrapUnique(new CellularSetupImpl()),
                              std::move(receiver));
}

CellularSetupImpl::CellularSetupImpl() = default;

CellularSetupImpl::~CellularSetupImpl() = default;

void CellularSetupImpl::StartActivation(
    mojo::PendingRemote<mojom::ActivationDelegate> delegate,
    StartActivationCallback callback) {
  size_t request_id = next_request_id_;
  ++next_request_id_;

  NetworkHandler* network_handler = NetworkHandler::Get();
  std::unique_ptr<OtaActivator> ota_activator =
      OtaActivatorImpl::Factory::Create(
          std::move(delegate),
          base::BindOnce(&CellularSetupImpl::OnActivationAttemptFinished,
                         weak_ptr_factory_.GetWeakPtr(), request_id),
          network_handler->network_state_handler(),
          network_handler->network_connection_handler(),
          network_handler->network_activation_handler());

  std::move(callback).Run(ota_activator->GenerateRemote());

  // Store the OtaActivator instance in a map indexed by request ID; once the
  // attempt has finished, the map entry will be deleted in
  // OnActivationAttemptFinished().
  ota_activator_map_.AddWithID(std::move(ota_activator), request_id);
}

void CellularSetupImpl::OnActivationAttemptFinished(size_t request_id) {
  ota_activator_map_.Remove(request_id);
}

}  // namespace ash::cellular_setup
