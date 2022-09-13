// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_UTIL_H_
#define COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_UTIL_H_

namespace base {
class TimeTicks;
}

namespace breadcrumbs {

// Returns the time when breadcrumbs logging started, for use in calculating
// breadcrumbs timestamps.
base::TimeTicks GetStartTime();

}  // namespace breadcrumbs

#endif  // COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_UTIL_H_
