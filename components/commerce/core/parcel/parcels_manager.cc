// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/parcel/parcels_manager.h"

#include <set>

#include "base/functional/bind.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "components/commerce/core/parcel/parcels_server_proxy.h"
#include "components/commerce/core/parcel/parcels_storage.h"
#include "components/commerce/core/parcel/parcels_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace commerce {

namespace {

constexpr base::TimeDelta kAboutToDeliverThreshold = base::Days(1);
constexpr base::TimeDelta kRefreshIntervalForAboutToDeliver = base::Hours(1);
constexpr base::TimeDelta kDefaultRefreshInterval = base::Hours(12);

std::vector<ParcelIdentifier> ConvertParcelIdentifier(
    const std::vector<std::pair<ParcelIdentifier::Carrier, std::string>>&
        parcel_identifiers) {
  std::vector<ParcelIdentifier> result;
  for (auto& identifier : parcel_identifiers) {
    ParcelIdentifier id;
    id.set_carrier(identifier.first);
    id.set_tracking_id(identifier.second);
    result.emplace_back(std::move(id));
  }
  return result;
}

bool IsParcelDone(const parcel_tracking_db::ParcelTrackingContent& tracking) {
  ParcelStatus::ParcelState parcel_state =
      tracking.parcel_status().parcel_state();
  return IsParcelStateDone(parcel_state);
}

std::vector<ParcelIdentifier> GetParcelIdentifiersToRefresh(
    base::Clock* clock,
    const std::vector<parcel_tracking_db::ParcelTrackingContent>&
        parcel_trackings) {
  base::Time now = clock->Now();

  std::vector<ParcelIdentifier> identifiers;
  bool has_parcel_to_update = false;
  for (const auto& tracking : parcel_trackings) {
    if (IsParcelDone(tracking)) {
      continue;
    }
    identifiers.emplace_back(tracking.parcel_status().parcel_identifier());
    base::TimeDelta since_last_update =
        now - base::Time::FromDeltaSinceWindowsEpoch(
                  base::Microseconds(tracking.last_update_time_usec()));
    base::TimeDelta time_to_deliver =
        base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(
            tracking.parcel_status().estimated_delivery_time_usec())) -
        now;
    if (time_to_deliver < kAboutToDeliverThreshold) {
      if (since_last_update >= kRefreshIntervalForAboutToDeliver) {
        has_parcel_to_update = true;
      }
    } else if (since_last_update >= kDefaultRefreshInterval) {
      has_parcel_to_update = true;
    }
  }
  // Return all parcels that are not yet finished.
  if (has_parcel_to_update) {
    return identifiers;
  }
  return std::vector<ParcelIdentifier>();
}

std::unique_ptr<std::vector<ParcelStatus>>
ConvertParcelTrackingContentsToParcelStatuses(
    const std::vector<parcel_tracking_db::ParcelTrackingContent>&
        parcel_trackings) {
  auto ret = std::make_unique<std::vector<ParcelStatus>>();
  for (const auto& tracking : parcel_trackings) {
    ret->emplace_back(tracking.parcel_status());
  }
  return ret;
}

std::unique_ptr<std::vector<ParcelTrackingStatus>> UpdateStoredParcelStatus(
    const std::vector<ParcelStatus>& stored_parcel_status,
    const std::vector<ParcelStatus>& parcel_status_to_update) {
  auto result = std::make_unique<std::vector<ParcelTrackingStatus>>();
  std::set<std::pair<ParcelIdentifier::Carrier, std::string>> identifiers;
  for (const auto& status : parcel_status_to_update) {
    result->emplace_back(status);
    identifiers.emplace(status.parcel_identifier().carrier(),
                        status.parcel_identifier().tracking_id());
  }
  for (const auto& status : stored_parcel_status) {
    if (!identifiers.contains(
            std::make_pair(status.parcel_identifier().carrier(),
                           status.parcel_identifier().tracking_id()))) {
      result->emplace_back(status);
    }
  }
  return result;
}

}  // namespace

