// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_switches.h"

namespace switches {

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kClearPrefForUnknownCountry,
             "ClearCountryPrefForStoredUnknownCountry",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif
}  // namespace switches
