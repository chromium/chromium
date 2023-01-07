// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEAVY_AD_INTERVENTION_HEAVY_AD_HELPER_H_
#define COMPONENTS_HEAVY_AD_INTERVENTION_HEAVY_AD_HELPER_H_

#include <string>

namespace heavy_ad_intervention {

// Returns a string containing HTML of an error page for the heavy ad
// intervention.
std::string PrepareHeavyAdPage(const std::string& application_locale);

}  // namespace heavy_ad_intervention

#endif  // COMPONENTS_HEAVY_AD_INTERVENTION_HEAVY_AD_HELPER_H_
