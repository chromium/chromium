// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_PROTECTION_FEATURES_H_
#define COMPONENTS_ENTERPRISE_DATA_PROTECTION_FEATURES_H_
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace enterprise_data_protection {

BASE_DECLARE_FEATURE(kEnableDeepScanVerdictCacheSize);

BASE_DECLARE_FEATURE_PARAM(size_t, kVerdictCacheMaxSize);

}  // namespace enterprise_data_protection

#endif  // COMPONENTS_ENTERPRISE_DATA_PROTECTION_FEATURES_H_
