// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/buckets/bucket_init_params.h"

#include "components/services/storage/public/cpp/buckets/constants.h"

namespace storage {

BucketInitParams BucketInitParams::ForDefaultBucket(
    const blink::StorageKey& storage_key) {
  return BucketInitParams(storage_key, kDefaultBucketName);
}

BucketInitParams::BucketInitParams(blink::StorageKey storage_key,
                                   const std::string& name)
    : storage_key(std::move(storage_key)), name(name) {}

BucketInitParams::~BucketInitParams() = default;

BucketInitParams::BucketInitParams(const BucketInitParams&) = default;
BucketInitParams::BucketInitParams(BucketInitParams&&) noexcept = default;
BucketInitParams& BucketInitParams::operator=(const BucketInitParams&) =
    default;
BucketInitParams& BucketInitParams::operator=(BucketInitParams&&) noexcept =
    default;

}  // namespace storage
