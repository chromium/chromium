// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace content_settings {

// Enables an improved UI for third-party cookie blocking in incognito mode.
#if defined(OS_IOS)
const base::Feature kImprovedCookieControls{"ImprovedCookieControls",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_IOS)

// Enables auto dark feature in theme settings.
#if defined(OS_ANDROID)
const base::Feature kDarkenWebsitesCheckboxInThemesSetting{
    "DarkenWebsitesCheckboxInThemesSetting", base::FEATURE_DISABLED_BY_DEFAULT};
constexpr base::FeatureParam<bool> kDarkenWebsitesCheckboxOptOut{
    &kDarkenWebsitesCheckboxInThemesSetting, "opt_out", true};
#endif  // defined(OS_ANDROID)

}  // namespace content_settings
