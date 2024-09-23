// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_HASHING_H_
#define COMPONENTS_VARIATIONS_HASHING_H_

#include <stdint.h>

#include <string>
#include <string_view>

#include "base/component_export.h"

namespace variations {

// Computes a uint32_t hash of a given string based on its SHA1 hash. Suitable
// for uniquely identifying field trial names and group names.
COMPONENT_EXPORT(VARIATIONS) uint32_t HashName(std::string_view name);

// Returns the hex string representation of HashName(name).
COMPONENT_EXPORT(VARIATIONS)
std::string HashNameAsHexString(std::string_view name);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_HASHING_H_
