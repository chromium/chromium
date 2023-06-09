// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for the privacy sandbox UI. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "build/build_config.h"');
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

TEST_F('PrivacySandboxDialogSmallWindowTest', 'Consent', function() {
  runMochaSuite('PrivacySandboxDialogConsent');
});

TEST_F('PrivacySandboxDialogSmallWindowTest', 'Notice', function() {
  runMochaSuite('PrivacySandboxDialogNotice');
});

TEST_F('PrivacySandboxDialogSmallWindowTest', 'Combined', function() {
  runMochaSuite('PrivacySandboxDialogCombined');
});

TEST_F('PrivacySandboxDialogSmallWindowTest', 'NoticeEEA', function() {
  runMochaSuite('PrivacySandboxDialogNoticeEEA');
});

TEST_F('PrivacySandboxDialogSmallWindowTest', 'NoticeROW', function() {
  runMochaSuite('PrivacySandboxDialogNoticeROW');
});

TEST_F('PrivacySandboxDialogSmallWindowTest', 'Restricted', function() {
  runMochaSuite('PrivacySandboxDialogNoticeRestricted');
});

TEST_F('PrivacySandboxDialogSmallWindowTest', 'Mixin', function() {
  runMochaSuite('PrivacySandboxDialogMixin');
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

TEST_F('PrivacySandboxDialogBigWindowTest', 'Consent', function() {
  runMochaSuite('PrivacySandboxDialogConsent');
});

TEST_F('PrivacySandboxDialogBigWindowTest', 'Notice', function() {
  runMochaSuite('PrivacySandboxDialogNotice');
});

// TODO(https://crbug.com/1446188): Re-enable the test.
GEN('#if BUILDFLAG(IS_CHROMEOS)');
GEN('# define MAYBE_Combined DISABLED_Combined');
GEN('#else');
GEN('# define MAYBE_Combined Combined');
GEN('#endif');
TEST_F('PrivacySandboxDialogBigWindowTest', 'MAYBE_Combined', function() {
  runMochaSuite('PrivacySandboxDialogCombined');
});

// TODO(https://crbug.com/1446188): Re-enable the test.
GEN('#if BUILDFLAG(IS_CHROMEOS)');
GEN('# define MAYBE_NoticeEEA DISABLED_NoticeEEA');
GEN('#else');
GEN('# define MAYBE_NoticeEEA NoticeEEA');
GEN('#endif');
TEST_F('PrivacySandboxDialogBigWindowTest', 'MAYBE_NoticeEEA', function() {
  runMochaSuite('PrivacySandboxDialogNoticeEEA');
});

TEST_F('PrivacySandboxDialogBigWindowTest', 'NoticeROW', function() {
  runMochaSuite('PrivacySandboxDialogNoticeROW');
});

TEST_F('PrivacySandboxDialogBigWindowTest', 'Restricted', function() {
  runMochaSuite('PrivacySandboxDialogNoticeRestricted');
});

TEST_F('PrivacySandboxDialogBigWindowTest', 'Mixin', function() {
  runMochaSuite('PrivacySandboxDialogMixin');
});
