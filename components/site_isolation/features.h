// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SITE_ISOLATION_FEATURES_H_
#define COMPONENTS_SITE_ISOLATION_FEATURES_H_

#include "base/feature_list.h"

namespace site_isolation {
namespace features {

extern const base::Feature kSiteIsolationForPasswordSites;
extern const base::Feature kSiteIsolationForOAuthSites;
extern const base::Feature kSiteIsolationMemoryThresholds;
extern const char kStrictSiteIsolationMemoryThresholdParamName[];
extern const char kPartialSiteIsolationMemoryThresholdParamName[];

}  // namespace features
}  // namespace site_isolation

#endif  // COMPONENTS_SITE_ISOLATION_FEATURES_H_
