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
BASE_DECLARE_FEATURE(kSiteIsolationMemoryThresholds);
extern const char kStrictSiteIsolationMemoryThresholdParamName[];
extern const char kPartialSiteIsolationMemoryThresholdParamName[];

}  // namespace features
}  // namespace site_isolation

#endif  // COMPONENTS_SITE_ISOLATION_FEATURES_H_
