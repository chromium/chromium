// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/signals_features.h"

namespace enterprise_signals::features {

BASE_FEATURE(kNewEvSignalsEnabled,
             "NewEvSignalsEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<bool> kDisableFileSystemInfo{
    &kNewEvSignalsEnabled, "DisableFileSystemInfo", false};
const base::FeatureParam<bool> kDisableSettings{&kNewEvSignalsEnabled,
                                                "DisableSettings", false};
const base::FeatureParam<bool> kDisableAntiVirus{&kNewEvSignalsEnabled,
                                                 "DisableAntiVirus", false};
const base::FeatureParam<bool> kDisableHotfix{&kNewEvSignalsEnabled,
                                              "DisableHotfix", false};

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
// Enables the consent promo for sharing device signal when a managed user
// signs in on an unmanaged device. This occurs after the sign-in intercept
// and before the sync promo (if enabled)
// This feature also requires UnmanagedDeviceSignalsConsentFlowEnabled policy to
// be enabled
BASE_FEATURE(kDeviceSignalsPromoAfterSigninIntercept,
             "DeviceSignalsPromoAfterSigninIntercept",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

bool IsNewFunctionEnabled(NewEvFunction new_ev_function) {
  // AntiVirus and Hotfix are considered "Launched". So only rely on the value
  // of the kill-switch to control the feature's behavior.
  bool disable_function = false;
  switch (new_ev_function) {
    case NewEvFunction::kFileSystemInfo:
      disable_function = kDisableFileSystemInfo.Get();
      break;
    case NewEvFunction::kSettings:
      disable_function = kDisableSettings.Get();
      break;
    case NewEvFunction::kAntiVirus:
      disable_function = kDisableAntiVirus.Get();
      break;
    case NewEvFunction::kHotfix:
      disable_function = kDisableHotfix.Get();
      break;
  }

  if (!base::FeatureList::IsEnabled(kNewEvSignalsEnabled)) {
    return false;
  }

  return !disable_function;
}

}  // namespace enterprise_signals::features
