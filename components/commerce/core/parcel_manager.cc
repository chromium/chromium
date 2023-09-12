// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/parcel_manager.h"

#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace commerce {

ParcelManager::ParcelManager(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    SessionProtoStorage<parcel_tracking_db::ParcelTrackingContent>*
        parcel_tracking_proto_db,
    AccountChecker* account_checker) {}

ParcelManager::~ParcelManager() = default;

}  // namespace commerce
