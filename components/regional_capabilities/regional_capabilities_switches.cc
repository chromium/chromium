// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_switches.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"

namespace switches {

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kClearPrefForUnknownCountry,
             "ClearCountryPrefForStoredUnknownCountry",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kUseFinchPermanentCountryForFetchCountryId,
             "UseFinchPermanentCountyForFetchCountryId",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

}  // namespace switches
