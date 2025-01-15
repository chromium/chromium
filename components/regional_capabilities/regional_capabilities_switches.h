// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SWITCHES_H_
#define COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SWITCHES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace switches {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
// When the `country_codes::kCountryIDUnknown` is stored in prefs and this
// feature is enabled the pref will be cleared allowing a valid country to be
// set again.
BASE_DECLARE_FEATURE(kClearPrefForUnknownCountry);
#endif
}  // namespace switches

#endif  // COMPONENTS_REGIONAL_CAPABILITIES_REGIONAL_CAPABILITIES_SWITCHES_H_
