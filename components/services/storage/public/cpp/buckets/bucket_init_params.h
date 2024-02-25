// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_BUCKETS_BUCKET_INIT_PARAMS_H_
#define COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_BUCKETS_BUCKET_INIT_PARAMS_H_

#include <optional>

#include "base/time/time.h"
#include "components/services/storage/public/cpp/buckets/bucket_id.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/buckets/bucket_manager_host.mojom.h"

namespace storage {

// A collection of attributes to describe a bucket.
//
// These attributes are used for creating the bucket in the database.
struct COMPONENT_EXPORT(STORAGE_SERVICE_BUCKETS_SUPPORT) BucketInitParams {
 public:
  // Creates an object that represents the `default` bucket for the given
  // storage key.
  static BucketInitParams ForDefaultBucket(
      const blink::StorageKey& storage_key);

  BucketInitParams(blink::StorageKey storage_key, const std::string& name);
  ~BucketInitParams();

  BucketInitParams(const BucketInitParams&);
  BucketInitParams(BucketInitParams&&) noexcept;
  BucketInitParams& operator=(const BucketInitParams&);
  BucketInitParams& operator=(BucketInitParams&&) noexcept;

  blink::StorageKey storage_key;
  std::string name;

  // `is_null()` when not specified.
  base::Time expiration = base::Time();

  // 0 when not specified.
  int64_t quota = 0;

  // nullopt when not specified.
  std::optional<bool> persistent;
  std::optional<blink::mojom::BucketDurability> durability;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_BUCKETS_BUCKET_INIT_PARAMS_H_
