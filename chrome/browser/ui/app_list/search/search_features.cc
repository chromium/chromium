// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_features.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chromeos/constants/chromeos_features.h"

namespace search_features {

const base::Feature kLauncherGameSearch{"LauncherGameSearch",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

bool IsLauncherGameSearchEnabled() {
  if (!ash::features::IsProductivityLauncherEnabled())
    return false;

  return base::FeatureList::IsEnabled(kLauncherGameSearch) ||
         chromeos::features::IsCloudGamingDeviceEnabled();
}

}  // namespace search_features
