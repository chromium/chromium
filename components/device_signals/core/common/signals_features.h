// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_SIGNALS_FEATURES_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_SIGNALS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace enterprise_signals::features {

// Feature flag for new private SecureConnect functions exposing additional
// device signals.
extern const base::Feature kNewEvSignalsEnabled;

// Feature parameters that can be used to turn off individual functions.
extern const base::FeatureParam<bool> kDisableFileSystemInfo;
extern const base::FeatureParam<bool> kDisableSettings;
extern const base::FeatureParam<bool> kDisableAntiVirus;
extern const base::FeatureParam<bool> kDisableHotfix;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
extern const base::Feature kDeviceSignalsPromoAfterSigninIntercept;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

// Enum used to map a given function to its kill switch.
enum class NewEvFunction { kFileSystemInfo, kSettings, kAntiVirus, kHotfix };

// Returns true if the function pointed at by `new_ev_function` is considered
// to be enabled based on the feature flag and its parameters.
bool IsNewFunctionEnabled(NewEvFunction new_ev_function);

}  // namespace enterprise_signals::features

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_SIGNALS_FEATURES_H_
