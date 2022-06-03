// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/node_ordinal.h"

#include <algorithm>
#include <string>

#include "base/check.h"
#include "base/notreached.h"

namespace syncer {

NodeOrdinal Int64ToNodeOrdinal(int64_t x) {
  uint64_t y = static_cast<uint64_t>(x);
  y ^= 0x8000000000000000ULL;
  std::string bytes(NodeOrdinal::kMinLength, '\x00');
  if (y == 0) {
    // 0 is a special case since |bytes| must not be all zeros.
    bytes.push_back('\x80');
  } else {
    for (int i = 7; i >= 0; --i) {
      bytes[i] = static_cast<uint8_t>(y);
      y >>= 8;
    }
  }
  NodeOrdinal ordinal(bytes);
  DCHECK(ordinal.IsValid());
  return ordinal;
}

int64_t NodeOrdinalToInt64(const NodeOrdinal& ordinal) {
  uint64_t y = 0;
  const std::string& s = ordinal.ToInternalValue();
  size_t l = NodeOrdinal::kMinLength;
  if (s.length() < l) {
    NOTREACHED();
    l = s.length();
  }
  for (size_t i = 0; i < l; ++i) {
    const uint8_t byte = s[l - i - 1];
    y |= static_cast<uint64_t>(byte) << (i * 8);
  }
  y ^= 0x8000000000000000ULL;
  // This is technically implementation-defined if y > INT64_MAX, so
  // we're assuming that we're on a twos-complement machine.
  return static_cast<int64_t>(y);
}

}  // namespace syncer
