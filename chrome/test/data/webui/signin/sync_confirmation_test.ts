// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://sync-confirmation/sync_confirmation_app.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {SyncConfirmationAppElement} from 'chrome://sync-confirmation/sync_confirmation_app.js';
import {ScreenMode, SyncConfirmationBrowserProxyImpl} from 'chrome://sync-confirmation/sync_confirmation_browser_proxy.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSyncConfirmationBrowserProxy} from './test_sync_confirmation_browser_proxy.js';

suite(`SigninSyncConfirmationTest`, function() {
  let app: SyncConfirmationAppElement;
  let browserProxy: TestSyncConfirmationBrowserProxy;

  async function testButtonClick(buttonSelector: string) {
    const allButtons =
        Array.from(app.shadowRoot!.querySelectorAll('cr-button'));
    const actionButton =
        app.shadowRoot!.querySelector<HTMLElement>(buttonSelector);

    allButtons.forEach(button => assertFalse(button.disabled));
    assertFalse(!!app.shadowRoot!.querySelector('.spinner'));

    assertTrue(!!actionButton);
    actionButton.click();
    await microtasksFinished();

    allButtons.forEach(button => assertTrue(button.disabled));
    assertTrue(!!app.shadowRoot!.querySelector('.spinner'));
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

  // Tests that the buttons are initially hidden, pending minor-mode compliance
  // configuration.
  test('LoadPage', function() {
    const cancelButton =
        app.shadowRoot!.querySelector<HTMLElement>('#notNowButton');
    assertFalse(cancelButton!.hidden);
    assertTrue(cancelButton!.classList.contains('visibility-hidden'));

    const confirmButton =
        app.shadowRoot!.querySelector<HTMLElement>('#confirmButton');
    assertFalse(confirmButton!.hidden);
    assertTrue(confirmButton!.classList.contains('visibility-hidden'));
  });

  // Tests clicking on confirm button.
  test('ConfirmClicked', async function() {
    await testButtonClick('#confirmButton');
    await browserProxy.whenCalled('confirm');
  });

  // Tests clicking on cancel button.
  test('CancelClicked', async function() {
    await testButtonClick('#notNowButton');
    await browserProxy.whenCalled('undo');
  });

  // Tests clicking on settings button.
  test('SettingsClicked', async function() {
    await testButtonClick('#settingsButton');
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

    const syncBenefitsList =
        JSON.parse(loadTimeData.getString('syncBenefitsList'));

    for (let i = 0; i < syncBenefitsList.length; i++) {
      consentDescriptionTexts.push(i18n(syncBenefitsList[i].title));
    }
    consentDescriptionTexts.push(i18n('syncConfirmationSyncInfoDesc'));

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
    webUIListenerCallback('screen-mode-changed', ScreenMode.RESTRICTED);

    app.shadowRoot!.querySelector<HTMLElement>('#confirmButton')!.click();
    const [description, confirmation, screenMode] =
        await browserProxy.whenCalled('confirm');

    assertEquals(i18n('syncConfirmationConfirmLabel'), confirmation);
    assertArrayEquals(getConsentDescriptionTexts(i18n), description);
    assertEquals(ScreenMode.RESTRICTED, screenMode);
  });

  // Tests that the expected strings are recorded when clicking the
  // Settings button.
  test('recordConsentOnSettingsLink', async function() {
    const i18n = app.i18n.bind(app);
    webUIListenerCallback('screen-mode-changed', ScreenMode.RESTRICTED);

    app.shadowRoot!.querySelector<HTMLElement>('#settingsButton')!.click();
    const [description, confirmation, screenMode] =
        await browserProxy.whenCalled('goToSettings');

    assertEquals(i18n('syncConfirmationSettingsLabel'), confirmation);
    assertArrayEquals(getConsentDescriptionTexts(i18n), description);
    assertEquals(ScreenMode.RESTRICTED, screenMode);
  });

  // Tests that the expected strings are recorded when clicking the
  // Settings button.
  test('passScreenModeOnUndo', async function() {
    webUIListenerCallback('screen-mode-changed', ScreenMode.RESTRICTED);

    app.shadowRoot!.querySelector<HTMLElement>('#notNowButton')!.click();
    const [screenMode] = await browserProxy.whenCalled('undo');

    assertEquals(ScreenMode.RESTRICTED, screenMode);
  });
});
