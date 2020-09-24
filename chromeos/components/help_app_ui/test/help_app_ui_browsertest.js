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
  get typedefCppFixture() {
    return 'HelpAppUiBrowserTest';
  }

  /** @override */
  get runAccessibilityChecks() {
    return false;
  }
};

const toString16 = s => ({data: Array.from(s, c => c.charCodeAt())});

// Tests that chrome://help-app goes somewhere instead of 404ing or crashing.
TEST_F('HelpAppUIBrowserTest', 'HasChromeSchemeURL', () => {
  const guest = document.querySelector('iframe');

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

// Tests that we can make calls to the LSS to search.
TEST_F('HelpAppUIBrowserTest', 'CanSearchViaLSSIndex', async () => {
  const result = await indexRemote.find(toString16('search string!'), 100);

  // Status 3 corresponds to kEmptyIndex.
  // https://source.chromium.org/chromium/chromium/src/+/master:chromeos/components/local_search_service/mojom/types.mojom;drc=c2c84a5ac7711dedcc0b7ff9e79bf7f2da019537;l=72
  assertEquals(result.status, 3);
  assertEquals(result.results, null);
  testDone();
});

// Test cases injected into the guest context.
// See implementations in help_app_guest_ui_browsertest.js.

TEST_F('HelpAppUIBrowserTest', 'GuestHasLang', async () => {
  await runTestInGuest('GuestHasLang');
  testDone();
});
