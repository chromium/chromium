// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_LIB_RESOURCE_INFO_H_
#define COMPONENTS_METRICS_STRUCTURED_LIB_RESOURCE_INFO_H_

#include <cstdint>

namespace metrics::structured {
// The current usage and limits of some recourse.
//
// These resources could be disk space or memory consumption.
struct ResourceInfo {
  uint64_t used_size_bytes = 0;
  uint64_t max_size_bytes = 0;

  explicit ResourceInfo(uint64_t max_size_bytes);

  // Check whether |this| can accommodate |size|.
  bool HasRoom(uint64_t size_bytes) const;

  // Increases currently used space with |size|.
  bool Consume(uint64_t size_bytes);
};
}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_LIB_RESOURCE_INFO_H_
