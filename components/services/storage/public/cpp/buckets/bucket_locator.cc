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

// static
BucketLocator BucketLocator::ForDefaultBucket(blink::StorageKey storage_key) {
  BucketLocator locator;
  locator.storage_key = std::move(storage_key);
  locator.is_default = true;
  locator.type = blink::mojom::StorageType::kTemporary;
  return locator;
}

BucketLocator::BucketLocator(const BucketLocator&) = default;
BucketLocator::BucketLocator(BucketLocator&&) noexcept = default;
BucketLocator& BucketLocator::operator=(const BucketLocator&) = default;
BucketLocator& BucketLocator::operator=(BucketLocator&&) noexcept = default;

bool BucketLocator::IsEquivalentTo(const BucketLocator& other) const {
  return *this == other ||
         (this->is_default &&
          (std::tie(storage_key, type, is_default) ==
           std::tie(other.storage_key, other.type, other.is_default)));
}

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

bool CompareBucketLocators::operator()(const BucketLocator& a,
                                       const BucketLocator& b) const {
  // In this custom comparator, we make default buckets match regardless
  // of ID as the ID can be blank for a default bucket.
  if (a.IsEquivalentTo(b))
    return false;

  // The normal operator< doesn't work here because it doesn't maintain a
  // strict weak ordering.
  return std::tie(a.storage_key, a.type, a.is_default, a.id) <
         std::tie(b.storage_key, b.type, b.is_default, b.id);
}

}  // namespace storage
