// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/buckets/bucket_info.h"

#include "base/ranges/algorithm.h"

namespace storage {

BASE_FEATURE(kDefaultBucketUsesRelaxedDurability,
             "DefaultBucketUsesRelaxedDurability",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
      durability(durability) {
  // The default bucket's parameters are hard-coded in
  // `BucketInitParams::ForDefaultBucket`, and then persisted in the
  // quota database. The easiest way to override that in a reversible manner (in
  // case of the flag being flipped on and off) is here.
  // TODO(crbug.com/965883): migrate the quota db, update `BucketInitParams`,
  // and remove this.
  if (base::FeatureList::IsEnabled(
          storage::kDefaultBucketUsesRelaxedDurability) &&
      is_default()) {
    this->durability = blink::mojom::BucketDurability::kRelaxed;
  }
}

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
