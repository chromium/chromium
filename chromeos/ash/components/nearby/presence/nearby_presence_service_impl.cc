// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/nearby_presence_service_impl.h"

#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "chromeos/ash/components/nearby/presence/prefs/nearby_presence_prefs.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "components/prefs/pref_service.h"

namespace {

::nearby::internal::DeviceType ConvertMojomDeviceType(
    ash::nearby::presence::mojom::PresenceDeviceType mojom_type) {
  switch (mojom_type) {
    case ash::nearby::presence::mojom::PresenceDeviceType::kUnspecified:
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

ash::nearby::presence::NearbyPresenceService::PresenceDevice
BuildPresenceDevice(ash::nearby::presence::mojom::PresenceDevicePtr device) {
  // TODO(b/276642472): Populate actions and rssi fields.
  return ash::nearby::presence::NearbyPresenceService::PresenceDevice(
      ConvertMojomDeviceType(device->device_type), device->stable_device_id,
      device->endpoint_id, device->device_name,
      /*actions_=*/{}, /*rssi_=*/-65);
}

}  // namespace

namespace ash::nearby::presence {

NearbyPresenceServiceImpl::NearbyPresenceServiceImpl(
    PrefService* pref_service,
    ash::nearby::NearbyProcessManager* process_manager)
    : pref_service_(pref_service), process_manager_(process_manager) {
  CHECK(pref_service_);
  CHECK(process_manager_);
}

NearbyPresenceServiceImpl::~NearbyPresenceServiceImpl() = default;

void NearbyPresenceServiceImpl::StartScan(
    ScanFilter scan_filter,
    ScanDelegate* scan_delegate,
    base::OnceCallback<void(std::unique_ptr<ScanSession>, PresenceStatus)>
        on_start_scan_callback) {
  if (!SetProcessReference()) {
    LOG(ERROR) << "Failed to create process reference.";
    std::move(on_start_scan_callback)
        .Run(/*scan_session=*/nullptr, PresenceStatus::kFailure);
    return;
  }

  if (!scan_observer_.is_bound()) {
    process_reference_->GetNearbyPresence()->SetScanObserver(
        scan_observer_.BindNewPipeAndPassRemote());
  }

  CHECK(scan_delegate);
  std::vector<PresenceIdentityType> type_vector;
  if (scan_filter.identity_type_ == IdentityType::kPrivate) {
    type_vector.push_back(PresenceIdentityType::kIdentityTypePrivate);
  }
  std::vector<mojom::PresenceScanFilterPtr> filters_vector;
  auto filter = PresenceFilter::New(mojom::PresenceDeviceType::kChromeos);
  filters_vector.push_back(std::move(filter));

  process_reference_->GetNearbyPresence()->StartScan(
      mojom::ScanRequest::New(/*account_name=*/std::string(), type_vector,
                              std::move(filters_vector)),
      base::BindOnce(&NearbyPresenceServiceImpl::OnScanStarted,
                     weak_ptr_factory_.GetWeakPtr(), scan_delegate,
                     std::move(on_start_scan_callback)));
}

void NearbyPresenceServiceImpl::OnScanStarted(
    ScanDelegate* scan_delegate,
    base::OnceCallback<void(std::unique_ptr<ScanSession>, PresenceStatus)>
        on_start_scan_callback,
    mojo::PendingRemote<mojom::ScanSession> pending_remote,
    PresenceStatus status) {
  std::unique_ptr<ScanSession> scan_session;
  if (status == PresenceStatus::kOk) {
    scan_session = std::make_unique<ScanSession>(
        std::move(pending_remote),
        base::BindOnce(&NearbyPresenceServiceImpl::OnScanSessionDisconnect,
                       weak_ptr_factory_.GetWeakPtr(), scan_delegate));
    scan_delegate_set_.insert(scan_delegate);
  }
  std::move(on_start_scan_callback).Run(std::move(scan_session), status);
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

}  // namespace ash::nearby::presence
