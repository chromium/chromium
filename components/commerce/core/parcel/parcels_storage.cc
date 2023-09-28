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
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace commerce {

namespace {
std::string GetDbKeyFromParcelStatus(
    const ParcelIdentifier& parcel_identifier) {
  return base::StringPrintf("%d_%s", parcel_identifier.carrier(),
                            parcel_identifier.tracking_id().c_str());
}
}  // namespace

ParcelsStorage::ParcelsStorage(
    SessionProtoStorage<ParcelTrackingContent>* parcel_tracking_db,
    base::Clock* clock)
    : proto_db_(parcel_tracking_db), clock_(clock) {}

ParcelsStorage::~ParcelsStorage() = default;

void ParcelsStorage::Init(OnInitializedCallback callback) {
  DCHECK(!is_initialized_);
  proto_db_->LoadAllEntries(base::BindOnce(&ParcelsStorage::OnAllParcelsLoaded,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           std::move(callback)));
}

std::unique_ptr<std::vector<parcel_tracking_db::ParcelTrackingContent>>
ParcelsStorage::GetAllParcelTrackingContents() {
  DCHECK(is_initialized_);
  auto result = std::make_unique<std::vector<ParcelTrackingContent>>();
  for (auto& kv : parcels_cache_) {
    result->emplace_back(kv.second);
  }
  return result;
}

void ParcelsStorage::UpdateParcelStatus(
    const std::vector<ParcelStatus>& parcel_status,
    StorageUpdateCallback callback) {
  DCHECK(is_initialized_);
  auto content_to_insert = std::make_unique<
      std::vector<std::pair<std::string, ParcelTrackingContent>>>();
  for (const auto& status : parcel_status) {
    std::string key = GetDbKeyFromParcelStatus(status.parcel_identifier());
    ParcelTrackingContent content;
    content.set_key(key);
    auto* new_status = content.mutable_parcel_status();
    *new_status = status;
    content.set_last_update_time_usec(
        clock_->Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
    content_to_insert->emplace_back(key, content);
    parcels_cache_[key] = std::move(content);
  }
  proto_db_->UpdateEntries(std::move(content_to_insert),
                           std::make_unique<std::vector<std::string>>(),
                           std::move(callback));
}

void ParcelsStorage::DeleteParcelStatus(const std::string& tracking_id,
                                        StorageUpdateCallback callback) {
  DCHECK(is_initialized_);

  absl::optional<ParcelIdentifier> parcel_identifier;
  for (auto& kv : parcels_cache_) {
    auto& identifier = kv.second.parcel_status().parcel_identifier();
    if (identifier.tracking_id() == tracking_id) {
      parcel_identifier = identifier;
      break;
    }
  }
  if (parcel_identifier.has_value()) {
    std::string key = GetDbKeyFromParcelStatus(parcel_identifier.value());
    parcels_cache_.erase(key);
    proto_db_->DeleteOneEntry(key, base::BindOnce(std::move(callback)));
  }
}

void ParcelsStorage::DeleteAllParcelStatus(StorageUpdateCallback callback) {
  DCHECK(is_initialized_);
  parcels_cache_.clear();
  proto_db_->DeleteAllContent(std::move(callback));
}

void ParcelsStorage::OnAllParcelsLoaded(OnInitializedCallback callback,
                                        bool success,
                                        ParcelTrackings parcel_trackings) {
  DCHECK(!is_initialized_);
  if (!success) {
    LOG(ERROR) << "Unable to load all Parcels from the db.";
  }
  is_initialized_ = true;
  for (auto& kv : parcel_trackings) {
    parcels_cache_.emplace(std::move(kv.first), std::move(kv.second));
  }
  std::move(callback).Run(success);
}

}  // namespace commerce
