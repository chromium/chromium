// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_BITSET_H_
#define COMPONENTS_UKM_BITSET_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/component_export.h"

namespace ukm {

// A custom bitset class used to keep track of web feature usage across
// sources in UKM. `std::bitset<>` and `base::EnumSet<>` are intentionally not
// used because they do not provide the flexibility needed to efficiently
// serialize/deserialize the set.
class COMPONENT_EXPORT(UKM_RECORDER) BitSet {
 public:
  // Creates an empty BitSet with specified `set_size`.
  explicit BitSet(size_t set_size);
  // Constructor to recreate a serialized bitset (provided as `data`).
  BitSet(size_t set_size, std::string_view data);

  BitSet(const BitSet&) = delete;
  BitSet& operator=(const BitSet&) = delete;

  ~BitSet();

  // Adds the given `index` to the bitset.
  void Add(size_t index);
  // Returns whether the given `index` is present in the bitset.
  bool Contains(size_t index) const;

  // Serializes `this` and returns the compressed value. The bitset can be
  // recreated with the `BitSet(size_t, std::string_view)` constructor.
  std::string Serialize() const;

  size_t set_size() const { return set_size_; }

 private:
  // Returns which element `index` maps to in `bitset_`.
  size_t ToInternalIndex(size_t index) const;
  // Returns which bit `index` maps to given a certain `bitset_` element.
  uint8_t ToBitmask(size_t index) const;

  const size_t set_size_;
  std::vector<uint8_t> bitset_;
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_BITSET_H_
