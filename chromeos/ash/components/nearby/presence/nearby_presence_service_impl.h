// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_NEARBY_PRESENCE_SERVICE_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_NEARBY_PRESENCE_SERVICE_IMPL_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/nearby/presence/nearby_presence_service.h"
#include "chromeos/ash/services/nearby/public/cpp/nearby_process_manager.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

class PrefService;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash::nearby::presence {

class NearbyPresenceCredentialManager;

class NearbyPresenceServiceImpl
    : public NearbyPresenceService,
      public KeyedService,
      public ::ash::nearby::presence::mojom::ScanObserver {
 public:
  NearbyPresenceServiceImpl(
      PrefService* pref_service,
      ash::nearby::NearbyProcessManager* process_manager,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  NearbyPresenceServiceImpl(const NearbyPresenceServiceImpl&) = delete;
  NearbyPresenceServiceImpl& operator=(const NearbyPresenceServiceImpl&) =
      delete;
  ~NearbyPresenceServiceImpl() override;

  // NearbyPresenceService:
  void StartScan(
      ScanFilter scan_filter,
      ScanDelegate* scan_delegate,
      base::OnceCallback<void(std::unique_ptr<ScanSession>, StatusCode)>
          on_start_scan_callback) override;
  void Initialize(base::OnceClosure on_initialized_callback) override;
  void UpdateCredentials() override;

 private:
  void OnScanStarted(
      ScanDelegate* scan_delegate,
      base::OnceCallback<void(std::unique_ptr<ScanSession>, StatusCode)>
          on_start_scan_callback,
      mojo::PendingRemote<mojom::ScanSession> pending_remote,
      mojo_base::mojom::AbslStatusCode status);
  void OnScanSessionDisconnect(ScanDelegate* scan_delegate);
  void OnNearbyProcessStopped(
      ash::nearby::NearbyProcessManager::NearbyProcessShutdownReason);

  // KeyedService:
  void Shutdown() override;

  bool SetProcessReference();

  // ScanObserver:
  void OnDeviceFound(mojom::PresenceDevicePtr device) override;
  void OnDeviceChanged(mojom::PresenceDevicePtr device) override;
  void OnDeviceLost(mojom::PresenceDevicePtr device) override;

  void OnCredentialManagerInitialized(
      base::OnceClosure on_initialized_callback,
      std::unique_ptr<NearbyPresenceCredentialManager>
          initialized_credential_manager);
  void UpdateCredentialsAfterCredentialManagerInitialized();

  const raw_ptr<PrefService> pref_service_ = nullptr;
  const raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const raw_ptr<ash::nearby::NearbyProcessManager> process_manager_ = nullptr;
  std::unique_ptr<ash::nearby::NearbyProcessManager::NearbyProcessReference>
      process_reference_;
  std::unique_ptr<NearbyPresenceCredentialManager> credential_manager_;

  mojo::Receiver<::ash::nearby::presence::mojom::ScanObserver> scan_observer_{
      this};
  base::flat_set<ScanDelegate*> scan_delegate_set_;

  base::WeakPtrFactory<NearbyPresenceServiceImpl> weak_ptr_factory_{this};
};

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_NEARBY_PRESENCE_SERVICE_IMPL_H_
