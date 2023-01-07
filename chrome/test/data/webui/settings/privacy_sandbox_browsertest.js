// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for the privacy sandbox UI. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "build/build_config.h"');
GEN('#include "content/public/test/browser_test.h"');

var PrivacySandboxTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/privacy_sandbox_test.js';
  }
};

// TODO(crbug.com/1373779): Flaky on Mac.
GEN('#if BUILDFLAG(IS_MAC)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');
TEST_F('PrivacySandboxTest', 'MAYBE_All', function() {
  mocha.run();
});
