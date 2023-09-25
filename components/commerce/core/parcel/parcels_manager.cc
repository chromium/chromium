// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/parcel/parcels_manager.h"

#include "base/functional/bind.h"
#include "components/commerce/core/parcel/parcels_server_proxy.h"
#include "components/commerce/core/parcel/parcels_storage.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace commerce {

namespace {

std::vector<ParcelIdentifier> ConvertParcelIdentifier(
    const std::vector<std::pair<ParcelIdentifier::Carrier, std::string>>&
        parcel_identifiers) {
  std::vector<ParcelIdentifier> result;
  for (auto& identifier : parcel_identifiers) {
    ParcelIdentifier id;
    id.set_carrier(identifier.first);
    id.set_tracking_id(identifier.second);
  }
  return result;
}

void DoNothing(bool success) {}
}  // namespace

ParcelsManager::ParcelsManager(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    SessionProtoStorage<parcel_tracking_db::ParcelTrackingContent>*
        parcel_tracking_proto_db,
    AccountChecker* account_checker)
    : parcels_server_proxy_(
          std::make_unique<ParcelsServerProxy>(identity_manager,
                                               url_loader_factory)),
      parcels_storage_(
          std::make_unique<ParcelsStorage>(parcel_tracking_proto_db)) {}

ParcelsManager::ParcelsManager(
    std::unique_ptr<ParcelsServerProxy> parcels_server_proxy,
    std::unique_ptr<ParcelsStorage> parcels_storage)
    : parcels_server_proxy_(std::move(parcels_server_proxy)),
      parcels_storage_(std::move(parcels_storage)) {}

ParcelsManager::~ParcelsManager() = default;

void ParcelsManager::StartTrackingParcels(
    const std::vector<std::pair<ParcelIdentifier::Carrier, std::string>>&
        parcel_identifiers,
    const std::string& source_page_domain,
    GetParcelStatusCallback callback) {
  pending_operations_.push(
      base::BindOnce(&ParcelsManager::StartTrackingParcelsInternal,
                     weak_ptr_factory_.GetWeakPtr(), parcel_identifiers,
                     source_page_domain, std::move(callback)));
  ProcessPendingOperations();
}

