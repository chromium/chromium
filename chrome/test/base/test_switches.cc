// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/test_switches.h"

namespace switches {

// Also emit full event trace logs for successful tests.
const char kAlsoEmitSuccessLogs[] = "also-emit-success-logs";

// Directory to output JavaScript code coverage. When supplied enables coverage
// in selected browser tests.
const char kDevtoolsCodeCoverage[] = "devtools-code-coverage";

// Show the mean value of histograms that native performance tests
// are monitoring. Note that this is only applicable for PerformanceTest
// subclasses.
const char kPerfTestPrintUmaMeans[] = "perf-test-print-uma-means";

}  // namespace switches
