// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/nearby_presence_service_impl.h"

#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_credential_manager_impl.h"
#include "chromeos/ash/components/nearby/presence/nearby_presence_service_enum_coversions.h"
#include "chromeos/ash/components/nearby/presence/prefs/nearby_presence_prefs.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

::nearby::internal::DeviceType ConvertMojomDeviceType(
    ash::nearby::presence::mojom::PresenceDeviceType mojom_type) {
  switch (mojom_type) {
    case ash::nearby::presence::mojom::PresenceDeviceType::kUnknown:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_UNKNOWN;
    case ash::nearby::presence::mojom::PresenceDeviceType::kPhone:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_PHONE;
    case ash::nearby::presence::mojom::PresenceDeviceType::kTablet:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_TABLET;
    case ash::nearby::presence::mojom::PresenceDeviceType::kDisplay:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_DISPLAY;
    case ash::nearby::presence::mojom::PresenceDeviceType::kLaptop:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_LAPTOP;
    case ash::nearby::presence::mojom::PresenceDeviceType::kTv:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_TV;
    case ash::nearby::presence::mojom::PresenceDeviceType::kWatch:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_WATCH;
    case ash::nearby::presence::mojom::PresenceDeviceType::kChromeos:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_CHROMEOS;
    case ash::nearby::presence::mojom::PresenceDeviceType::kFoldable:
      return ::nearby::internal::DeviceType::DEVICE_TYPE_FOLDABLE;
  }
}

ash::nearby::presence::NearbyPresenceService::Action ConvertActionToActionType(
    ash::nearby::presence::mojom::ActionType action_type) {
  switch (action_type) {
    case ash::nearby::presence::mojom::ActionType::kActiveUnlockAction:
      return ash::nearby::presence::NearbyPresenceService::Action::
          kActiveUnlock;
    case ash::nearby::presence::mojom::ActionType::kNearbyShareAction:
      return ash::nearby::presence::NearbyPresenceService::Action::kNearbyShare;
    case ash::nearby::presence::mojom::ActionType::kInstantTetheringAction:
      return ash::nearby::presence::NearbyPresenceService::Action::
          kInstantTethering;
    case ash::nearby::presence::mojom::ActionType::kPhoneHubAction:
      return ash::nearby::presence::NearbyPresenceService::Action::kPhoneHub;
    case ash::nearby::presence::mojom::ActionType::kPresenceManagerAction:
      return ash::nearby::presence::NearbyPresenceService::Action::
          kPresenceManager;
    case ash::nearby::presence::mojom::ActionType::kFinderAction:
      return ash::nearby::presence::NearbyPresenceService::Action::kFinder;
    case ash::nearby::presence::mojom::ActionType::kFastPairSassAction:
      return ash::nearby::presence::NearbyPresenceService::Action::
          kFastPairSass;
    case ash::nearby::presence::mojom::ActionType::kTapToTransferAction:
      return ash::nearby::presence::NearbyPresenceService::Action::
          kTapToTransfer;
    case ash::nearby::presence::mojom::ActionType::kLastAction:
      return ash::nearby::presence::NearbyPresenceService::Action::kLast;
  }
}

ash::nearby::presence::NearbyPresenceService::PresenceDevice
BuildPresenceDevice(ash::nearby::presence::mojom::PresenceDevicePtr device) {
  std::vector<ash::nearby::presence::NearbyPresenceService::Action> actions;
  for (auto action : device->actions) {
    actions.push_back(ConvertActionToActionType(action));
  }

  // TODO(b/276642472): Populate actions and rssi fields.
  return ash::nearby::presence::NearbyPresenceService::PresenceDevice(
      ConvertMojomDeviceType(device->device_type), device->stable_device_id,
      device->endpoint_id, device->device_name, actions,
      /*rssi_=*/-65);
}

}  // namespace

