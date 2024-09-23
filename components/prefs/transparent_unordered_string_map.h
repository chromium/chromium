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
struct StringViewHasher : public std::hash<std::string_view> {
  using is_transparent = void;
};
}  // namespace internal

// A `std::unordered_map` from `std::string` to `ValueType` that allows
// copy-less find for `std::string_view`.
template <typename ValueType>
using TransparentUnorderedStringMap =
    std::unordered_map<std::string,
                       ValueType,
                       internal::StringViewHasher,
                       std::equal_to<>>;

#endif  // COMPONENTS_PREFS_TRANSPARENT_UNORDERED_STRING_MAP_H_
