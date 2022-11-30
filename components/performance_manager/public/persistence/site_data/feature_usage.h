// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERSISTENCE_SITE_DATA_FEATURE_USAGE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERSISTENCE_SITE_DATA_FEATURE_USAGE_H_

namespace performance_manager {

// A tri-state return value for site feature usage. If a definitive decision
// can't be made then an "unknown" result can be returned.
enum class SiteFeatureUsage {
  kSiteFeatureNotInUse,
  kSiteFeatureInUse,
  kSiteFeatureUsageUnknown,
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERSISTENCE_SITE_DATA_FEATURE_USAGE_H_
