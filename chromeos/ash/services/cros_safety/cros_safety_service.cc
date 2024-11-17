// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cros_safety/cros_safety_service.h"

#include "base/memory/singleton.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "cloud_safety_session.h"
#include "components/user_manager/user_manager.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

namespace ash {

CrosSafetyService::CrosSafetyService(manta::MantaService* manta_service)
    : manta_service_(manta_service) {
  CHECK(manta_service_);
  CHECK(mojo_service_manager::IsServiceManagerBound())
      << "CrosSafetyService requires mojo service manager.";
  mojo_service_manager::GetServiceManagerProxy()->Register(
      chromeos::mojo_services::kCrosSafetyService,
      provider_receiver_.BindNewPipeAndPassRemote());
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

void CrosSafetyService::CreateOnDeviceSafetySession(
    mojo::PendingReceiver<cros_safety::mojom::OnDeviceSafetySession> session,
    CreateOnDeviceSafetySessionCallback callback) {
  NOTIMPLEMENTED();
}

void CrosSafetyService::CreateCloudSafetySession(
    mojo::PendingReceiver<cros_safety::mojom::CloudSafetySession> session,
    CreateCloudSafetySessionCallback callback) {
  // initialize cloud_safety_session_ if not previously done.
  if (!cloud_safety_session_) {
    auto provider = manta_service_->CreateWalrusProvider();
    if (!provider) {
      std::move(callback).Run(cros_safety::mojom::GetCloudSafetySessionResult::
                                  kMantaServiceFailedToCreate);
      return;
    }

    cloud_safety_session_ =
        std::make_unique<CloudSafetySession>(std::move(provider));
  }

  cloud_safety_session_->AddReceiver(std::move(session));
  std::move(callback).Run(cros_safety::mojom::GetCloudSafetySessionResult::kOk);
}

}  // namespace ash
