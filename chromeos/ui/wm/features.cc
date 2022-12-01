// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/wm/features.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace chromeos::wm::features {

// Enables a window to float.
// https://crbug.com/1240411
BASE_FEATURE(kFloatWindow,
             "CrOSLabsFloatWindow",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPartialSplit, "PartialSplit", base::FEATURE_DISABLED_BY_DEFAULT);

bool IsFloatWindowEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return base::FeatureList::IsEnabled(kFloatWindow);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->IsFloatWindowEnabled();
#else
  return false;
#endif
}

bool IsPartialSplitEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return base::FeatureList::IsEnabled(kPartialSplit);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->IsPartialSplitEnabled();
#else
  return false;
#endif
}

}  // namespace chromeos::wm::features
