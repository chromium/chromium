// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_DATA_MODELS_COUNTRY_TYPE_H_
#define COMPONENTS_WALLET_CORE_BROWSER_DATA_MODELS_COUNTRY_TYPE_H_

#include <string>

#include "base/types/strong_alias.h"

namespace wallet {

// Country code in the format of uppercase ISO 3166-1 alpha-2. Example: US, BR,
// IN. Empty if unknown.
// This country code is estimated by the variations::VariationsService.
using GeoIpCountryCode =
    base::StrongAlias<class GeoIpCountryCodeTag, std::string>;

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_DATA_MODELS_COUNTRY_TYPE_H_
