// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_country_id.h"

#include "base/check_is_test.h"
#include "components/regional_capabilities/access/country_access_reason.h"

namespace regional_capabilities {

CountryIdHolder::CountryIdHolder(country_codes::CountryId country_id)
    : country_id_(country_id) {}

CountryIdHolder::~CountryIdHolder() = default;

CountryIdHolder::CountryIdHolder(const CountryIdHolder& other) = default;

CountryIdHolder& CountryIdHolder::operator=(const CountryIdHolder& other) =
    default;

bool CountryIdHolder::operator==(const CountryIdHolder& other) const {
  return country_id_ == other.country_id_;
}

country_codes::CountryId CountryIdHolder::GetRestricted(
    CountryAccessKey access_key) const {
  // TODO(crbug.com/328040066): Record access to UMA.
  return country_id_;
}

country_codes::CountryId CountryIdHolder::GetForTesting() const {
  CHECK_IS_TEST();
  return country_id_;
}

}  // namespace regional_capabilities
