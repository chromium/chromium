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
    base::span<const GenericEnumTableEntry> data,
    std::string_view str) {
  for (const auto& entry : data) {
    if (entry.has_str() && entry.str() == str) {
      return &entry;
    }
  }
  return nullptr;
}

// static
std::optional<std::string_view> GenericEnumTableEntry::FindByValue(
    base::span<const GenericEnumTableEntry> data,
    int value) {
  for (const auto& entry : data) {
    if (entry.value == value && entry.has_str()) {
      return entry.str();
    }
  }
  return std::nullopt;
}

}  // namespace cast_util
