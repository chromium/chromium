// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://sync-confirmation/sync_confirmation_app.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {SyncConfirmationAppElement} from 'chrome://sync-confirmation/sync_confirmation_app.js';
import {SyncConfirmationBrowserProxyImpl} from 'chrome://sync-confirmation/sync_confirmation_browser_proxy.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
// <if expr="lacros">
import {assertFalse} from 'chrome://webui-test/chai_assert.js';
// </if>
// <if expr="not lacros">
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
// </if>

import {TestSyncConfirmationBrowserProxy} from './test_sync_confirmation_browser_proxy.js';

[true, false].forEach(isNewDesignEnabled => {
  const suiteSuffix = isNewDesignEnabled ? 'NewDesign' : 'OldDesign';

  // <if expr="not lacros">
  const STANDARD_TITLE = 'Turn on sync?';
  const STANDARD_CONSENT_DESCRIPTION_TEXT = [
    STANDARD_TITLE,
    'Sync your bookmarks, passwords, history, and more on all your devices',
    'Google may use your history to personalize Search and other Google ' +
        'services',
  ];
  const STANDARD_CONSENT_CONFIRMATION = 'Yes, I\'m in';
  // </if>
  // <if expr="lacros">
  const STANDARD_TITLE = 'Chrome browser sync is on';
  const STANDARD_CONSENT_DESCRIPTION_TEXT = [
    STANDARD_TITLE,
    'Your bookmarks, passwords, history, and more are synced on all your ' +
        'devices',
    'Google may use your history to personalize Search and other Google ' +
        'services',
  ];
  const STANDARD_CONSENT_CONFIRMATION = 'Done';
  // </if>

  suite(`SigninSyncConfirmationTest${suiteSuffix}`, function() {
    let app: SyncConfirmationAppElement;

    setup(async function() {
      const browserProxy = new TestSyncConfirmationBrowserProxy();
      SyncConfirmationBrowserProxyImpl.setInstance(browserProxy);
      loadTimeData.overrideValues({isNewDesign: isNewDesignEnabled});
      document.body.innerHTML = '';
      app = document.createElement('sync-confirmation-app');
      document.body.append(app);
      // Check that the account image is requested when the app element is
      // attached to the document.
      await browserProxy.whenCalled('requestAccountInfo');
    });

    // Tests that no DCHECKS are thrown during initialization of the UI.
    test('LoadPage', function() {
      assertEquals(
          STANDARD_TITLE,
          app.shadowRoot!.querySelector(
                             '#syncConfirmationHeading')!.textContent!.trim());

      const cancelButton = app.shadowRoot!.querySelector(
          isNewDesignEnabled ? '#notNowButton' : '#cancelButton');
      // <if expr="lacros">
      // Test that the Cancel button is hidden on lacros.
      assertFalse(!!cancelButton);
      // </if>
      // <if expr="not lacros">
      assertTrue(!!cancelButton);
      // </if>
    });
  });

  // This test suite verifies that the consent strings recorded in various
  // scenarios are as expected. If the corresponding HTML file was updated
  // without also updating the attributes referring to consent strings,
  // this test will break.
  suite(`SigninSyncConfirmationConsentRecordingTest${suiteSuffix}`, function() {
    let app: SyncConfirmationAppElement;
    let browserProxy: TestSyncConfirmationBrowserProxy;

    setup(async function() {
      // This test suite makes comparisons with strings in their default locale,
      // which is en-US.
      assertEquals(
          'en-US', navigator.language,
          'Cannot verify strings for the ' + navigator.language + 'locale.');

      browserProxy = new TestSyncConfirmationBrowserProxy();
      SyncConfirmationBrowserProxyImpl.setInstance(browserProxy);
      loadTimeData.overrideValues({isNewDesign: isNewDesignEnabled});

      document.body.innerHTML = '';
      app = document.createElement('sync-confirmation-app');
      document.body.append(app);
      // Wait for the app element to get attached to the document (which is when
      // the account image gets requested).
      await browserProxy.whenCalled('requestAccountInfo');
    });

    // Tests that the expected strings are recorded when clicking the Confirm
    // button.
    test('recordConsentOnConfirm', async function() {
      app.shadowRoot!.querySelector<HTMLElement>('#confirmButton')!.click();
      const [description, confirmation] =
          await browserProxy.whenCalled('confirm');
      assertEquals(
          JSON.stringify(STANDARD_CONSENT_DESCRIPTION_TEXT),
          JSON.stringify(description));
      assertEquals(STANDARD_CONSENT_CONFIRMATION, confirmation);
    });

    // Tests that the expected strings are recorded when clicking the Confirm
    // button.
    test('recordConsentOnSettingsLink', async function() {
      app.shadowRoot!.querySelector<HTMLElement>('#settingsButton')!.click();
      const [description, confirmation] =
          await browserProxy.whenCalled('goToSettings');
      assertEquals(
          JSON.stringify(STANDARD_CONSENT_DESCRIPTION_TEXT),
          JSON.stringify(description));
      // 'Sync settings' is recorded for new design but this is passed from the
      // UI class so overriding loadTimeData does not help here.
      assertEquals('Settings', confirmation);
    });
  });
});
