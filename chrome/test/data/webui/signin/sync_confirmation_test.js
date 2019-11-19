// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://sync-confirmation/sync_confirmation_app.js';

import {SyncConfirmationBrowserProxyImpl} from 'chrome://sync-confirmation/sync_confirmation_browser_proxy.js';
import {TestSyncConfirmationBrowserProxy} from './test_sync_confirmation_browser_proxy.js';

suite('SigninSyncConfirmationTest', function() {
  let app;
  setup(function() {
    const browserProxy = new TestSyncConfirmationBrowserProxy();
    SyncConfirmationBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    app = document.createElement('sync-confirmation-app');
    document.body.append(app);
    // Check that the account image is requested when the app element is
    // attached to the document.
    return browserProxy.whenCalled('requestAccountImage');
  });

  // Tests that no DCHECKS are thrown during initialization of the UI.
  test('LoadPage', function() {
    assertEquals(
        'Turn on sync?', app.$.syncConfirmationHeading.textContent.trim());
  });
});

// This test suite verifies that the consent strings recorded in various
// scenarios are as expected. If the corresponding HTML file was updated
// without also updating the attributes referring to consent strings,
// this test will break.
suite('SigninSyncConfirmationConsentRecordingTest', function() {
  let app;
  let browserProxy;

  setup(function() {
    // This test suite makes comparisons with strings in their default locale,
    // which is en-US.
    assertEquals(
        'en-US', navigator.language,
        'Cannot verify strings for the ' + navigator.language + 'locale.');

    browserProxy = new TestSyncConfirmationBrowserProxy();
    SyncConfirmationBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();
    app = document.createElement('sync-confirmation-app');
    document.body.append(app);
  });

  const STANDARD_CONSENT_DESCRIPTION_TEXT = [
    'Turn on sync?',
    'Sync your bookmarks, passwords, history, and more on all your devices',
    'Google may use your history to personalize Search, ads, and other ' +
        'Google services',
  ];


  // Tests that the expected strings are recorded when clicking the Confirm
  // button.
  test('recordConsentOnConfirm', function() {
    app.$$('#confirmButton').click();
    return browserProxy.whenCalled('confirm').then(function(
        [description, confirmation]) {
      assertEquals(
          JSON.stringify(STANDARD_CONSENT_DESCRIPTION_TEXT),
          JSON.stringify(description));
      assertEquals('Yes, I\'m in', confirmation);
    });
  });

  // Tests that the expected strings are recorded when clicking the Confirm
  // button.
  test('recordConsentOnSettingsLink', function() {
    app.$$('#settingsButton').click();
    return browserProxy.whenCalled('goToSettings').then(function([
      description, confirmation
    ]) {
      assertEquals(
          JSON.stringify(STANDARD_CONSENT_DESCRIPTION_TEXT),
          JSON.stringify(description));
      assertEquals('Settings', confirmation);
    });
  });
});
