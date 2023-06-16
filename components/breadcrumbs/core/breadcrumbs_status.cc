// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumbs_status.h"

#include "base/feature_list.h"

#include <atomic>

namespace breadcrumbs {

namespace {

// If true, breadcrumbs is forced to enabled for testing purposes. If false,
// breadcrumbs is in its default state.
std::atomic<bool> is_enabled_for_testing;

BASE_FEATURE(kLogBreadcrumbs,
             "LogBreadcrumbs",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

bool IsEnabled() {
  if (is_enabled_for_testing) {
    return true;
  }
  return base::FeatureList::IsEnabled(breadcrumbs::kLogBreadcrumbs);
}

ScopedEnableBreadcrumbsForTesting::ScopedEnableBreadcrumbsForTesting() {
  is_enabled_for_testing = true;
}

ScopedEnableBreadcrumbsForTesting::~ScopedEnableBreadcrumbsForTesting() {
  is_enabled_for_testing = false;
}

}  // namespace breadcrumbs