ParcelsManager::ParcelsManager(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    SessionProtoStorage<parcel_tracking_db::ParcelTrackingContent>*
        parcel_tracking_proto_db)
    : clock_(base::DefaultClock::GetInstance()),
      parcels_server_proxy_(
          std::make_unique<ParcelsServerProxy>(identity_manager,
                                               url_loader_factory)),
      parcels_storage_(
          std::make_unique<ParcelsStorage>(parcel_tracking_proto_db, clock_)) {}

ParcelsManager::ParcelsManager(
    std::unique_ptr<ParcelsServerProxy> parcels_server_proxy,
    std::unique_ptr<ParcelsStorage> parcels_storage,
    base::Clock* clock)
    : clock_(clock),
      parcels_server_proxy_(std::move(parcels_server_proxy)),
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

void ParcelsManager::StopTrackingParcels(
    const std::vector<std::pair<ParcelIdentifier::Carrier, std::string>>&
        parcel_identifiers,
    StopParcelTrackingCallback callback) {
  pending_operations_.push(base::BindOnce(
      &ParcelsManager::StopTrackingParcelsInternal,
      weak_ptr_factory_.GetWeakPtr(), parcel_identifiers, std::move(callback)));
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
  if (success) {
    parcels_storage_->ModifyOldDoneParcels();
  }
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
  auto parcel_trackings = parcels_storage_->GetAllParcelTrackingContents();
  if (parcel_trackings->empty()) {
    std::move(callback).Run(
        storage_status_ == StorageInitializationStatus::kSuccess,
        std::make_unique<std::vector<ParcelTrackingStatus>>());
    OnCurrentOperationFinished();
    return;
  }

  // Refresh all parcel status if one of them needs update.
  std::vector<ParcelIdentifier> identifiers_to_refresh =
      GetParcelIdentifiersToRefresh(clock_, *parcel_trackings);
  if (identifiers_to_refresh.empty()) {
    auto tracking_statuses =
        std::make_unique<std::vector<ParcelTrackingStatus>>();
    for (const auto& tracking : *parcel_trackings) {
      tracking_statuses->emplace_back(tracking.parcel_status());
    }
    std::move(callback).Run(
        storage_status_ == StorageInitializationStatus::kSuccess,
        std::move(tracking_statuses));
    OnCurrentOperationFinished();
    return;
  }

  auto parcel_status =
      ConvertParcelTrackingContentsToParcelStatuses(*parcel_trackings);
  parcels_server_proxy_->GetParcelStatus(
      std::move(identifiers_to_refresh),
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

void ParcelsManager::StopTrackingParcelsInternal(
    const std::vector<std::pair<ParcelIdentifier::Carrier, std::string>>&
        parcel_identifiers,
    StopParcelTrackingCallback callback) {
  std::vector<ParcelIdentifier> identifiers =
      ConvertParcelIdentifier(parcel_identifiers);
  parcels_server_proxy_->StopTrackingParcels(
      identifiers, base::BindOnce(&ParcelsManager::OnStopTrackingParcelsDone,
                                  weak_ptr_factory_.GetWeakPtr(), identifiers,
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

  // Merge stored parcel status with the server status.
  auto result =
      UpdateStoredParcelStatus(*stored_parcel_status, *new_parcel_status);

  // Update the database.
  if (success) {
    // TODO(qinmin): Check if we need to handle storage update failure.
    parcels_storage_->UpdateParcelStatus(*new_parcel_status,
                                         base::DoNothingAs<void(bool)>());
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
                                         base::DoNothingAs<void(bool)>());
  }

  std::move(callback).Run(success);
  OnCurrentOperationFinished();
}

void ParcelsManager::OnStopTrackingParcelsDone(
    const std::vector<ParcelIdentifier>& parcel_identifiers,
    StopParcelTrackingCallback callback,
    bool success) {
  DCHECK(is_processing_pending_operations_);
  // Update the database if network request succeeds.
  if (success) {
    parcels_storage_->DeleteParcelsStatus(parcel_identifiers,
                                          base::DoNothingAs<void(bool)>());
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
    parcels_storage_->DeleteAllParcelStatus(base::DoNothingAs<void(bool)>());
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
