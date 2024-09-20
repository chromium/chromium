// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_UI_UTILS_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_UI_UTILS_H_

#include <string>

#include "components/plus_addresses/plus_address_types.h"

namespace plus_addresses {

// Returns a string for UI display computed from the `plus_address` facet URI.
// For Android origins, the package name is returned. For web origins, the
// formatted URL without the cryptographic scheme is returned.
std::u16string GetOriginForDisplay(const PlusProfile& plus_address);

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_UI_UTILS_H_
