// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/signals_features.h"

namespace enterprise_signals::features {

BASE_FEATURE(kAllowClientCertificateReportingForUsers,
             "AllowClientCertificateReportingForUsers",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
// Enables the triggering of device signals consent dialog when conditions met
// This feature also requires UnmanagedDeviceSignalsConsentFlowEnabled policy to
// be enabled
BASE_FEATURE(kDeviceSignalsConsentDialog,
             "DeviceSignalsConsentDialog",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsConsentDialogEnabled() {
  return base::FeatureList::IsEnabled(kDeviceSignalsConsentDialog);
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_CHROMEOS_ASH)

BASE_FEATURE(kNewEvSignalsUnaffiliatedEnabled,
             "NewEvSignalsUnaffiliatedEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClearClientCertsOnExtensionReport,
             "ClearClientCertsOnExtensionReport",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsClearClientCertsOnExtensionReportEnabled() {
  return base::FeatureList::IsEnabled(kClearClientCertsOnExtensionReport);
}

}  // namespace enterprise_signals::features
