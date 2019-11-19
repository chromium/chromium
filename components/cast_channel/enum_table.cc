// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_channel/enum_table.h"

#include <cstdlib>

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
    base::StringPiece str) {
  for (std::size_t i = 0; i < size; i++) {
    if (data[i].length == str.length() &&
        std::memcmp(data[i].chars, str.data(), str.length()) == 0)
      return &data[i];
  }
  return nullptr;
}

// static
base::Optional<base::StringPiece> GenericEnumTableEntry::FindByValue(
    const GenericEnumTableEntry data[],
    std::size_t size,
    int value) {
  for (std::size_t i = 0; i < size; i++) {
    if (data[i].value == value && data[i].has_str())
      return data[i].str();
  }
  return base::nullopt;
}

}  // namespace cast_util
