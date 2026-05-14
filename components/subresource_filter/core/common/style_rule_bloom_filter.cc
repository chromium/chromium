// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/style_rule_bloom_filter.h"

#include <algorithm>

namespace subresource_filter {

namespace {

std::pair<size_t, size_t> GetIndices(uint32_t hash, size_t num_bits) {
  size_t index1 = hash % num_bits;
  // 16777619u is the FNV-1a 32-bit prime, used here to generate a second hash.
  size_t index2 =
      static_cast<size_t>((static_cast<uint64_t>(hash) * 16777619u) % num_bits);
  return {index1, index2};
}

void SetBit(size_t bit_index, base::span<uint8_t> filter) {
  filter[bit_index / 8] |= (1 << (bit_index % 8));
}

}  // namespace

// --- StyleRuleBloomFilter ---

StyleRuleBloomFilter::StyleRuleBloomFilter(base::span<const uint8_t> data)
    : data_(data) {}

bool StyleRuleBloomFilter::MaybeContains(uint32_t hash) const {
  if (data_.empty()) {
    return true;
  }
  size_t num_bits = data_.size() * 8;
  auto [bit_index1, bit_index2] = GetIndices(hash, num_bits);
  return HasBit(bit_index1) && HasBit(bit_index2);
}

bool StyleRuleBloomFilter::HasBit(size_t bit_index) const {
  return data_[bit_index / 8] & (1 << (bit_index % 8));
}

// --- StyleRuleBloomFilterBuilder ---

StyleRuleBloomFilterBuilder::StyleRuleBloomFilterBuilder(
    size_t expected_rule_count) {
  size_t filter_size_bytes =
      std::max<size_t>(1024, (expected_rule_count * 10 + 7) / 8);
  buffer_.resize(filter_size_bytes, 0);
}

StyleRuleBloomFilterBuilder::~StyleRuleBloomFilterBuilder() = default;

void StyleRuleBloomFilterBuilder::SetBits(uint32_t hash) {
  size_t num_bits = buffer_.size() * 8;
  auto [bit_index1, bit_index2] = GetIndices(hash, num_bits);
  SetBit(bit_index1, buffer_);
  SetBit(bit_index2, buffer_);
}

}  // namespace subresource_filter
