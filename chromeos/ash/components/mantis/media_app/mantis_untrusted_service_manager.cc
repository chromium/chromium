// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/mantis/media_app/mantis_untrusted_service_manager.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/mantis/media_app/mantis_untrusted_service.h"
#include "chromeos/ash/components/mantis/mojom/mantis_service.mojom.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "chromeos/ash/components/mojo_service_manager/mojom/mojo_service_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

namespace ash {

MantisUntrustedServiceManager::MantisUntrustedServiceManager() {
  ash::mojo_service_manager::GetServiceManagerProxy()->Request(
      chromeos::mojo_services::kCrosMantisService, std::nullopt,
      cros_service_.BindNewPipeAndPassReceiver().PassPipe());
  cros_service_.reset_on_disconnect();
}

MantisUntrustedServiceManager::~MantisUntrustedServiceManager() = default;

void MantisUntrustedServiceManager::OnQueryDone(
    base::OnceCallback<void(bool)> callback,
    chromeos::mojo_service_manager::mojom::ErrorOrServiceStatePtr result) {
  if (!result->is_state() || !result->get_state()->is_registered_state()) {
    std::move(callback).Run(false);
    return;
  }
  cros_service_->GetMantisFeatureStatus(base::BindOnce(
      [](base::OnceCallback<void(bool)> callback,
         mantis::mojom::MantisFeatureStatus status) {
        std::move(callback).Run(status ==
                                mantis::mojom::MantisFeatureStatus::kAvailable);
      },
      std::move(callback)));
}

void MantisUntrustedServiceManager::IsAvailable(
    base::OnceCallback<void(bool)> callback) {
  if (switches::IsMantisSecretKeyMatched()) {
    std::move(callback).Run(true);
    return;
  }

  // TODO(crbug.com/362993438): Check admin console policy, age restriction, and
  // region restriction.

  // Query kCrosMantisService first since it might not be available on every
  // devices.
  ash::mojo_service_manager::GetServiceManagerProxy()->Query(
      chromeos::mojo_services::kCrosMantisService,
      base::BindOnce(&MantisUntrustedServiceManager::OnQueryDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void MantisUntrustedServiceManager::Create(CreateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo::PendingRemote<mantis::mojom::MantisProcessor> processor;
  // This API is designed by CrOS service to handle multiple calls safely.
  cros_service_->Initialize(
      // TODO(crbug.com/378333373): Handle progress observer.
      /*progress_observer=*/mojo::NullRemote(),
      processor.InitWithNewPipeAndPassReceiver(),
      base::BindOnce(&MantisUntrustedServiceManager::OnInitializeDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(processor)));
}

void MantisUntrustedServiceManager::OnInitializeDone(
    CreateCallback callback,
    mojo::PendingRemote<mantis::mojom::MantisProcessor> processor,
    mantis::mojom::InitializeResult result) {
  if (result != mantis::mojom::InitializeResult::kSuccess) {
    std::move(callback).Run(CreateResult::NewError(result));
    return;
  }
  mantis_untrusted_service_ =
      std::make_unique<MantisUntrustedService>(std::move(processor));
  std::move(callback).Run(CreateResult::NewService(
      mantis_untrusted_service_->BindNewPipeAndPassRemote()));
}

}  // namespace ash
