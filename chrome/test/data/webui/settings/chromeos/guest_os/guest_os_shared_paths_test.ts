// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {GuestOsBrowserProxyImpl, SettingsGuestOsSharedPathsElement} from 'chrome://os-settings/lazy_load.js';
import {CrButtonElement, CrDialogElement} from 'chrome://os-settings/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestGuestOsBrowserProxy} from './test_guest_os_browser_proxy.js';

suite('<settings-guest-os-shared-paths>', () => {
  let page: SettingsGuestOsSharedPathsElement;
  let guestOsBrowserProxy: TestGuestOsBrowserProxy;

  async function setPrefs(sharedPaths: {[key: string]: string[]}) {
    guestOsBrowserProxy.resetResolver('getGuestOsSharedPathsDisplayText');
    page.prefs = {
      guest_os: {
        paths_shared_to_vms: {value: sharedPaths},
      },
    };
    await guestOsBrowserProxy.whenCalled('getGuestOsSharedPathsDisplayText');
    flush();
  }

  setup(() => {
    guestOsBrowserProxy = new TestGuestOsBrowserProxy();
    GuestOsBrowserProxyImpl.setInstanceForTesting(guestOsBrowserProxy);
    page = document.createElement('settings-guest-os-shared-paths');
    page.guestOsType = 'pluginVm';
    document.body.appendChild(page);
  });

  teardown(() => {
    page.remove();
  });

  test('Remove', async () => {
    await setPrefs({'path1': ['PvmDefault'], 'path2': ['PvmDefault']});
    assertEquals(3, page.shadowRoot!.querySelectorAll('.settings-box').length);
    const rows = '.list-item:not([hidden])';
    assertEquals(2, page.shadowRoot!.querySelectorAll(rows).length);

    assertFalse(page.$.guestOsInstructionsRemove.hidden);
    assertFalse(page.$.guestOsList.hidden);
    assertTrue(page.$.guestOsListEmpty.hidden);
    const button = page.shadowRoot!.querySelector<CrButtonElement>(
        '.list-item cr-icon-button');
    assertTrue(!!button);

    // Remove first shared path, still one left.
    button.click();
    {
      const [vmName, path] =
          await guestOsBrowserProxy.whenCalled('removeGuestOsSharedPath');
      assertEquals('PvmDefault', vmName);
      assertEquals('path1', path);
    }
    await setPrefs({'path2': ['PvmDefault']});
    assertEquals(1, page.shadowRoot!.querySelectorAll(rows).length);
    assertFalse(page.$.guestOsInstructionsRemove.hidden);

    // Remove remaining shared path, none left.
    guestOsBrowserProxy.resetResolver('removeGuestOsSharedPath');
    const rowButton = page.shadowRoot!.querySelector<CrButtonElement>(
        `${rows} cr-icon-button`);
    assertTrue(!!rowButton);
    rowButton.click();
    {
      const [vmName, path] =
          await guestOsBrowserProxy.whenCalled('removeGuestOsSharedPath');
      assertEquals('PvmDefault', vmName);
      assertEquals('path2', path);
    }
    await setPrefs({'ignored': ['ignore']});
    assertTrue(page.$.guestOsList.hidden);
    // Verify remove instructions are hidden, and empty list message is shown.
    assertTrue(page.$.guestOsInstructionsRemove.hidden);
    assertTrue(page.$.guestOsList.hidden);
    assertFalse(page.$.guestOsListEmpty.hidden);
  });

  test('RemoveFailedRetry', async () => {
    await setPrefs({'path1': ['PvmDefault'], 'path2': ['PvmDefault']});

    // Remove shared path fails.
    guestOsBrowserProxy.stubRemoveSharedPathResult(false);
    const button = page.shadowRoot!.querySelector<CrButtonElement>(
        '.list-item cr-icon-button');
    assertTrue(!!button);
    button.click();

    await guestOsBrowserProxy.whenCalled('removeGuestOsSharedPath');
    flush();
    const dialog = page.shadowRoot!.querySelector<CrDialogElement>(
        '#removeSharedPathFailedDialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);

    // Click retry and make sure 'removeGuestOsSharedPath' is called
    // and dialog is closed/removed.
    guestOsBrowserProxy.stubRemoveSharedPathResult(true);
    const actionButton =
        dialog.querySelector<CrButtonElement>('.action-button');
    assertTrue(!!actionButton);
    actionButton.click();
    await guestOsBrowserProxy.whenCalled('removeGuestOsSharedPath');
    assertEquals(
        null, page.shadowRoot!.querySelector('#removeSharedPathFailedDialog'));
  });
});
