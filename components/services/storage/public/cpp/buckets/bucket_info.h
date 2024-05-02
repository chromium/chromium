// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_BUCKETS_BUCKET_INFO_H_
#define COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_BUCKETS_BUCKET_INFO_H_

#include <stdint.h>

#include <set>
#include <string>

#include "base/component_export.h"
#include "base/time/time.h"
#include "components/services/storage/public/cpp/buckets/bucket_id.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"

namespace storage {

// Snapshot of a bucket's information in the quota database.
//
// Properties that can be updated by the Storage Buckets API, like
// `expiration` and `quota`, may get out of sync with the database. The
// database is the source of truth.
struct COMPONENT_EXPORT(STORAGE_SERVICE_BUCKETS_SUPPORT) BucketInfo {
  BucketInfo(BucketId bucket_id,
             blink::StorageKey storage_key,
             blink::mojom::StorageType type,
             std::string name,
             base::Time expiration,
             int64_t quota,
             bool persistent,
             blink::mojom::BucketDurability durability);

  BucketInfo();
  ~BucketInfo();

  BucketInfo(const BucketInfo&);
  BucketInfo(BucketInfo&&) noexcept;
  BucketInfo& operator=(const BucketInfo&);
  BucketInfo& operator=(BucketInfo&&) noexcept;

  COMPONENT_EXPORT(STORAGE_SERVICE_BUCKETS_SUPPORT)
  friend bool operator==(const BucketInfo& lhs, const BucketInfo& rhs);

  COMPONENT_EXPORT(STORAGE_SERVICE_BUCKETS_SUPPORT)
  friend bool operator!=(const BucketInfo& lhs, const BucketInfo& rhs);

  COMPONENT_EXPORT(STORAGE_SERVICE_BUCKETS_SUPPORT)
  friend bool operator<(const BucketInfo& lhs, const BucketInfo& rhs);

  BucketLocator ToBucketLocator() const {
    return BucketLocator(id, storage_key, type, name == kDefaultBucketName);
  }

  bool is_default() const { return name == kDefaultBucketName; }
  bool is_null() const { return !id; }

  BucketId id;
  blink::StorageKey storage_key;
  blink::mojom::StorageType type = blink::mojom::StorageType::kTemporary;
  std::string name;
  base::Time expiration;
  int64_t quota = 0;
  bool persistent = false;
  blink::mojom::BucketDurability durability =
      blink::mojom::BucketDurability::kRelaxed;
};

std::set<BucketLocator> COMPONENT_EXPORT(STORAGE_SERVICE_BUCKETS_SUPPORT)
    BucketInfosToBucketLocators(const std::set<BucketInfo>& bucket_infos);

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_BUCKETS_BUCKET_INFO_H_
