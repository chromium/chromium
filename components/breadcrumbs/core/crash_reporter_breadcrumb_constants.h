// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREADCRUMBS_CORE_CRASH_REPORTER_BREADCRUMB_CONSTANTS_H_
#define COMPONENTS_BREADCRUMBS_CORE_CRASH_REPORTER_BREADCRUMB_CONSTANTS_H_

namespace breadcrumbs {

// The maximum string length for breadcrumbs data. The breadcrumbs size cannot
// be larger than the maximum length of a single product data value in the crash
// reporter: currently 2550 bytes in Breakpad (see doc comment in
// breakpad/src/common/long_string_dictionary.h) and 20480 bytes in Crashpad
// (see crashpad/client/annotation.h:kValueMaxSize). This value should be large
// enough to include enough events so that they are useful for diagnosing
// crashes.
constexpr unsigned long kMaxDataLength = 1530;

}  // namespace breadcrumbs

#endif  // COMPONENTS_BREADCRUMBS_CORE_CRASH_REPORTER_BREADCRUMB_CONSTANTS_H_
