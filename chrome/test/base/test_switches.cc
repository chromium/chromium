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

// Makes browser tests create the omnibox popups' WebUI content as production
// does, instead of only when it is needed. Used to compare test runtimes and as
// a kill switch for the on-demand creation. Must be passed on the test launcher
// command line: `InProcessBrowserTest` reads it in its constructor, so
// appending it in `SetUpCommandLine()` has no effect. To change this for one
// fixture, use `omnibox::SetCreatePopupWebUIOnDemandForTesting()` instead.
const char kDisableOmniboxPopupOnDemandCreation[] =
    "disable-omnibox-popup-on-demand-creation";

// Show the mean value of histograms that native performance tests
// are monitoring. Note that this is only applicable for PerformanceTest
// subclasses.
const char kPerfTestPrintUmaMeans[] = "perf-test-print-uma-means";

}  // namespace switches