void ParcelsManager::GetAllParcelStatuses(GetParcelStatusCallback callback) {
  pending_operations_.push(
      base::BindOnce(&ParcelsManager::GetAllParcelStatusesInternal,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  ProcessPendingOperations();
}

void ParcelsManager::StopTrackingParcel(const std::string& tracking_id,
                                        StopParcelTrackingCallback callback) {
  pending_operations_.push(base::BindOnce(
      &ParcelsManager::StopTrackingParcelInternal,
      weak_ptr_factory_.GetWeakPtr(), tracking_id, std::move(callback)));
  ProcessPendingOperations();
}

void ParcelsManager::StopTrackingAllParcels(
    StopParcelTrackingCallback callback) {
  pending_operations_.push(
      base::BindOnce(&ParcelsManager::StopTrackingAllParcelsInternal,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  ProcessPendingOperations();
}

void ParcelsManager::ProcessPendingOperations() {
  // Initialize parcel storage if it is not initialized,
  if (storage_status_ == StorageInitializationStatus::kUninitialized) {
    storage_status_ = StorageInitializationStatus::kInitializing;
    is_processing_pending_operations_ = true;
    parcels_storage_->Init(
        base::BindOnce(&ParcelsManager::OnParcelStorageInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (is_processing_pending_operations_ || pending_operations_.empty()) {
    return;
  }

  is_processing_pending_operations_ = true;
  base::OnceClosure callback = std::move(pending_operations_.front());
  pending_operations_.pop();
  std::move(callback).Run();
}

void ParcelsManager::OnParcelStorageInitialized(bool success) {
  // TODO(qinmin): determine if we need to handle storage failure issue.
  storage_status_ = success ? StorageInitializationStatus::kSuccess
                            : StorageInitializationStatus::kFailed;
  OnCurrentOperationFinished();
}

void ParcelsManager::StartTrackingParcelsInternal(
    const std::vector<std::pair<ParcelIdentifier::Carrier, std::string>>&
        parcel_identifiers,
    const std::string& source_page_domain,
    GetParcelStatusCallback callback) {
  parcels_server_proxy_->StartTrackingParcels(
      ConvertParcelIdentifier(parcel_identifiers), source_page_domain,
      base::BindOnce(&ParcelsManager::OnGetParcelStatusDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::make_unique<std::vector<ParcelStatus>>(),
                     false /*has_stored_status*/, std::move(callback)));
}

void ParcelsManager::GetAllParcelStatusesInternal(
    GetParcelStatusCallback callback) {
  auto parcel_status = parcels_storage_->GetAllParcelStatus();
  if (parcel_status->empty()) {
    std::move(callback).Run(
        storage_status_ == StorageInitializationStatus::kSuccess,
        std::make_unique<std::vector<ParcelTrackingStatus>>());
    return;
  }
  // TODO(qinmin): Check whether the data is fresh and we need to call the
  // server.
  std::vector<ParcelIdentifier> identifiers;
  for (auto& status : *parcel_status) {
    identifiers.emplace_back(status.parcel_identifier());
  }
  parcels_server_proxy_->GetParcelStatus(
      std::move(identifiers),
      base::BindOnce(&ParcelsManager::OnGetParcelStatusDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(parcel_status),
                     storage_status_ == StorageInitializationStatus::kSuccess,
                     std::move(callback)));
}

void ParcelsManager::StopTrackingParcelInternal(
    const std::string& tracking_id,
    StopParcelTrackingCallback callback) {
  parcels_server_proxy_->StopTrackingParcel(
      tracking_id, base::BindOnce(&ParcelsManager::OnStopTrackingParcelDone,
                                  weak_ptr_factory_.GetWeakPtr(), tracking_id,
                                  std::move(callback)));
}

void ParcelsManager::StopTrackingAllParcelsInternal(
    StopParcelTrackingCallback callback) {
  parcels_server_proxy_->StopTrackingAllParcels(
      base::BindOnce(&ParcelsManager::OnStopTrackingAllParcelsDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ParcelsManager::OnGetParcelStatusDone(
    std::unique_ptr<std::vector<ParcelStatus>> stored_parcel_status,
    bool has_stored_status,
    GetParcelStatusCallback callback,
    bool success,
    std::unique_ptr<std::vector<ParcelStatus>> new_parcel_status) {
  DCHECK(is_processing_pending_operations_);

  // If network request fails, fallback to stored parcel status.
  std::vector<ParcelStatus>* status_to_return;
  if (success) {
    status_to_return = new_parcel_status.get();
  } else {
    status_to_return = stored_parcel_status.get();
  }

  auto result = std::make_unique<std::vector<ParcelTrackingStatus>>();
  for (auto& status : *status_to_return) {
    result->emplace_back(status);
  }

  // Update the database.
  if (success) {
    auto& status_to_update = *new_parcel_status;
    // TODO(qinmin): Check if we need to handle storage update failure.
    parcels_storage_->UpdateParcelStatus(status_to_update,
                                         base::BindOnce(&DoNothing));
  }

  // If there are stored status, treat the call as successful.
  std::move(callback).Run(success | has_stored_status, std::move(result));
  OnCurrentOperationFinished();
}

void ParcelsManager::OnStopTrackingParcelDone(
    const std::string& tracking_id,
    StopParcelTrackingCallback callback,
    bool success) {
  DCHECK(is_processing_pending_operations_);

  // Update the database if network request succeeds.
  if (success) {
    parcels_storage_->DeleteParcelStatus(tracking_id,
                                         base::BindOnce(&DoNothing));
  }

  std::move(callback).Run(success);
  OnCurrentOperationFinished();
}

void ParcelsManager::OnStopTrackingAllParcelsDone(
    StopParcelTrackingCallback callback,
    bool success) {
  DCHECK(is_processing_pending_operations_);

  // Update the database if network request succeeds.
  if (success) {
    parcels_storage_->DeleteAllParcelStatus(base::BindOnce(&DoNothing));
  }

  std::move(callback).Run(success);
  OnCurrentOperationFinished();
}

void ParcelsManager::OnCurrentOperationFinished() {
  DCHECK(is_processing_pending_operations_);
  is_processing_pending_operations_ = false;
  ProcessPendingOperations();
}

}  // namespace commerce
