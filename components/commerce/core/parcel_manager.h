// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_PARCEL_MANAGER_H_
#define COMPONENTS_COMMERCE_CORE_PARCEL_MANAGER_H_

#include "base/memory/scoped_refptr.h"
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

class ParcelManager {
 public:
  ParcelManager(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      SessionProtoStorage<parcel_tracking_db::ParcelTrackingContent>*
          parcel_tracking_proto_db,
      AccountChecker* account_checker);
  ~ParcelManager();
  ParcelManager(const ParcelManager&) = delete;
  ParcelManager& operator=(const ParcelManager&) = delete;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_PARCEL_MANAGER_H_
