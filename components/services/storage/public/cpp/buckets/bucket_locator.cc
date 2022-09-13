// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/buckets/bucket_locator.h"

namespace storage {

BucketLocator::BucketLocator(BucketId id,
                             blink::StorageKey storage_key,
                             blink::mojom::StorageType type,
                             bool is_default)
    : id(std::move(id)),
      storage_key(std::move(storage_key)),
      type(type),
      is_default(is_default) {}

BucketLocator::BucketLocator() = default;
BucketLocator::~BucketLocator() = default;

BucketLocator::BucketLocator(const BucketLocator&) = default;
BucketLocator::BucketLocator(BucketLocator&&) noexcept = default;
BucketLocator& BucketLocator::operator=(const BucketLocator&) = default;
BucketLocator& BucketLocator::operator=(BucketLocator&&) noexcept = default;

bool operator==(const BucketLocator& lhs, const BucketLocator& rhs) {
  return std::tie(lhs.id, lhs.storage_key, lhs.type, lhs.is_default) ==
         std::tie(rhs.id, rhs.storage_key, rhs.type, rhs.is_default);
}

bool operator!=(const BucketLocator& lhs, const BucketLocator& rhs) {
  return !(lhs == rhs);
}

bool operator<(const BucketLocator& lhs, const BucketLocator& rhs) {
  return std::tie(lhs.id, lhs.storage_key, lhs.type, lhs.is_default) <
         std::tie(rhs.id, rhs.storage_key, rhs.type, rhs.is_default);
}

}  // namespace storage
