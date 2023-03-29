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
BASE_FEATURE(kWindowLayoutMenu,
             "WindowLayoutMenu",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsWindowLayoutMenuEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return base::FeatureList::IsEnabled(kWindowLayoutMenu);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->IsWindowLayoutMenuEnabled();
#else
  return false;
#endif
}

}  // namespace chromeos::wm::features
