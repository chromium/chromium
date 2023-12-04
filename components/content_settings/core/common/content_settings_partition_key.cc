// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings_partition_key.h"

#include "base/check.h"

namespace content_settings {

#if BUILDFLAG(IS_IOS)
// static
const PartitionKey& PartitionKey::GetDefault() {
  return GetDefaultImpl();
}
#else
// static
const PartitionKey& PartitionKey::GetDefaultForTesting() {
  return GetDefaultImpl();
}
#endif  // BUILDFLAG(IS_IOS)

// static
const PartitionKey& PartitionKey::WipGetDefault() {
  return GetDefaultImpl();
}

PartitionKey::PartitionKey(const PartitionKey& key) = default;
PartitionKey::PartitionKey(PartitionKey&& key) = default;

// static
const PartitionKey& PartitionKey::GetDefaultImpl() {
  static const base::NoDestructor<PartitionKey> key;
  return *key;
}

PartitionKey::PartitionKey() : PartitionKey("", "", false) {}

PartitionKey::PartitionKey(const std::string& domain,
                           const std::string& name,
                           bool in_memory)
    : domain_{domain}, name_{name}, in_memory_{in_memory} {
  if (domain_.empty()) {
    // This is a default partition key.
    CHECK(name_.empty());
    CHECK(!in_memory_);
  }
}

}  // namespace content_settings
