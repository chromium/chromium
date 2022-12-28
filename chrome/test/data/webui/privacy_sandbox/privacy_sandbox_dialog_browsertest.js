// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for the privacy sandbox UI. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');
GEN('#include "chrome/browser/ui/views/frame/browser_view.h"')
GEN('#include "ui/views/widget/widget.h"')

var PrivacySandboxDialogSmallWindowTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://privacy-sandbox-dialog/test_loader.html?module=privacy_sandbox/privacy_sandbox_dialog_test.js';
  }

  /** @override */
  testGenPreamble() {
    // Force the window size to be small. The dialog will start with a
    // scrollbar.
    GEN(`
    BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->SetBounds(
      {0, 0, 620, 600});
    `);
  }
};

TEST_F('PrivacySandboxDialogSmallWindowTest', 'All', function() {
  mocha.run();
});

var PrivacySandboxDialogBigWindowTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://privacy-sandbox-dialog/test_loader.html?module=privacy_sandbox/privacy_sandbox_dialog_test.js';
  }

  /** @override */
  testGenPreamble() {
    // Force the window size to be bigger. The dialog will start without a
    // scrollbar.
    GEN(`
    BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->SetBounds(
      {0, 0, 620, 900});
    `);
  }
};

TEST_F('PrivacySandboxDialogBigWindowTest', 'All', function() {
  mocha.run();
});
