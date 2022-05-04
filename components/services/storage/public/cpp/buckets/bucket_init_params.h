// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_BUCKETS_BUCKET_INIT_PARAMS_H_
#define COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_BUCKETS_BUCKET_INIT_PARAMS_H_

#include "base/time/time.h"
#include "components/services/storage/public/cpp/buckets/bucket_id.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

// A collection of attributes to describe a bucket.
//
// These attributes are used for creating the bucket in the database.
struct COMPONENT_EXPORT(STORAGE_SERVICE_BUCKETS_SUPPORT) BucketInitParams {
  // Creates a default bucket for the given storage key.
  explicit BucketInitParams(blink::StorageKey storage_key);

  ~BucketInitParams();

  BucketInitParams(const BucketInitParams&);
  BucketInitParams(BucketInitParams&&) noexcept;
  BucketInitParams& operator=(const BucketInitParams&);
  BucketInitParams& operator=(BucketInitParams&&) noexcept;

  blink::StorageKey storage_key;
  std::string name{kDefaultBucketName};
  base::Time expiration = base::Time::Max();
  int64_t quota = 0;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_PUBLIC_CPP_BUCKETS_BUCKET_INIT_PARAMS_H_
