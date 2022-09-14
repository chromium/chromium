// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for the privacy sandbox UI. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

var PrivacySandboxDialogTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://privacy-sandbox-dialog/test_loader.html?module=privacy_sandbox/privacy_sandbox_dialog_test.js';
  }
};

TEST_F('PrivacySandboxDialogTest', 'All', function() {
  mocha.run();
});
