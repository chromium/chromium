// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/parcel_manager.h"

#include "base/functional/bind.h"
#include "components/commerce/core/parcel/parcels_server_proxy.h"
#include "components/commerce/core/parcel/parcels_storage.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace commerce {

namespace {

void OnGetParcelStatusDone(
    ParcelManager::GetParcelStatusCallback callback,
    ParcelRequestStatus status,
    std::unique_ptr<std::vector<ParcelStatus>> parcel_status) {
  std::move(callback).Run(status == ParcelRequestStatus::kSuccess,
                          std::move(parcel_status));
}

void OnStopTrackingParcelDone(
    ParcelManager::StopParcelTrackingCallback callback,
    ParcelRequestStatus status) {
  std::move(callback).Run(status == ParcelRequestStatus::kSuccess);
}

void OnStopTrackingAllParcelsDone(
    ParcelManager::StopParcelTrackingCallback callback,
    ParcelRequestStatus status) {
  std::move(callback).Run(status == ParcelRequestStatus::kSuccess);
}
}  // namespace

ParcelManager::ParcelManager(
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

ParcelManager::~ParcelManager() = default;

void ParcelManager::StartTrackingParcels(
    const std::vector<ParcelIdentifier>& parcel_identifiers,
    const std::string& source_page_domain,
    GetParcelStatusCallback callback) {
  parcels_server_proxy_->StartTrackingParcels(
      parcel_identifiers, source_page_domain,
      base::BindOnce(&OnGetParcelStatusDone, std::move(callback)));
}

void ParcelManager::GetParcelStatus(
    const std::vector<ParcelIdentifier>& parcel_identifiers,
    GetParcelStatusCallback callback) {
  // TODO(qinmin): check db first before sending request to the server.
  parcels_server_proxy_->GetParcelStatus(
      parcel_identifiers,
      base::BindOnce(&OnGetParcelStatusDone, std::move(callback)));
}

void ParcelManager::StopTrackingParcel(const std::string& tracking_id,
                                       StopParcelTrackingCallback callback) {
  parcels_server_proxy_->StopTrackingParcel(
      tracking_id,
      base::BindOnce(&OnStopTrackingParcelDone, std::move(callback)));
}

void ParcelManager::StopTrackingAllParcels(
    StopParcelTrackingCallback callback) {
  parcels_server_proxy_->StopTrackingAllParcels(
      base::BindOnce(&OnStopTrackingAllParcelsDone, std::move(callback)));
}

// Called to stop tracking a given parcel.

}  // namespace commerce
