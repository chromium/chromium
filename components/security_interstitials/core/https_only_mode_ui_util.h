// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_HTTPS_ONLY_MODE_UI_UTIL_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_HTTPS_ONLY_MODE_UI_UTIL_H_

#include "base/values.h"

class GURL;

namespace security_interstitials::https_only_mode {
struct HttpInterstitialState;
}

// Populates |load_time_data| for interstitial HTML.
void PopulateHttpsOnlyModeStringsForBlockingPage(
    base::Value::Dict& load_time_data,
    const GURL& url,
    const security_interstitials::https_only_mode::HttpInterstitialState& state,
    bool august2024_refresh_enabled);

// Values added to get shared interstitial HTML to play nice.
void PopulateHttpsOnlyModeStringsForSharedHTML(
    base::Value::Dict& load_time_data,
    bool august2024_refresh_enabled);

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_HTTPS_ONLY_MODE_UI_UTIL_H_
