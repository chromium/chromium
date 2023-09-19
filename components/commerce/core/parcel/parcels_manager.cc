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

void OnGetParcelStatusDone(
    GetParcelStatusCallback callback,
    ParcelRequestStatus request_status,
    std::unique_ptr<std::vector<ParcelStatus>> parcel_status) {
  auto result = std::make_unique<std::vector<ParcelTrackingStatus>>();
  for (auto& status : *parcel_status) {
    result->emplace_back(status);
  }
  std::move(callback).Run(request_status == ParcelRequestStatus::kSuccess,
                          std::move(result));
}

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

void OnStopTrackingParcelDone(StopParcelTrackingCallback callback,
                              ParcelRequestStatus status) {
  std::move(callback).Run(status == ParcelRequestStatus::kSuccess);
}

void OnStopTrackingAllParcelsDone(StopParcelTrackingCallback callback,
                                  ParcelRequestStatus status) {
  std::move(callback).Run(status == ParcelRequestStatus::kSuccess);
}
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

ParcelsManager::~ParcelsManager() = default;

void ParcelsManager::StartTrackingParcels(
    const std::vector<std::pair<ParcelIdentifier::Carrier, std::string>>&
        parcel_identifiers,
    const std::string& source_page_domain,
    GetParcelStatusCallback callback) {
  parcels_server_proxy_->StartTrackingParcels(
      ConvertParcelIdentifier(parcel_identifiers), source_page_domain,
      base::BindOnce(&OnGetParcelStatusDone, std::move(callback)));
}

void ParcelsManager::GetParcelStatus(
    const std::vector<std::pair<ParcelIdentifier::Carrier, std::string>>&
        parcel_identifiers,
    GetParcelStatusCallback callback) {
  // TODO(qinmin): check db first before sending request to the server.
  parcels_server_proxy_->GetParcelStatus(
      ConvertParcelIdentifier(parcel_identifiers),
      base::BindOnce(&OnGetParcelStatusDone, std::move(callback)));
}

void ParcelsManager::StopTrackingParcel(const std::string& tracking_id,
                                        StopParcelTrackingCallback callback) {
  parcels_server_proxy_->StopTrackingParcel(
      tracking_id,
      base::BindOnce(&OnStopTrackingParcelDone, std::move(callback)));
}

void ParcelsManager::StopTrackingAllParcels(
    StopParcelTrackingCallback callback) {
  parcels_server_proxy_->StopTrackingAllParcels(
      base::BindOnce(&OnStopTrackingAllParcelsDone, std::move(callback)));
}

// Called to stop tracking a given parcel.

}  // namespace commerce
