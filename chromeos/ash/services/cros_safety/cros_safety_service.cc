// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cros_safety/cros_safety_service.h"

#include "base/memory/singleton.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "chromeos/ash/experiences/arc/arc_util.h"
#include "chromeos/ash/experiences/arc/session/arc_bridge_service.h"
#include "chromeos/ash/experiences/arc/session/arc_service_manager.h"
#include "cloud_safety_session.h"
#include "components/user_manager/user_manager.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

namespace ash {

CrosSafetyService::CrosSafetyService(
    raw_ptr<manta::MantaService> manta_service) {
  CHECK(mojo_service_manager::IsServiceManagerBound())
      << "CrosSafetyService requires mojo service manager.";
  mojo_service_manager::GetServiceManagerProxy()->Register(
      chromeos::mojo_services::kCrosSafetyService,
      provider_receiver_.BindNewPipeAndPassRemote());

  if (manta_service) {
    auto provider = manta_service->CreateWalrusProvider();
    if (provider) {
      cloud_safety_session_ =
          std::make_unique<CloudSafetySession>(std::move(provider));
    }
  }
}

CrosSafetyService::~CrosSafetyService() = default;

void CrosSafetyService::BindReceiver(
    mojo::PendingReceiver<cros_safety::mojom::CrosSafetyService> receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

void CrosSafetyService::Request(
    chromeos::mojo_service_manager::mojom::ProcessIdentityPtr identity,
    mojo::ScopedMessagePipeHandle receiver) {
  BindReceiver(mojo::PendingReceiver<cros_safety::mojom::CrosSafetyService>(
      std::move(receiver)));
}

void CrosSafetyService::GetArcSafetySessionComplete(
    CreateOnDeviceSafetySessionCallback callback,
    arc::mojom::GetArcSafetySessionResult result) {
  switch (result) {
    case arc::mojom::GetArcSafetySessionResult::kOk:
      std::move(callback).Run(
          cros_safety::mojom::GetOnDeviceSafetySessionResult::kOk);
      break;
    case arc::mojom::GetArcSafetySessionResult::kSafetyServiceNotFound:
      std::move(callback).Run(
          cros_safety::mojom::GetOnDeviceSafetySessionResult::
              kHadesNotAvailable);
      break;
    case arc::mojom::GetArcSafetySessionResult::kBindSafetyServiceError:
      std::move(callback).Run(
          cros_safety::mojom::GetOnDeviceSafetySessionResult::kHadesNotReady);
      break;
    case arc::mojom::GetArcSafetySessionResult::kServiceNotAvailable:
      std::move(callback).Run(
          cros_safety::mojom::GetOnDeviceSafetySessionResult::
              kArcSafetyServiceNotAvailable);
      break;
    default:
      LOG(ERROR) << "Unknown GetArcSafetySessionResult: " << result;
      std::move(callback).Run(
          cros_safety::mojom::GetOnDeviceSafetySessionResult::kGenericError);
  }
}

void CrosSafetyService::CreateOnDeviceSafetySession(
    mojo::PendingReceiver<cros_safety::mojom::OnDeviceSafetySession> session,
    CreateOnDeviceSafetySessionCallback callback) {
  // ARC is only available under primary user session.
  if (!user_manager::UserManager::Get()->IsPrimaryUser(
          user_manager::UserManager::Get()->GetActiveUser()) ||
      !arc::IsArcAvailable() || !arc::ArcServiceManager::Get()) {
    // TODO(crbug.com/379073760) Separate kArcDisabledByUser cases so we can
    // inform the user to enable arc.
    std::move(callback).Run(
        cros_safety::mojom::GetOnDeviceSafetySessionResult::kArcNotAllowed);
    return;
  }

  if (!arc::ArcServiceManager::Get()
           ->arc_bridge_service()
           ->on_device_safety()
           ->IsConnected()) {
    std::move(callback).Run(cros_safety::mojom::GetOnDeviceSafetySessionResult::
                                kOnDeviceSafetyNotConnected);
    return;
  }

  auto* on_device_safety_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc::ArcServiceManager::Get()->arc_bridge_service()->on_device_safety(),
      GetArcSafetySession);

  if (!on_device_safety_instance) {
    std::move(callback).Run(cros_safety::mojom::GetOnDeviceSafetySessionResult::
                                kArcGetInstanceForMethodFailed);
    return;
  }

  on_device_safety_instance->GetArcSafetySession(
      std::move(session),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&CrosSafetyService::GetArcSafetySessionComplete,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
          arc::mojom::GetArcSafetySessionResult::kServiceNotAvailable));
}

void CrosSafetyService::CreateCloudSafetySession(
    mojo::PendingReceiver<cros_safety::mojom::CloudSafetySession> session,
    CreateCloudSafetySessionCallback callback) {
  if (!cloud_safety_session_) {
    std::move(callback).Run(cros_safety::mojom::GetCloudSafetySessionResult::
                                kMantaServiceFailedToCreate);
    return;
  }

  cloud_safety_session_->AddReceiver(std::move(session));
  std::move(callback).Run(cros_safety::mojom::GetCloudSafetySessionResult::kOk);
}

}  // namespace ash
