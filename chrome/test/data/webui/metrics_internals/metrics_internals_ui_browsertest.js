// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the chrome://metrics-internals WebUI page.
 * Unlike most other WebUI tests under the chrome/test/data/webui directory,
 * this file tests both the frontend and the backend intentionally.
 */

GEN('#include "chrome/test/data/webui/metrics_internals/metrics_internals_ui_browsertest.h"');
GEN('#include "content/public/test/browser_test.h"');

var MetricsInternalsUIBrowserTest = class extends testing.Test {
  /** @override */
  get isAsync() {
    return true;
  }

  /** @override */
  get webuiHost() {
    return 'metrics-internals';
  }
};

var MetricsInternalsUIBrowserTestWithoutLog =
    class extends MetricsInternalsUIBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://metrics-internals/test_loader.html?module=metrics_internals/no_logs_test.js';
  }
}

TEST_F('MetricsInternalsUIBrowserTestWithoutLog', 'All', function() {
  mocha.run();
});

var MetricsInternalsUIBrowserTestWithLog =
    class extends MetricsInternalsUIBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://metrics-internals/test_loader.html?module=metrics_internals/with_log_test.js';
  }

  /** @override */
  get typedefCppFixture() {
    return 'MetricsInternalsUIBrowserTest';
  }
};

TEST_F('MetricsInternalsUIBrowserTestWithLog', 'All', function() {
  mocha.run();
});
