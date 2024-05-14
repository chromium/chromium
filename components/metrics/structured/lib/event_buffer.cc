// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/lib/event_buffer.h"

namespace metrics::structured {

ResourceInfo::ResourceInfo(int32_t max_size) : ResourceInfo(0, max_size) {}

ResourceInfo::ResourceInfo(int32_t used_size, int32_t max_size)
    : used_size(used_size), max_size(max_size) {}

bool ResourceInfo::HasRoom(int32_t size) const {
  return used_size + size <= max_size;
}

bool ResourceInfo::Consume(int32_t size) {
  used_size += size;
  return used_size <= max_size;
}

}  // namespace metrics::structured
