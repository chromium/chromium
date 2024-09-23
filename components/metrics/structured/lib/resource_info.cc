// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/lib/resource_info.h"

#include <cstdint>

namespace metrics::structured {

ResourceInfo::ResourceInfo(uint64_t max_size_bytes)
    : max_size_bytes(max_size_bytes) {}

bool ResourceInfo::HasRoom(uint64_t size_bytes) const {
  return used_size_bytes + size_bytes <= max_size_bytes;
}

bool ResourceInfo::Consume(uint64_t size_bytes) {
  used_size_bytes += size_bytes;
  return used_size_bytes <= max_size_bytes;
}
}  // namespace metrics::structured
