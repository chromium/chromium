// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {CrostiniBrowserProxyImpl, GuestOsBrowserProxyImpl, SettingsCrostiniPageElement, SettingsGuestOsConfirmationDialogElement} from 'chrome://os-settings/lazy_load.js';
import {Router, routes} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestGuestOsBrowserProxy} from '../guest_os/test_guest_os_browser_proxy.js';
import {clearBody} from '../utils.js';

import {TestCrostiniBrowserProxy} from './test_crostini_browser_proxy.js';

suite('<settings-crostini-page>', () => {
  let crostiniPage: SettingsCrostiniPageElement;
  let guestOsBrowserProxy: TestGuestOsBrowserProxy;
  let crostiniBrowserProxy: TestCrostiniBrowserProxy;

  function setCrostiniPrefs(enabled: boolean, {
    sharedPaths = {},
    forwardedPorts = [],
    micAllowed = false,
    arcEnabled = false,
    bruschettaInstalled = false,
  } = {}): void {
    crostiniPage.prefs = {
      arc: {
        enabled: {value: arcEnabled},
      },
      bruschetta: {
        installed: {
          value: bruschettaInstalled,
        },
      },
      crostini: {
        enabled: {value: enabled},
        mic_allowed: {value: micAllowed},
        port_forwarding: {ports: {value: forwardedPorts}},
      },
      guest_os: {
        paths_shared_to_vms: {value: sharedPaths},
      },
    };
    flush();
  }

  setup(() => {
    loadTimeData.overrideValues({
      isCrostiniAllowed: true,
      isCrostiniSupported: true,
    });
    crostiniBrowserProxy = new TestCrostiniBrowserProxy();
    CrostiniBrowserProxyImpl.setInstanceForTesting(crostiniBrowserProxy);
    guestOsBrowserProxy = new TestGuestOsBrowserProxy();
    GuestOsBrowserProxyImpl.setInstanceForTesting(guestOsBrowserProxy);

    Router.getInstance().navigateTo(routes.CROSTINI);

    clearBody();
    crostiniPage = document.createElement('settings-crostini-page');
    document.body.appendChild(crostiniPage);
    flush();
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
  });

  suite('<settings-guest-os-confirmation-dialog>', () => {
    let dialog: SettingsGuestOsConfirmationDialogElement;
    let cancelOrCloseEvents: CustomEvent[];
    let closeEventPromise: Promise<Event>;

    setup(() => {
      cancelOrCloseEvents = [];
      dialog = document.createElement('settings-guest-os-confirmation-dialog');

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

  // Functionality is already tested in OSSettingsGuestOsSharedPathsTest,
  // so just check that we correctly set up the page for our 'termina' VM.
  test(
      '<settings-guest-os-shared-paths> is correctly set up for termina VM',
      async () => {
        setCrostiniPrefs(
            true,
            {sharedPaths: {path1: ['termina'], path2: ['some-other-vm']}});
        await flushTasks();

        Router.getInstance().navigateTo(routes.CROSTINI_SHARED_PATHS);
        await flushTasks();

        const subpage = crostiniPage.shadowRoot!.querySelector(
            'settings-guest-os-shared-paths');
        assertTrue(!!subpage);
        assertEquals(
            1, subpage.shadowRoot!.querySelectorAll('.list-item').length);
      });
});
