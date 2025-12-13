// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/channel/enum_table.h"

#include <cstdlib>
#include <string_view>

#include "base/compiler_specific.h"

namespace cast_util {

#ifdef ARCH_CPU_64_BITS
// This assertion is pretty paranoid.  It will probably only ever be triggered
// if someone who doesn't understand how EnumTable works tries to add extra
// members to GenericEnumTableEntry.
static_assert(sizeof(GenericEnumTableEntry) == 16,
              "Instances of GenericEnumTableEntry are too big.");
#endif

// static
const GenericEnumTableEntry* GenericEnumTableEntry::FindByString(
    const GenericEnumTableEntry data[],
    std::size_t size,
    std::string_view str) {
  for (std::size_t i = 0; i < size; i++) {
    if (UNSAFE_TODO(data[i]).length == str.length() &&
        UNSAFE_TODO(std::memcmp(data[i].chars, str.data(), str.length())) ==
            0) {
      return &UNSAFE_TODO(data[i]);
    }
  }
  return nullptr;
}

// static
std::optional<std::string_view> GenericEnumTableEntry::FindByValue(
    const GenericEnumTableEntry data[],
    std::size_t size,
    int value) {
  for (std::size_t i = 0; i < size; i++) {
    if (UNSAFE_TODO(data[i]).value == value && UNSAFE_TODO(data[i]).has_str()) {
      return UNSAFE_TODO(data[i]).str();
    }
  }
  return std::nullopt;
}

}  // namespace cast_util
