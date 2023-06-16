// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREADCRUMBS_CORE_BREADCRUMBS_STATUS_H_
#define COMPONENTS_BREADCRUMBS_CORE_BREADCRUMBS_STATUS_H_

namespace breadcrumbs {

// Returns true if breadcrumbs logging is enabled. Note that if metrics consent
// was not provided, this will return true but breadcrumbs will not actually be
// uploaded or persisted to disk.
bool IsEnabled();

// Forces `breadcrumbs::IsEnabled()` to return true while it exists. Returns
// breadcrumbs to its default state once destroyed.
class ScopedEnableBreadcrumbsForTesting {
 public:
  ScopedEnableBreadcrumbsForTesting();
  ~ScopedEnableBreadcrumbsForTesting();
};

}  // namespace breadcrumbs

#endif  // COMPONENTS_BREADCRUMBS_CORE_BREADCRUMBS_STATUS_H_
