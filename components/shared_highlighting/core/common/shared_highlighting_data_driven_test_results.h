// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_DATA_DRIVEN_TEST_RESULTS_H_
#define COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_DATA_DRIVEN_TEST_RESULTS_H_

namespace shared_highlighting {

// Struct for holding the results of the data-driven tests of Shared
// Highlighting.
struct SharedHighlightingDataDrivenTestResults {
  // Indicates that a link was generated for a data-driven test case.
  bool generation_success = false;
  // Indicates that the expected text was highlighted for a data-driven test
  // case.
  bool highlighting_success = false;
  // TODO(crbug.com/40835417): Add highlighted text to results for easier
  // failure diagosis
};
}  // namespace shared_highlighting

#endif  // COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_DATA_DRIVEN_TEST_RESULTS_H_