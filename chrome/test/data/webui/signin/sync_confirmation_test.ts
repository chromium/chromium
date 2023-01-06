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
const isTangibleSync = loadTimeData.getBoolean('isTangibleSync');

suite(`SigninSyncConfirmationTest`, function() {
  let app: SyncConfirmationAppElement;
  let browserProxy: TestSyncConfirmationBrowserProxy;

  function getCancelButtonID() {
    return isTangibleSync || !isModalDialogDesignEnabled ? '#notNowButton' :
                                                           '#cancelButton';
  }

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
        app.shadowRoot!.querySelector<HTMLElement>(getCancelButtonID());
    assertFalse(cancelButton!.hidden);
  });

  // Tests clicking on confirm button.
  test('ConfirmClicked', async function() {
    testButtonClick('#confirmButton');
    await browserProxy.whenCalled('confirm');
  });

  // Tests clicking on cancel button.
  test('CancelClicked', async function() {
    testButtonClick(getCancelButtonID());
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

  function getConsentDescriptionTexts(i18n: Function) {
    const consentDescriptionTexts = [
      i18n('syncConfirmationTitle'),
      i18n('syncConfirmationSyncInfoTitle'),
    ];

    if (isTangibleSync) {
      const syncBenefitsList =
          JSON.parse(loadTimeData.getString('syncBenefitsList'));

      for (let i = 0; i < syncBenefitsList.length; i++) {
        consentDescriptionTexts.push(i18n(syncBenefitsList[i].title));
      }
      consentDescriptionTexts.push(i18n('syncConfirmationSyncInfoDesc'));
      return consentDescriptionTexts;
    }

    if (!isModalDialogDesignEnabled ||
        (isModalDialogDesignEnabled && !isSigninInterceptFreEnabled)) {
      consentDescriptionTexts.push(i18n('syncConfirmationSyncInfoDesc'));
    }
    return consentDescriptionTexts;
  }

  setup(async function() {
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
    const i18n = app.i18n.bind(app);

    app.shadowRoot!.querySelector<HTMLElement>('#confirmButton')!.click();
    const [description, confirmation] =
        await browserProxy.whenCalled('confirm');

    assertEquals(i18n('syncConfirmationConfirmLabel'), confirmation);
    assertArrayEquals(getConsentDescriptionTexts(i18n), description);
  });

  // Tests that the expected strings are recorded when clicking the
  // Settings button.
  test('recordConsentOnSettingsLink', async function() {
    const i18n = app.i18n.bind(app);

    app.shadowRoot!.querySelector<HTMLElement>('#settingsButton')!.click();
    const [description, confirmation] =
        await browserProxy.whenCalled('goToSettings');

    assertEquals(i18n('syncConfirmationSettingsLabel'), confirmation);
    assertArrayEquals(getConsentDescriptionTexts(i18n), description);
  });
});
