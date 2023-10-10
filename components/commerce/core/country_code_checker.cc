// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/country_code_checker.h"

#include "components/country_codes/country_codes.h"
#include "components/variations/service/variations_service.h"

namespace commerce {

std::string GetCurrentCountryCode(variations::VariationsService* variations) {
  std::string country;

  if (variations) {
    country = variations->GetStoredPermanentCountry();
  }

  // Since variations doesn't provide a permanent country by default on things
  // like local builds, we try to fall back to the country_codes component which
  // should always have one.
  if (country.empty()) {
    country = country_codes::GetCurrentCountryCode();
  }

  return country;
}

}  // namespace commerce
