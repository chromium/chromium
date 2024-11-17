// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CROS_SAFETY_CROS_SAFETY_SERVICE_H_
#define CHROMEOS_ASH_SERVICES_CROS_SAFETY_CROS_SAFETY_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/mojo_service_manager/mojom/mojo_service_manager.mojom.h"
#include "chromeos/ash/services/cros_safety/cloud_safety_session.h"
#include "chromeos/ash/services/cros_safety/public/mojom/cros_safety.mojom.h"
#include "chromeos/ash/services/cros_safety/public/mojom/cros_safety_service.mojom.h"
#include "components/manta/manta_service.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// CrosSafetyService is the entry point for doing trust and safety filtering for
// ChromeOS on-device features. It registers itself to the mojo service manager
// so that platform clients can request CrosSafetyService as well.
class CrosSafetyService
    : public cros_safety::mojom::CrosSafetyService,
      public chromeos::mojo_service_manager::mojom::ServiceProvider {
 public:
  using CreateOnDeviceSafetySessionCallback = base::OnceCallback<void(
      cros_safety::mojom::GetOnDeviceSafetySessionResult)>;
  using CreateCloudSafetySessionCallback =
      base::OnceCallback<void(cros_safety::mojom::GetCloudSafetySessionResult)>;

  explicit CrosSafetyService(manta::MantaService* manta_service);
  CrosSafetyService(const CrosSafetyService&) = delete;
  CrosSafetyService& operator=(const CrosSafetyService&) = delete;
  ~CrosSafetyService() override;

  // Binds this instance to |receiver|.
  void BindReceiver(
      mojo::PendingReceiver<cros_safety::mojom::CrosSafetyService> receiver);

  // cros_safety::mojom::CrosSafetyService overrides
  void CreateOnDeviceSafetySession(
      mojo::PendingReceiver<cros_safety::mojom::OnDeviceSafetySession> session,
      CreateOnDeviceSafetySessionCallback callback) override;
  void CreateCloudSafetySession(
      mojo::PendingReceiver<cros_safety::mojom::CloudSafetySession> session,
      CreateCloudSafetySessionCallback callback) override;

 private:
  // chromeos::mojo_service_manager::mojom::ServiceProvider overrides.
  void Request(
      chromeos::mojo_service_manager::mojom::ProcessIdentityPtr identity,
      mojo::ScopedMessagePipeHandle receiver) override;

  const raw_ptr<manta::MantaService> manta_service_;
  std::unique_ptr<CloudSafetySession> cloud_safety_session_;

  // Receiver for mojo service manager service provider.
  mojo::Receiver<chromeos::mojo_service_manager::mojom::ServiceProvider>
      provider_receiver_{this};

  // Receivers for external CrosSafetyService requests.
  mojo::ReceiverSet<cros_safety::mojom::CrosSafetyService> receiver_set_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_CROS_SAFETY_CROS_SAFETY_SERVICE_H_
