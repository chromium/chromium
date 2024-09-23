// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/nearby_presence_service_impl.h"

#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "chromeos/ash/components/nearby/presence/conversions/nearby_presence_conversions.h"
#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_credential_manager_impl.h"
#include "chromeos/ash/components/nearby/presence/metrics/nearby_presence_metrics.h"
#include "chromeos/ash/components/nearby/presence/nearby_presence_connections_manager.h"
#include "chromeos/ash/components/nearby/presence/nearby_presence_service_enum_coversions.h"
#include "chromeos/ash/components/nearby/presence/prefs/nearby_presence_prefs.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "components/cross_device/logging/logging.h"
#include "components/prefs/pref_service.h"
#include "components/push_notification/push_notification_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

::nearby::presence::PresenceDevice BuildPresenceDevice(
    ash::nearby::presence::mojom::PresenceDevicePtr device) {
  ::nearby::presence::PresenceDevice presence_device(device->endpoint_id);
  presence_device.SetDeviceIdentityMetaData(
      ash::nearby::presence::MetadataFromMojom(device->metadata.get()));
  for (auto action : device->actions) {
    presence_device.AddAction(static_cast<uint32_t>(action));
  }

  if (device->decrypt_shared_credential.get()) {
    presence_device.SetDecryptSharedCredential(
        ash::nearby::presence::SharedCredentialFromMojom(
            device->decrypt_shared_credential.get()));
  }

  return presence_device;
}

}  // namespace

namespace ash::nearby::presence {

NearbyPresenceServiceImpl::NearbyPresenceServiceImpl(
    PrefService* pref_service,
    ash::nearby::NearbyProcessManager* process_manager,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    push_notification::PushNotificationService* push_notification_service)
    : PushNotificationClient(push_notification::ClientId::kNearbyPresence),
      pref_service_(pref_service),
      process_manager_(process_manager),
      identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory),
      push_notification_service_(push_notification_service) {
  CHECK(pref_service_);
  CHECK(process_manager_);
  CHECK(identity_manager_);
  CHECK(url_loader_factory_);

  CHECK(push_notification_service_);
  push_notification_service_->GetPushNotificationClientManager()
      ->AddPushNotificationClient(this);
}

NearbyPresenceServiceImpl::~NearbyPresenceServiceImpl() {
  push_notification_service_->GetPushNotificationClientManager()
      ->RemovePushNotificationClient(GetClientId());
}

void NearbyPresenceServiceImpl::StartScan(
    ScanFilter scan_filter,
    ScanDelegate* scan_delegate,
    base::OnceCallback<void(std::unique_ptr<ScanSession>, enums::StatusCode)>
        on_start_scan_callback) {
  if (!SetProcessReference()) {
    LOG(ERROR) << "Failed to create process reference.";
    std::move(on_start_scan_callback)
        .Run(/*scan_session=*/nullptr,
             /*status=*/enums::StatusCode::kFailedToStartProcess);

    metrics::RecordScanRequestResult(enums::StatusCode::kFailedToStartProcess);
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

  start_scan_start_time_ = base::TimeTicks::Now();

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
    CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
        << __func__ << ": Initiating updating credentials.";
    credential_manager_->UpdateCredentials();
    return;
  }

  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << __func__
      << ": Attempted to update credentials, but "
         "CredentialManager was not yet initialized.";

  // Otherwise, initialize a `CredentialManager` before updating credentials.
  Initialize(
      base::BindOnce(&NearbyPresenceServiceImpl::
                         UpdateCredentialsAfterCredentialManagerInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<NearbyPresenceConnectionsManager>
NearbyPresenceServiceImpl::CreateNearbyPresenceConnectionsManager() {
  return std::make_unique<NearbyPresenceConnectionsManager>(process_manager_);
}

void NearbyPresenceServiceImpl::Shutdown() {
  process_reference_.reset();
  scan_delegate_set_.clear();
}

void NearbyPresenceServiceImpl::OnDeviceFound(mojom::PresenceDevicePtr device) {
  auto build_device = BuildPresenceDevice(std::move(device));
  metrics::RecordDeviceFoundLatency(base::TimeTicks::Now() -
                                    start_scan_start_time_);
  for (ScanDelegate* delegate : scan_delegate_set_) {
    delegate->OnPresenceDeviceFound(build_device);
  }
}

void NearbyPresenceServiceImpl::OnDeviceChanged(
    mojom::PresenceDevicePtr device) {
  auto build_device = BuildPresenceDevice(std::move(device));
  for (ScanDelegate* delegate : scan_delegate_set_) {
    delegate->OnPresenceDeviceChanged(build_device);
  }
}

void NearbyPresenceServiceImpl::OnDeviceLost(mojom::PresenceDevicePtr device) {
  auto build_device = BuildPresenceDevice(std::move(device));
  for (ScanDelegate* delegate : scan_delegate_set_) {
    delegate->OnPresenceDeviceLost(build_device);
  }
}

void NearbyPresenceServiceImpl::OnMessageReceived(
    base::flat_map<std::string, std::string> message) {
  CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
      << __func__ << ": Push notification message recieved.";
  if ((message.at(push_notification::kNotificationClientIdKey) ==
       kNearbyPresencePushNotificationClientId) &&
      (message.at(push_notification::kNotificationTypeIdKey) ==
       kNearbyPresencePushNotificationTypeId)) {
    // TODO(b/319286048): Check for action specific information.
    CD_LOG(ERROR, Feature::NEARBY_INFRA)
        << __func__
        << ": Push notification message is correctly "
           "formatted. Updating credentials now.";
    UpdateCredentials();
  } else {
    CD_LOG(VERBOSE, Feature::NEARBY_INFRA)
        << __func__
        << ": Push notification message is malformed. Discarding message.";
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

void NearbyPresenceServiceImpl::OnScanStarted(
    ScanDelegate* scan_delegate,
    base::OnceCallback<void(std::unique_ptr<ScanSession>, enums::StatusCode)>
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
      .Run(std::move(scan_session), enums::ConvertToPresenceStatus(status));

  metrics::RecordScanRequestResult(enums::ConvertToPresenceStatus(status));
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

void NearbyPresenceServiceImpl::OnNearbyProcessStopped(
    ash::nearby::NearbyProcessManager::NearbyProcessShutdownReason
        shutdown_reason) {
  LOG(WARNING) << __func__ << ": Nearby process stopped.";
  metrics::RecordNearbyProcessShutdownReason(shutdown_reason);
  Shutdown();
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
