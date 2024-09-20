// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_SIGNALS_FEATURES_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_SIGNALS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace enterprise_signals::features {

// Allows the reporting of client certificates for managed users.
BASE_DECLARE_FEATURE(kAllowClientCertificateReportingForUsers);

// Feature flag for new private SecureConnect functions exposing additional
// device signals.
BASE_DECLARE_FEATURE(kNewEvSignalsEnabled);

// Feature parameters that can be used to turn off individual functions.
extern const base::FeatureParam<bool> kDisableFileSystemInfo;
extern const base::FeatureParam<bool> kDisableSettings;
extern const base::FeatureParam<bool> kDisableAntiVirus;
extern const base::FeatureParam<bool> kDisableHotfix;

// Enum used to map a given function to its kill switch.
enum class NewEvFunction { kFileSystemInfo, kSettings, kAntiVirus, kHotfix };

// Returns true if the function pointed at by `new_ev_function` is considered
// to be enabled based on the feature flag and its parameters.
bool IsNewFunctionEnabled(NewEvFunction new_ev_function);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
BASE_DECLARE_FEATURE(kDeviceSignalsConsentDialog);

// Returns true if device signals consent dialog has been enabled for
// consent collection.
bool IsConsentDialogEnabled();
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_CHROMEOS_ASH)

// Feature flag for supporting the new private SecureConnect functions in
// unaffiliated contexts via the consent flow.
BASE_DECLARE_FEATURE(kNewEvSignalsUnaffiliatedEnabled);

// Feature flag to clear cached client certificates for given URLs when
// the private enterprise.reportingPrivate.getCertificate extension API
// is invoked.
BASE_DECLARE_FEATURE(kClearClientCertsOnExtensionReport);

// Returns true if `kClearClientCertsOnExtensionReport` is enabled.
bool IsClearClientCertsOnExtensionReportEnabled();

}  // namespace enterprise_signals::features

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_SIGNALS_FEATURES_H_
