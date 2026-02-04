// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_TRANSPARENT_UNORDERED_STRING_MAP_H_
#define COMPONENTS_PREFS_TRANSPARENT_UNORDERED_STRING_MAP_H_

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace internal {
// TODO(crbug.com/473916362): Remove this once all usages of
// `TransparentUnorderedStringMap` are migrated to Abseil maps.
struct StringViewHasher : public std::hash<std::string_view> {
  using is_transparent = void;
};
}  // namespace internal

// TODO(crbug.com/473916362): Usages of this should be replaced with an
// Abseil map such as absl::flat_hash_map, which allows for both string and
// string_view keys, and provides better performance than std::unordered_map.
// A `std::unordered_map` from `std::string` to `ValueType` that allows
// copy-less find for `std::string_view`.
template <typename ValueType>
using TransparentUnorderedStringMap =
    std::unordered_map<std::string,
                       ValueType,
                       internal::StringViewHasher,
                       std::equal_to<>>;

#endif  // COMPONENTS_PREFS_TRANSPARENT_UNORDERED_STRING_MAP_H_
