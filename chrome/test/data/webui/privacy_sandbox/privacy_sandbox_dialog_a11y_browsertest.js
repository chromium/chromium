// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test bodies are mostly empty because they are used to sanity check that the
// correct element was loaded. A11y testing is performed by axe-core. It runs
// a set of checks of the loaded page to verify that it is accessible.

// See docs/website/site/developers/accessibility/testing/axe-core/index.md for
// documentation on a11y web UI tests and examples.

GEN_INCLUDE([
  '//chrome/test/data/webui/a11y/accessibility_test.js',
  '//chrome/test/data/webui/polymer_browser_test_base.js',
]);

GEN('#include "content/public/test/browser_test.h"');

PrivacySandboxCombinedDialogA11yTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://privacy-sandbox-dialog/combined?debug';
  }

  /** @override */
  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }
};

AccessibilityTest.define('PrivacySandboxCombinedDialogA11yTest', {
  name: 'PrivacySandboxCombinedDialogA11yTest',
  tests: {
    'Consent': function() {
      // Verify that the combined dialog was loaded with a consent step active.
      const consentStep =
          document.body.querySelector('privacy-sandbox-combined-dialog-app')
              .shadowRoot.querySelector('privacy-sandbox-dialog-consent-step');
      assertTrue(!!consentStep);
      assertTrue(consentStep.classList.contains('active'));
    },
  },
});

PrivacySandboxNoticeEeaDialogA11yTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://privacy-sandbox-dialog/combined?step=notice&debug';
  }

  /** @override */
  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }
};

AccessibilityTest.define('PrivacySandboxNoticeEeaDialogA11yTest', {
  name: 'PrivacySandboxNoticeEeaDialogA11yTest',
  tests: {
    'NoticeEea': function() {
      // Verify that the combined dialog was loaded with a notice step active.
      const noticeStep =
          document.body.querySelector('privacy-sandbox-combined-dialog-app')
              .shadowRoot.querySelector('privacy-sandbox-dialog-notice-step');
      assertTrue(!!noticeStep);
      assertTrue(noticeStep.classList.contains('active'));
    },
  },
});

PrivacySandboxNoticeRowDialogA11yTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://privacy-sandbox-dialog/notice?debug';
  }

  /** @override */
  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }
};

AccessibilityTest.define('PrivacySandboxNoticeRowDialogA11yTest', {
  name: 'PrivacySandboxNoticeRowDialogA11yTest',
  tests: {
    'NoticeRow': function() {
      // Verify that notice dialog was loaded.
      const noticeApp =
          document.body.querySelector('privacy-sandbox-notice-dialog-app');
      assertTrue(!!noticeApp);
      assertTrue(!!noticeApp.shadowRoot.querySelector('#settingsButton'));
      assertTrue(!!noticeApp.shadowRoot.querySelector('#ackButton'));
    },
  },
});
