// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
GEN('#include "chrome/browser/ui/ui_features.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "services/network/public/cpp/features.h"');

var MetricsReporterTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    // MetricsReporter needs a host that enables BINDINGS_POLICY_MOJO_WEB_UI.
    // Any WebUI host should work, but chrome://webui-test or chrome://test
    // won't work.
    return 'chrome://new-tab-page/test_loader.html?module=metrics_reporter/metrics_reporter_test.js';
  }

  get isAsync() {
    return true;
  }
};

TEST_F('MetricsReporterTest', 'All', function() {
  mocha.run();
});
