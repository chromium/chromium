// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/parcel/parcels_storage.h"

#include <string>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "components/session_proto_db/session_proto_storage.h"

namespace commerce {

namespace {
std::string GetDbKeyFromParcelStatus(
    const ParcelIdentifier& parcel_identifier) {
  return base::StringPrintf("%d_%s", parcel_identifier.carrier(),
                            parcel_identifier.tracking_id().c_str());
}
}  // namespace

ParcelsStorage::ParcelsStorage(
    SessionProtoStorage<ParcelTrackingContent>* parcel_tracking_db)
    : proto_db_(parcel_tracking_db) {}

ParcelsStorage::~ParcelsStorage() = default;

void ParcelsStorage::Init() {
  DCHECK(!is_initialized_);
  proto_db_->LoadAllEntries(base::BindOnce(&ParcelsStorage::OnAllParcelsLoaded,
                                           weak_ptr_factory_.GetWeakPtr()));
}

void ParcelsStorage::GetAllParcelStatus(GetParcelStatusCallback callback) {
  DCHECK(is_initialized_);
  // TODO(qinmin): Call the callback with everything in cache.
}

void ParcelsStorage::UpdateParcelStatus(
    const std::vector<ParcelStatus>& parcel_status,
    StorageUpdateCallback callback) {
  for (const auto& status : parcel_status) {
    std::string key = GetDbKeyFromParcelStatus(status.parcel_identifier());
    if (parcels_cache_.find(key) != parcels_cache_.end()) {
      // TODO(qinmin): Update the db.
    } else {
      // TODO(qinmin): Insert into db.
    }
    parcels_cache_[key] = status;
  }
}

void ParcelsStorage::DeleteParcelStatus(
    const ParcelIdentifier& parcel_identifier,
    StorageUpdateCallback callback) {
  std::string key = GetDbKeyFromParcelStatus(parcel_identifier);
  parcels_cache_.erase(key);
  proto_db_->DeleteOneEntry(key, base::BindOnce(std::move(callback)));
}

void ParcelsStorage::DeleteAllParcelStatus(StorageUpdateCallback callback) {
  parcels_cache_.clear();
  proto_db_->DeleteAllContent(std::move(callback));
}

void ParcelsStorage::OnAllParcelsLoaded(bool success,
                                        ParcelTrackings parcel_trackings) {
  DCHECK(!is_initialized_);
  if (!success) {
    LOG(ERROR) << "Unable to load all Parcels from the db.";
  }
  is_initialized_ = true;
  for (auto& kv : parcel_trackings) {
    auto& parcel_status = kv.second.parcel_status();
    parcels_cache_.emplace(std::move(kv.first), std::move(parcel_status));
  }
}

}  // namespace commerce