namespace ash::nearby::presence {

NearbyPresenceServiceImpl::NearbyPresenceServiceImpl(
    PrefService* pref_service,
    ash::nearby::NearbyProcessManager* process_manager,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : pref_service_(pref_service),
      identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory),
      process_manager_(process_manager) {
  CHECK(pref_service_);
  CHECK(process_manager_);
  CHECK(identity_manager_);
  CHECK(url_loader_factory_);
}

NearbyPresenceServiceImpl::~NearbyPresenceServiceImpl() = default;

void NearbyPresenceServiceImpl::StartScan(
    ScanFilter scan_filter,
    ScanDelegate* scan_delegate,
    base::OnceCallback<void(std::unique_ptr<ScanSession>,
                            NearbyPresenceService::StatusCode)>
        on_start_scan_callback) {
  if (!SetProcessReference()) {
    LOG(ERROR) << "Failed to create process reference.";
    std::move(on_start_scan_callback)
        .Run(/*scan_session=*/nullptr,
             /*status=*/NearbyPresenceService::StatusCode::
                 kFailedToStartProcess);
    return;
  }

  if (!scan_observer_.is_bound()) {
    process_reference_->GetNearbyPresence()->SetScanObserver(
        scan_observer_.BindNewPipeAndPassRemote());
  }

  CHECK(scan_delegate);
  std::vector<PresenceIdentityType> identity_types;
  identity_types.push_back(
      ConvertToMojomIdentityType(scan_filter.identity_type_));
  std::vector<mojom::PresenceScanFilterPtr> filters;
  auto filter = PresenceFilter::New(mojom::PresenceDeviceType::kChromeos);
  filters.push_back(std::move(filter));

  process_reference_->GetNearbyPresence()->StartScan(
      mojom::ScanRequest::New(/*account_name=*/std::string(), identity_types,
                              std::move(filters)),
      base::BindOnce(&NearbyPresenceServiceImpl::OnScanStarted,
                     weak_ptr_factory_.GetWeakPtr(), scan_delegate,
                     std::move(on_start_scan_callback)));
}

void NearbyPresenceServiceImpl::Initialize(
    base::OnceClosure on_initialized_callback) {
  if (!SetProcessReference()) {
    LOG(ERROR) << "Failed to create process reference.";
    return;
  }

  CHECK(process_reference_);
  CHECK(process_reference_->GetNearbyPresence());
  CHECK(NearbyPresenceCredentialManagerImpl::Creator::Get());
  NearbyPresenceCredentialManagerImpl::Creator::Get()->Create(
      pref_service_, identity_manager_, url_loader_factory_,
      process_reference_->GetNearbyPresence(),
      base::BindOnce(&NearbyPresenceServiceImpl::OnCredentialManagerInitialized,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(on_initialized_callback)));
}

void NearbyPresenceServiceImpl::UpdateCredentials() {
  // If the `credential_manager_` field is non-null, it means the initialization
  // flow has already occurred, and we can move forward with updating
  // credentials.
  if (credential_manager_) {
    credential_manager_->UpdateCredentials();
    return;
  }

  // Otherwise, initialize a `CredentialManager` before updating credentials.
  Initialize(
      base::BindOnce(&NearbyPresenceServiceImpl::
                         UpdateCredentialsAfterCredentialManagerInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbyPresenceServiceImpl::OnScanStarted(
    ScanDelegate* scan_delegate,
    base::OnceCallback<void(std::unique_ptr<ScanSession>,
                            NearbyPresenceService::StatusCode)>
        on_start_scan_callback,
    mojo::PendingRemote<mojom::ScanSession> pending_remote,
    mojo_base::mojom::AbslStatusCode status) {
  std::unique_ptr<ScanSession> scan_session;
  if (status == mojo_base::mojom::AbslStatusCode::kOk) {
    scan_session = std::make_unique<ScanSession>(
        std::move(pending_remote),
        base::BindOnce(&NearbyPresenceServiceImpl::OnScanSessionDisconnect,
                       weak_ptr_factory_.GetWeakPtr(), scan_delegate));
    scan_delegate_set_.insert(scan_delegate);
  }
  std::move(on_start_scan_callback)
      .Run(std::move(scan_session), ConvertToPresenceStatus(status));
}

void NearbyPresenceServiceImpl::OnScanSessionDisconnect(
    ScanDelegate* scan_delegate) {
  CHECK(scan_delegate);
  for (auto it = scan_delegate_set_.begin(); it != scan_delegate_set_.end();
       it++) {
    if (*it == scan_delegate) {
      scan_delegate->OnScanSessionInvalidated();
      scan_delegate_set_.erase(it);
      return;
    }
  }
}

bool NearbyPresenceServiceImpl::SetProcessReference() {
  if (!process_reference_) {
    process_reference_ = process_manager_->GetNearbyProcessReference(
        base::BindOnce(&NearbyPresenceServiceImpl::OnNearbyProcessStopped,
                       weak_ptr_factory_.GetWeakPtr()));

    if (!process_reference_) {
      // TODO(b/277819923): add log here.
      return false;
    }
  }
  return true;
}

void NearbyPresenceServiceImpl::OnNearbyProcessStopped(
    ash::nearby::NearbyProcessManager::NearbyProcessShutdownReason) {
  // TODO(b/277819923): Add metric to record shutdown reason for Nearby
  // Presence process.
  LOG(WARNING) << __func__ << ": Nearby process stopped.";
  Shutdown();
}

void NearbyPresenceServiceImpl::Shutdown() {
  process_reference_.reset();
  scan_delegate_set_.clear();
}

void NearbyPresenceServiceImpl::OnDeviceFound(mojom::PresenceDevicePtr device) {
  auto build_device = BuildPresenceDevice(std::move(device));
  for (auto* delegate : scan_delegate_set_) {
    delegate->OnPresenceDeviceFound(build_device);
  }
}

void NearbyPresenceServiceImpl::OnDeviceChanged(
    mojom::PresenceDevicePtr device) {
  auto build_device = BuildPresenceDevice(std::move(device));
  for (auto* delegate : scan_delegate_set_) {
    delegate->OnPresenceDeviceChanged(build_device);
  }
}

void NearbyPresenceServiceImpl::OnDeviceLost(mojom::PresenceDevicePtr device) {
  auto build_device = BuildPresenceDevice(std::move(device));
  for (auto* delegate : scan_delegate_set_) {
    delegate->OnPresenceDeviceLost(build_device);
  }
}

void NearbyPresenceServiceImpl::OnCredentialManagerInitialized(
    base::OnceClosure on_initialized_callback,
    std::unique_ptr<NearbyPresenceCredentialManager>
        initialized_credential_manager) {
  credential_manager_ = std::move(initialized_credential_manager);
  std::move(on_initialized_callback).Run();
}

void NearbyPresenceServiceImpl::
    UpdateCredentialsAfterCredentialManagerInitialized() {
  CHECK(credential_manager_);
  credential_manager_->UpdateCredentials();
}

}  // namespace ash::nearby::presence
