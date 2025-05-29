// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SITE_ISOLATION_FEATURES_H_
#define COMPONENTS_SITE_ISOLATION_FEATURES_H_

#include "base/feature_list.h"

namespace site_isolation {
namespace features {

BASE_DECLARE_FEATURE(kSiteIsolationForPasswordSites);
BASE_DECLARE_FEATURE(kSiteIsolationForOAuthSites);

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kSiteIsolationMemoryThresholdsAndroid);
extern const char kStrictSiteIsolationMemoryThresholdParamName[];
extern const char kPartialSiteIsolationMemoryThresholdParamName[];
#endif  // BUIDLFLAG(IS_ANDROID)

BASE_DECLARE_FEATURE(kOriginIsolationMemoryThreshold);
extern const char kOriginIsolationMemoryThresholdParamName[];

}  // namespace features
}  // namespace site_isolation

#endif  // COMPONENTS_SITE_ISOLATION_FEATURES_H_
