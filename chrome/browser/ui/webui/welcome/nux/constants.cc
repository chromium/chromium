// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/nux/constants.h"

#include "base/feature_list.h"

namespace nux {

extern const base::Feature kNuxEmailFeature{"NuxEmail",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

extern const base::Feature kNuxGoogleAppsFeature{
    "NuxGoogleApps", base::FEATURE_DISABLED_BY_DEFAULT};

extern const base::Feature kNuxOnboardingFeature{
    "NuxOnboarding", base::FEATURE_DISABLED_BY_DEFAULT};

extern const char kNuxEmailUrl[] = "chrome://welcome/email";
extern const char kNuxGoogleAppsUrl[] = "chrome://welcome/apps";

}  // namespace nux