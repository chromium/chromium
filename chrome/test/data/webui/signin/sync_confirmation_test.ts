// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://sync-confirmation/sync_confirmation_app.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {SyncConfirmationAppElement} from 'chrome://sync-confirmation/sync_confirmation_app.js';
import {SyncConfirmationBrowserProxyImpl} from 'chrome://sync-confirmation/sync_confirmation_browser_proxy.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestSyncConfirmationBrowserProxy} from './test_sync_confirmation_browser_proxy.js';

const isModalDialogDesignEnabled = loadTimeData.getBoolean('isModalDialog');
const isSigninInterceptFreEnabled =
    loadTimeData.getBoolean('isSigninInterceptFre');

const STANDARD_CONSENT_CONFIRMATION = 'Yes, I\'m in';
const SIGNIN_INTERCEPT_CONSENT_CONFIRMATION = 'Turn on sync';

const SETTINGS_CONSENT_CONFIRMATION = 'Settings';
const SYNC_SETTINGS_CONSENT_CONFIRMATION = 'Sync settings';

const CONSENT_DESCRIPTION_TEXTS = [
  'Turn on sync?',
  'Sync your bookmarks, passwords, history, and more on all your devices',
  'Google may use your history to personalize Search and other Google services',
];
const CONSENT_DESCRIPTION_SIGNIN_INTERCEPT_TEXTS = [
  'Welcome, Person 1',
  'Turn on sync to get your bookmarks, passwords, history, and more on this device and anywhere else you\'re syncing. Google may use your history to personalize Search and other Google services.',
];

// <if expr="chromeos_lacros">
CONSENT_DESCRIPTION_TEXTS[0] = 'Turn on Chrome browser sync?';
// </if>

suite(`SigninSyncConfirmationTest`, function() {
  let app: SyncConfirmationAppElement;
  let browserProxy: TestSyncConfirmationBrowserProxy;

  function testButtonClick(buttonSelector: string) {
    const allButtons =
        Array.from(app.shadowRoot!.querySelectorAll('cr-button')) as
        CrButtonElement[];
    const actionButton =
        app.shadowRoot!.querySelector(buttonSelector) as CrButtonElement;
    const spinner = app.shadowRoot!.querySelector('paper-spinner-lite');

    allButtons.forEach(button => assertFalse(button.disabled));
    assertFalse(spinner!.active);

    actionButton.click();

    allButtons.forEach(button => assertTrue(button.disabled));
    assertTrue(spinner!.active);
  }

  setup(async function() {
    browserProxy = new TestSyncConfirmationBrowserProxy();
    SyncConfirmationBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('sync-confirmation-app');
    document.body.append(app);
    // Check that the account image is requested when the app element is
    // attached to the document.
    await browserProxy.whenCalled('requestAccountInfo');
  });

  // Tests that no DCHECKS are thrown during initialization of the UI.
  test('LoadPage', function() {
    const cancelButton =
        app.shadowRoot!.querySelector(
            isModalDialogDesignEnabled ? '#cancelButton' : '#notNowButton') as
        HTMLElement;
    assertFalse(cancelButton!.hidden);
  });

  // Tests clicking on confirm button.
  test('ConfirmClicked', async function() {
    testButtonClick('#confirmButton');
    await browserProxy.whenCalled('confirm');
  });

  // Tests clicking on cancel button.
  test('CancelClicked', async function() {
    testButtonClick(
        isModalDialogDesignEnabled ? '#cancelButton' : '#notNowButton');
    await browserProxy.whenCalled('undo');
  });

  // Tests clicking on settings button.
  test('SettingsClicked', async function() {
    testButtonClick('#settingsButton');
    await browserProxy.whenCalled('goToSettings');
  });
});

// This test suite verifies that the consent strings recorded in various
// scenarios are as expected. If the corresponding HTML file was updated
// without also updating the attributes referring to consent strings,
// this test will break.
suite(`SigninSyncConfirmationConsentRecordingTest`, function() {
  let app: SyncConfirmationAppElement;
  let browserProxy: TestSyncConfirmationBrowserProxy;

  setup(async function() {
    // This test suite makes comparisons with strings in their default
    // locale, which is en-US.
    assertEquals(
        'en-US', navigator.language,
        'Cannot verify strings for the ' + navigator.language + 'locale.');

    browserProxy = new TestSyncConfirmationBrowserProxy();
    SyncConfirmationBrowserProxyImpl.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('sync-confirmation-app');
    document.body.append(app);
    // Wait for the app element to get attached to the document (which is
    // when the account image gets requested).
    await browserProxy.whenCalled('requestAccountInfo');
  });

  // Tests that the expected strings are recorded when clicking the
  // Confirm button.
  test('recordConsentOnConfirm', async function() {
    app.shadowRoot!.querySelector<HTMLElement>('#confirmButton')!.click();
    const [description, confirmation] =
        await browserProxy.whenCalled('confirm');
    if (isSigninInterceptFreEnabled) {
      // Confirmation button is capitalized on MacOS.
      assertEquals(
          SIGNIN_INTERCEPT_CONSENT_CONFIRMATION.toLowerCase(),
          confirmation.toLowerCase());
    } else {
      assertEquals(STANDARD_CONSENT_CONFIRMATION, confirmation);
    }
    assertArrayEquals(
        isSigninInterceptFreEnabled ?
            CONSENT_DESCRIPTION_SIGNIN_INTERCEPT_TEXTS :
            CONSENT_DESCRIPTION_TEXTS,
        description);
  });

  // Tests that the expected strings are recorded when clicking the
  // Settings button.
  test('recordConsentOnSettingsLink', async function() {
    app.shadowRoot!.querySelector<HTMLElement>('#settingsButton')!.click();
    const [description, confirmation] =
        await browserProxy.whenCalled('goToSettings');
    if (isModalDialogDesignEnabled) {
      assertEquals(SETTINGS_CONSENT_CONFIRMATION, confirmation);
    } else {
      assertEquals(SYNC_SETTINGS_CONSENT_CONFIRMATION, confirmation);
    }
    assertArrayEquals(
        isSigninInterceptFreEnabled ?
            CONSENT_DESCRIPTION_SIGNIN_INTERCEPT_TEXTS :
            CONSENT_DESCRIPTION_TEXTS,
        description);
  });
});
