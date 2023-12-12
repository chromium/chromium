// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {CrostiniBrowserProxyImpl, GuestOsBrowserProxyImpl, SettingsCrostiniConfirmationDialogElement, SettingsCrostiniPageElement} from 'chrome://os-settings/lazy_load.js';
import {Router} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {disableAnimationsAndTransitions} from 'chrome://webui-test/test_api.js';

import {TestGuestOsBrowserProxy} from '../guest_os/test_guest_os_browser_proxy.js';

import {TestCrostiniBrowserProxy} from './test_crostini_browser_proxy.js';

let crostiniPage: SettingsCrostiniPageElement;
let guestOsBrowserProxy: TestGuestOsBrowserProxy;
let crostiniBrowserProxy: TestCrostiniBrowserProxy;

suite('<settings-crostini-page>', () => {
  setup(() => {
    loadTimeData.overrideValues({
      isCrostiniAllowed: true,
      isCrostiniSupported: true,
    });
    crostiniBrowserProxy = new TestCrostiniBrowserProxy();
    CrostiniBrowserProxyImpl.setInstanceForTesting(crostiniBrowserProxy);
    guestOsBrowserProxy = new TestGuestOsBrowserProxy();
    GuestOsBrowserProxyImpl.setInstanceForTesting(guestOsBrowserProxy);

    crostiniPage = document.createElement('settings-crostini-page');
    document.body.appendChild(crostiniPage);
    flush();

    disableAnimationsAndTransitions();
  });

  teardown(() => {
    crostiniPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  suite('<settings-crostini-confirmation-dialog>', () => {
    let dialog: SettingsCrostiniConfirmationDialogElement;
    let cancelOrCloseEvents: CustomEvent[];
    let closeEventPromise: Promise<Event>;

    setup(() => {
      cancelOrCloseEvents = [];
      dialog = document.createElement('settings-crostini-confirmation-dialog');

      dialog.addEventListener('cancel', (e: Event) => {
        cancelOrCloseEvents.push(e as CustomEvent);
      });
      closeEventPromise = new Promise(
          (resolve) => dialog.addEventListener('close', (e: Event) => {
            cancelOrCloseEvents.push(e as CustomEvent);
            resolve(e);
          }));

      document.body.appendChild(dialog);
    });

    teardown(() => {
      dialog.remove();
    });

    test('accept', async () => {
      let crDialogElement = dialog.shadowRoot!.querySelector('cr-dialog');
      assertTrue(!!crDialogElement);
      assertTrue(crDialogElement.open);
      const actionButton =
          dialog.shadowRoot!.querySelector<HTMLButtonElement>('.action-button');
      assertTrue(!!actionButton);
      actionButton.click();

      await closeEventPromise;
      assertEquals(1, cancelOrCloseEvents.length);
      assertEquals('close', cancelOrCloseEvents[0]!.type);
      assertTrue(cancelOrCloseEvents[0]!.detail.accepted);
      crDialogElement = dialog.shadowRoot!.querySelector('cr-dialog');
      assertTrue(!!crDialogElement);
      assertFalse(crDialogElement.open);
    });

    test('cancel', async () => {
      let crDialogElement = dialog.shadowRoot!.querySelector('cr-dialog');
      assertTrue(!!crDialogElement);
      assertTrue(crDialogElement.open);
      const cancelButton =
          dialog.shadowRoot!.querySelector<HTMLButtonElement>('.cancel-button');
      assertTrue(!!cancelButton);
      cancelButton.click();

      await closeEventPromise;
      assertEquals(2, cancelOrCloseEvents.length);
      assertEquals('cancel', cancelOrCloseEvents[0]!.type);
      assertEquals('close', cancelOrCloseEvents[1]!.type);
      assertFalse(cancelOrCloseEvents[1]!.detail.accepted);
      crDialogElement = dialog.shadowRoot!.querySelector('cr-dialog');
      assertTrue(!!crDialogElement);
      assertFalse(crDialogElement.open);
    });
  });
});
