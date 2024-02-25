// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_PARCEL_PARCELS_MANAGER_H_
#define COMPONENTS_COMMERCE_CORE_PARCEL_PARCELS_MANAGER_H_

#include <memory>
#include <queue>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/proto/parcel.pb.h"
#include "components/commerce/core/proto/parcel_tracking_db_content.pb.h"
#include "components/session_proto_db/session_proto_storage.h"

namespace base {
class Clock;
}

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace commerce {
class ParcelsServerProxy;
class ParcelsStorage;

// Class for managing all the parcel information
class ParcelsManager {
 public:
  ParcelsManager(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      SessionProtoStorage<parcel_tracking_db::ParcelTrackingContent>*
          parcel_tracking_proto_db);
  // Ctor used for testing purposes.
  ParcelsManager(std::unique_ptr<ParcelsServerProxy> parcels_server_proxy,
                 std::unique_ptr<ParcelsStorage> parcels_storage,
                 base::Clock* clock);
  ~ParcelsManager();
  ParcelsManager(const ParcelsManager&) = delete;
  ParcelsManager& operator=(const ParcelsManager&) = delete;

  // Starts tracking a list of parcels.
  void StartTrackingParcels(
      const std::vector<std::pair<ParcelIdentifier::Carrier, std::string>>&
          parcel_identifiers,
      const std::string& source_page_domain,
      GetParcelStatusCallback callback);

  // Gets status for all stored parcels.
  void GetAllParcelStatuses(GetParcelStatusCallback callback);

  // Called to stop tracking a given parcel.
  // DEPRECATED.
  void StopTrackingParcel(const std::string& tracking_id,
                          StopParcelTrackingCallback callback);

  // Called to stop tracking multiple parcels.
  void StopTrackingParcels(
      const std::vector<std::pair<ParcelIdentifier::Carrier, std::string>>&
          parcel_identifiers,
      StopParcelTrackingCallback callback);

  // Called to stop tracking all parcels.
  void StopTrackingAllParcels(StopParcelTrackingCallback callback);

 private:
  enum class StorageInitializationStatus {
    // Storage is not yet initialized.
    kUninitialized = 0,
    // Storage is initializing.
    kInitializing = 1,
    // Storage is initialized successfully.
    kSuccess = 2,
    // Storage failed to initialize.
    kFailed = 3,
  };

  // Called to process all pending operations.
  void ProcessPendingOperations();

  // Called after parcel storage finished initialization.
  void OnParcelStorageInitialized(bool success);

  // Helper methods that implement the public methods.
  void StartTrackingParcelsInternal(
      const std::vector<std::pair<ParcelIdentifier::Carrier, std::string>>&
          parcel_identifiers,
      const std::string& source_page_domain,
      GetParcelStatusCallback callback);
  void GetAllParcelStatusesInternal(GetParcelStatusCallback callback);
  void StopTrackingParcelInternal(const std::string& tracking_id,
                                  StopParcelTrackingCallback callback);
  void StopTrackingParcelsInternal(
      const std::vector<std::pair<ParcelIdentifier::Carrier, std::string>>&
          parcel_identifiers,
      StopParcelTrackingCallback callback);

  void StopTrackingAllParcelsInternal(StopParcelTrackingCallback callback);

  // Called when parcel status is retrieved from the network. If
  // `has_stored_status` is true, the local stored parcel status could be
  // returned to the caller.
  void OnGetParcelStatusDone(
      std::unique_ptr<std::vector<ParcelStatus>> stored_parcel_status,
      bool has_stored_status,
      GetParcelStatusCallback callback,
      bool success,
      std::unique_ptr<std::vector<ParcelStatus>> new_parcel_status);

  // Called when stop tracking is completed.
  void OnStopTrackingParcelDone(const std::string& tracking_id,
                                StopParcelTrackingCallback callback,
                                bool success);

  // Called when stop all parcel tracking is completed.
  void OnStopTrackingParcelsDone(
      const std::vector<ParcelIdentifier>& parcel_identifiers,
      StopParcelTrackingCallback callback,
      bool success);

  // Called when stop all parcel tracking is completed.
  void OnStopTrackingAllParcelsDone(StopParcelTrackingCallback callback,
                                    bool success);

  // Called when one pending operation finishes.
  void OnCurrentOperationFinished();

  // Parcel storage initialization status.
  StorageInitializationStatus storage_status_ =
      StorageInitializationStatus::kUninitialized;

  bool is_processing_pending_operations_ = false;

  std::queue<base::OnceClosure> pending_operations_;

  raw_ptr<base::Clock> clock_;

  std::unique_ptr<ParcelsServerProxy> parcels_server_proxy_;

  std::unique_ptr<ParcelsStorage> parcels_storage_;

  base::WeakPtrFactory<ParcelsManager> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_PARCEL_PARCELS_MANAGER_H_
