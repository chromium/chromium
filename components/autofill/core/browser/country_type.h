// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_COUNTRY_TYPE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_COUNTRY_TYPE_H_

#include <string>

#include "base/types/strong_alias.h"

// Country code in the format of uppercase ISO 3166-1 alpha-2. Example: US, BR,
// IN. Empty if unknown.
// StrongAlias to distinguish it from language codes and coutries specified in
// address profiles or inferred from address forms. This country code is
// estimated by the variations::VariationsService.
using GeoIpCountryCode = base::StrongAlias<class GeoIpCountryTag, std::string>;

// Country code in the format of uppercase ISO 3166-1 alpha-2. Example: US, BR,
// IN. Empty if unknown.
// StrongAlias to distinguish countries specified in address profiles.
using AddressCountryCode =
    base::StrongAlias<class AutofillAddressCountryTag, std::string>;

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_COUNTRY_TYPE_H_
