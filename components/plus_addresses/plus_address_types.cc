// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_types.h"

#include <memory>

namespace plus_addresses {

PlusProfile::PlusProfile(std::string profile_id,
                         std::string facet,
                         std::string plus_address,
                         bool is_confirmed)
    : profile_id(std::move(profile_id)),
      facet(std::move(facet)),
      plus_address(std::move(plus_address)),
      is_confirmed(is_confirmed) {}
PlusProfile::PlusProfile(const PlusProfile&) = default;
PlusProfile::PlusProfile(PlusProfile&&) = default;
PlusProfile::~PlusProfile() = default;

}  // namespace plus_addresses
