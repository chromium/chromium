// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/lib/event_buffer.h"

namespace metrics::structured {

ResourceInfo::ResourceInfo(int32_t max_size_bytes)
    : ResourceInfo(0, max_size_bytes) {}

ResourceInfo::ResourceInfo(int32_t used_size_bytes, int32_t max_size_bytes)
    : used_size_bytes(used_size_bytes), max_size_bytes(max_size_bytes) {}

bool ResourceInfo::HasRoom(int32_t size_bytes) const {
  return used_size_bytes + size_bytes <= max_size_bytes;
}

bool ResourceInfo::Consume(int32_t size_bytes) {
  used_size_bytes += size_bytes;
  return used_size_bytes <= max_size_bytes;
}

}  // namespace metrics::structured
