// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_PARCEL_PARCELS_STORAGE_H_
#define COMPONENTS_COMMERCE_CORE_PARCEL_PARCELS_STORAGE_H_

#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/commerce/core/proto/parcel.pb.h"
#include "components/commerce/core/proto/parcel_tracking_db_content.pb.h"
#include "components/session_proto_db/session_proto_storage.h"

namespace commerce {
using ParcelTrackingContent = parcel_tracking_db::ParcelTrackingContent;
using GetParcelStatusCallback =
    base::OnceCallback<void(std::unique_ptr<std::vector<ParcelStatus>>)>;
using ParcelTrackings =
    std::vector<SessionProtoStorage<ParcelTrackingContent>::KeyAndValue>;
using StorageUpdateCallback = base::OnceCallback<void(bool /*success*/)>;

// Class for storing parcel tracking information in db.
class ParcelsStorage {
 public:
  explicit ParcelsStorage(
      SessionProtoStorage<ParcelTrackingContent>* parcel_tracking_db);
  ParcelsStorage(const ParcelsStorage&) = delete;
  ParcelsStorage& operator=(const ParcelsStorage&) = delete;
  virtual ~ParcelsStorage();

  // Initialize the storage, populate the cache entries.
  void Init();

  // Gets all parcel status.
  void GetAllParcelStatus(GetParcelStatusCallback callback);

  // Updates the status for a list of parcels.
  void UpdateParcelStatus(const std::vector<ParcelStatus>& parcel_status,
                          StorageUpdateCallback callback);

  // Deletes a parcel status from db.
  void DeleteParcelStatus(const ParcelIdentifier& parcel_identifier,
                          StorageUpdateCallback callback);

  // Deletes all the parcel status from db.
  void DeleteAllParcelStatus(StorageUpdateCallback callback);

 private:
  void OnAllParcelsLoaded(bool success, ParcelTrackings parcel_trackings);

  raw_ptr<SessionProtoStorage<ParcelTrackingContent>> proto_db_;

  // An in-memory cache of parcel status.
  std::map<std::string, ParcelStatus> parcels_cache_;

  bool is_initialized_ = false;

  base::WeakPtrFactory<ParcelsStorage> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_PARCEL_PARCELS_STORAGE_H_
