// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_FEATURES_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_FEATURES_H_

#include "base/feature_list.h"

namespace security_interstitials {

// Controls whether an interstitial is shown when submitting a mixed form.
extern const base::Feature kInsecureFormSubmissionInterstitial;

// Controls if the insecure form interstitial is enabled for forms that
// initially submit to https, but redirect to http. If not set the interstitial
// will be shown for forms that submit directly to http, or POST forms with a
// 307/308 redirect over http (since those expose the form data over http).
extern const char kInsecureFormSubmissionInterstitialMode[];
// If set to this mode, the interstitial will be shown for any redirect over
// http.
extern const char kInsecureFormSubmissionInterstitialModeIncludeAllRedirects[];
// If set to this mode, the interstitial will not be shown for any redirect.
extern const char kInsecureFormSubmissionInterstitialModeNoRedirects[];

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_FEATURES_H_
