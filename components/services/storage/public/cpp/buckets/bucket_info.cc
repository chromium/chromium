// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/buckets/bucket_info.h"

#include "base/ranges/algorithm.h"

namespace storage {

BucketInfo::BucketInfo(BucketId bucket_id,
                       blink::StorageKey storage_key,
                       blink::mojom::StorageType type,
                       std::string name,
                       base::Time expiration,
                       int64_t quota,
                       bool persistent,
                       blink::mojom::BucketDurability durability)
    : id(std::move(bucket_id)),
      storage_key(std::move(storage_key)),
      type(type),
      name(std::move(name)),
      expiration(std::move(expiration)),
      quota(quota),
      persistent(persistent),
      durability(durability) {}

BucketInfo::BucketInfo() = default;
BucketInfo::~BucketInfo() = default;

BucketInfo::BucketInfo(const BucketInfo&) = default;
BucketInfo::BucketInfo(BucketInfo&&) noexcept = default;
BucketInfo& BucketInfo::operator=(const BucketInfo&) = default;
BucketInfo& BucketInfo::operator=(BucketInfo&&) noexcept = default;

bool operator==(const BucketInfo& lhs, const BucketInfo& rhs) {
  return lhs.id == rhs.id;
}

bool operator!=(const BucketInfo& lhs, const BucketInfo& rhs) {
  return !(lhs == rhs);
}

bool operator<(const BucketInfo& lhs, const BucketInfo& rhs) {
  return lhs.id < rhs.id;
}

std::set<BucketLocator> COMPONENT_EXPORT(STORAGE_SERVICE_BUCKETS_SUPPORT)
    BucketInfosToBucketLocators(const std::set<BucketInfo>& bucket_infos) {
  std::set<BucketLocator> result;
  base::ranges::transform(bucket_infos, std::inserter(result, result.begin()),
                          &BucketInfo::ToBucketLocator);
  return result;
}

}  // namespace storage
