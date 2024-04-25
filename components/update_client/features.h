// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_FEATURES_H_
#define COMPONENTS_UPDATE_CLIENT_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include "base/metrics/field_trial_params.h"
#endif

namespace update_client::features {
#if BUILDFLAG(IS_MAC)
BASE_DECLARE_FEATURE(kDynamicCrxDownloaderPriority);
// The minimum size (in bytes) for which background downloads should be
// attempted.
extern const base::FeatureParam<int> kDynamicCrxDownloaderPrioritySizeThreshold;
#endif
}  // namespace update_client::features

#endif  // COMPONENTS_UPDATE_CLIENT_FEATURES_H_
