// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://help-app.
 */

GEN('#include "chromeos/components/help_app_ui/test/help_app_ui_browsertest.h"');

GEN('#include "chromeos/constants/chromeos_features.h"');
GEN('#include "content/public/test/browser_test.h"');

const HOST_ORIGIN = 'chrome://help-app';

var HelpAppUIBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return HOST_ORIGIN;
  }

  /** @override */
  get extraLibraries() {
    return [
      ...super.extraLibraries,
      '//chromeos/components/help_app_ui/test/driver.js',
      '//ui/webui/resources/js/assert.js',
    ];
  }

  /** @override */
  get isAsync() {
    return true;
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'chromeos::features::kHelpAppSearchServiceIntegration',
        'chromeos::features::kEnableLocalSearchService',
      ]
    };
  }

  /** @override */
  get typedefCppFixture() {
    return 'HelpAppUiBrowserTest';
  }

  /** @override */
  get runAccessibilityChecks() {
    return false;
  }
};

// Tests that chrome://help-app goes somewhere instead of 404ing or crashing.
TEST_F('HelpAppUIBrowserTest', 'HasChromeSchemeURL', () => {
  const guest = /** @type {!HTMLIFrameElement} */ (
      document.querySelector('iframe'));

  assertEquals(document.location.origin, HOST_ORIGIN);
  assertEquals(guest.src, GUEST_ORIGIN + '/');
  testDone();
});

// Tests that we have localised information in the HTML like title and lang.
TEST_F('HelpAppUIBrowserTest', 'HasTitleAndLang', () => {
  assertEquals(document.documentElement.lang, 'en');
  assertEquals(document.title, 'Explore');
  testDone();
});

// Test cases injected into the guest context.
// See implementations in help_app_guest_ui_browsertest.js.

TEST_F('HelpAppUIBrowserTest', 'GuestHasLang', async () => {
  await runTestInGuest('GuestHasLang');
  testDone();
});

TEST_F('HelpAppUIBrowserTest', 'GuestCanSearchWithHeadings', async () => {
  await runTestInGuest('GuestCanSearchWithHeadings');
  testDone();
});

TEST_F('HelpAppUIBrowserTest', 'GuestCanClearSearchIndex', async () => {
  await runTestInGuest('GuestCanClearSearchIndex');
  testDone();
});
