// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_STYLE_RULE_BLOOM_FILTER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_STYLE_RULE_BLOOM_FILTER_H_

#include <cstdint>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_span.h"

namespace subresource_filter {

// A simple bloom filter builder and reader over a byte span.
// It uses two indices per lookup, from a uint32_t hash in which only the first
// 24 bits are expected to be meaningful (as they come from blink AtomicString).
// It uses the first 16 bits for the first index and then mixes the full 24
// bits with a FNV-1a 32-bit prime to get the 16 bits for the second index.

// The reader class.
class StyleRuleBloomFilter {
 public:
  explicit StyleRuleBloomFilter(base::span<const uint8_t> data);

  // Returns true if the bloom filter contains hash. Called `MaybeContains`
  // because the underlying data that the bloom filter represents may not
  // contain it.
  bool MaybeContains(uint32_t hash) const;

 private:
  bool HasBit(size_t bit_index) const;
  base::raw_span<const uint8_t> data_;
};

// A builder class for constructing a StyleRuleBloomFilter. It will
// automatically size a vector (retrievable with `buffer()`) given
// `expected_rule_count`. Add hashes with calls to `SetBits.`
class StyleRuleBloomFilterBuilder {
 public:
  explicit StyleRuleBloomFilterBuilder(size_t expected_rule_count);

  StyleRuleBloomFilterBuilder(const StyleRuleBloomFilterBuilder&) = delete;
  StyleRuleBloomFilterBuilder& operator=(const StyleRuleBloomFilterBuilder&) =
      delete;

  ~StyleRuleBloomFilterBuilder();

  // Add `hash` to the bloom filter.
  void SetBits(uint32_t hash);

  // The underlying buffer that can be written to a flatbuffer or read from
  // via a `StyleRuleBloomFilter` class.
  const std::vector<uint8_t>& buffer() const { return buffer_; }

 private:
  std::vector<uint8_t> buffer_;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_STYLE_RULE_BLOOM_FILTER_H_
