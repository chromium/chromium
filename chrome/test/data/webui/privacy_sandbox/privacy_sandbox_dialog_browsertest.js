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

// TODO(https://crbug.com/1446188): Re-enable the test.
GEN('#if BUILDFLAG(IS_MAC)');
GEN('# define MAYBE_SmallAll DISABLED_SmallAll');
GEN('#else');
GEN('# define MAYBE_SmallAll SmallAll');
GEN('#endif');

TEST_F('PrivacySandboxDialogSmallWindowTest', 'MAYBE_SmallAll', function() {
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

// TODO(https://crbug.com/1446188): Re-enable the test.
GEN('#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)');
GEN('# define MAYBE_All DISABLED_All');
GEN('#else');
GEN('# define MAYBE_All All');
GEN('#endif');

TEST_F('PrivacySandboxDialogBigWindowTest', 'MAYBE_All', function() {
  mocha.run();
});
