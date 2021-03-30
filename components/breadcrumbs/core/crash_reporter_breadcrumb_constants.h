// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREADCRUMBS_CORE_CRASH_REPORTER_BREADCRUMB_CONSTANTS_H_
#define COMPONENTS_BREADCRUMBS_CORE_CRASH_REPORTER_BREADCRUMB_CONSTANTS_H_

namespace breadcrumbs {

// The maximum string length for breadcrumbs data. The breadcrumbs size cannot
// be larger than the maximum length of a single Breakpad product data value
// (currently 2550 bytes). This value should be large enough to include enough
//  events so that they are useful for diagnosing crashes.
constexpr unsigned long kMaxDataLength = 1530;

}  // namespace breadcrumbs

#endif  // COMPONENTS_BREADCRUMBS_CORE_CRASH_REPORTER_BREADCRUMB_CONSTANTS_H_
