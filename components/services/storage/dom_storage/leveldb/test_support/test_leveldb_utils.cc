// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/leveldb/test_support/test_leveldb_utils.h"

namespace storage {

std::vector<uint8_t> ToBytes(base::span<const uint8_t> source) {
  return std::vector<uint8_t>(source.begin(), source.end());
}

}  // namespace storage
