// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://sync-confirmation/sync_confirmation_app.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {SyncConfirmationAppElement} from 'chrome://sync-confirmation/sync_confirmation_app.js';
import {SyncConfirmationBrowserProxyImpl} from 'chrome://sync-confirmation/sync_confirmation_browser_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestSyncConfirmationBrowserProxy} from './test_sync_confirmation_browser_proxy.js';

[true, false].forEach(isModalDialogDesignEnabled => {
  const suiteDesignSuffix = isModalDialogDesignEnabled ? 'Modal' : 'NonModal';

  const STANDARD_CONSENT_CONFIRMATION = 'Yes, I\'m in';

  suite(`SigninSyncConfirmationTest${suiteDesignSuffix}`, function() {
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
      loadTimeData.overrideValues({
        isModalDialog: isModalDialogDesignEnabled,
      });
      document.body.innerHTML = '';
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
  suite(
      `SigninSyncConfirmationConsentRecordingTest${suiteDesignSuffix}`,
      function() {
        let app: SyncConfirmationAppElement;
        let browserProxy: TestSyncConfirmationBrowserProxy;

        setup(async function() {
          // This test suite makes comparisons with strings in their default
          // locale, which is en-US.
          assertEquals(
              'en-US', navigator.language,
              'Cannot verify strings for the ' + navigator.language +
                  'locale.');

          browserProxy = new TestSyncConfirmationBrowserProxy();
          SyncConfirmationBrowserProxyImpl.setInstance(browserProxy);
          loadTimeData.overrideValues(
              {isModalDialog: isModalDialogDesignEnabled});

          document.body.innerHTML = '';
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
          const [_, confirmation] = await browserProxy.whenCalled('confirm');
          assertEquals(STANDARD_CONSENT_CONFIRMATION, confirmation);
        });

        // Tests that the expected strings are recorded when clicking the
        // Confirm button.
        test('recordConsentOnSettingsLink', async function() {
          app.shadowRoot!.querySelector<HTMLElement>(
                             '#settingsButton')!.click();
          const [_, confirmation] =
              await browserProxy.whenCalled('goToSettings');
          // 'Sync settings' is recorded for non-modal design but this is passed
          // from the UI class so overriding loadTimeData does not help here.
          assertEquals('Settings', confirmation);
        });
      });
});
