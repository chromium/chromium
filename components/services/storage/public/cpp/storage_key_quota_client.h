// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_STORAGE_KEY_QUOTA_CLIENT_H_
#define COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_STORAGE_KEY_QUOTA_CLIENT_H_

#include "components/services/storage/public/mojom/quota_client.mojom.h"

namespace storage {

// Interface for storage key based QuotaClient to be inherited by quota managed
// storage APIs that have not implemented Storage Buckets support yet. As
// Storage APIs migrate their implementation to support Storage Buckets, they
// should use the bucket base QuotaClient to be added as part of
// crbug.com/1199417.
//
// TODO(crbug.com/1214066): Once all storage API's have migrated off storage key
// based QuotaClient. This class should be removed and all API's should inherit
// from bucket based QuotaClient.
class COMPONENT_EXPORT(STORAGE_SERVICE_PUBLIC) StorageKeyQuotaClient
    : public mojom::QuotaClient {
 protected:
  ~StorageKeyQuotaClient() override = default;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_STORAGE_KEY_QUOTA_CLIENT_H_
