// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_PARCEL_PARCELS_MANAGER_H_
#define COMPONENTS_COMMERCE_CORE_PARCEL_PARCELS_MANAGER_H_

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/proto/parcel.pb.h"
#include "components/commerce/core/proto/parcel_tracking_db_content.pb.h"
#include "components/session_proto_db/session_proto_storage.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace commerce {
class AccountChecker;
class ParcelsServerProxy;
class ParcelsStorage;

// Class for managing all the parcel information
class ParcelsManager {
 public:
  ParcelsManager(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      SessionProtoStorage<parcel_tracking_db::ParcelTrackingContent>*
          parcel_tracking_proto_db,
      AccountChecker* account_checker);
  ~ParcelsManager();
  ParcelsManager(const ParcelsManager&) = delete;
  ParcelsManager& operator=(const ParcelsManager&) = delete;

  // Starts tracking a list of parcels.
  void StartTrackingParcels(
      const std::vector<std::pair<ParcelIdentifier::Carrier, std::string>>&
          parcel_identifiers,
      const std::string& source_page_domain,
      GetParcelStatusCallback callback);

  // Gets status for a list of parcels.
  void GetParcelStatus(
      const std::vector<std::pair<ParcelIdentifier::Carrier, std::string>>&
          parcel_identifiers,
      GetParcelStatusCallback callback);

  // Called to stop tracking a given parcel.
  void StopTrackingParcel(const std::string& tracking_id,
                          StopParcelTrackingCallback callback);

  // Called to stop tracking all parcels.
  void StopTrackingAllParcels(StopParcelTrackingCallback callback);

 private:
  std::unique_ptr<ParcelsServerProxy> parcels_server_proxy_;

  std::unique_ptr<ParcelsStorage> parcels_storage_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_PARCEL_PARCELS_MANAGER_H_
