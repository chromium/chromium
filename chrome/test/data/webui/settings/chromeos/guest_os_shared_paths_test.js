// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/os_settings.js';

import {GuestOsBrowserProxyImpl} from 'chrome://os-settings/chromeos/lazy_load.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/** @implements {GuestOsBrowserProxy} */
class TestGuestOsBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getGuestOsSharedPathsDisplayText',
      'removeGuestOsSharedPath',
    ]);
    this.removeSharedPathResult = true;
  }

  /** override */
  getGuestOsSharedPathsDisplayText(paths) {
    this.methodCalled('getGuestOsSharedPathsDisplayText');
    return Promise.resolve(paths.map(path => path + '-displayText'));
  }

  /** override */
  removeGuestOsSharedPath(vmName, path) {
    this.methodCalled('removeGuestOsSharedPath', [vmName, path]);
    return Promise.resolve(this.removeSharedPathResult);
  }
}

suite('SharedPaths', function() {
  /** @type {?SettingsGuestOsSharedPathsElement} */
  let page = null;

  /** @type {?TestGuestOsBrowserProxy} */
  let guestOsBrowserProxy = null;

  function setPrefs(sharedPaths) {
    guestOsBrowserProxy.resetResolver('getGuestOsSharedPathsDisplayText');
    page.prefs = {
      guest_os: {
        paths_shared_to_vms: {value: sharedPaths},
      },
    };
    return guestOsBrowserProxy.whenCalled('getGuestOsSharedPathsDisplayText')
        .then(() => {
          flush();
        });
  }

  setup(function() {
    guestOsBrowserProxy = new TestGuestOsBrowserProxy();
    GuestOsBrowserProxyImpl.setInstanceForTesting(guestOsBrowserProxy);
    PolymerTest.clearBody();
    page = document.createElement('settings-guest-os-shared-paths');
    page.guestOsType = 'pluginVm';
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
  });

  test('Remove', async function() {
    await setPrefs({'path1': ['PvmDefault'], 'path2': ['PvmDefault']});
    assertEquals(3, page.shadowRoot.querySelectorAll('.settings-box').length);
    const rows = '.list-item:not([hidden])';
    assertEquals(2, page.shadowRoot.querySelectorAll(rows).length);

    assertFalse(page.$.guestOsInstructionsRemove.hidden);
    assertFalse(page.$.guestOsList.hidden);
    assertTrue(page.$.guestOsListEmpty.hidden);
    assertTrue(!!page.shadowRoot.querySelector('.list-item cr-icon-button'));

    // Remove first shared path, still one left.
    page.shadowRoot.querySelector('.list-item cr-icon-button').click();
    {
      const [vmName, path] =
          await guestOsBrowserProxy.whenCalled('removeGuestOsSharedPath');
      assertEquals('PvmDefault', vmName);
      assertEquals('path1', path);
    }
    await setPrefs({'path2': ['PvmDefault']});
    assertEquals(1, page.shadowRoot.querySelectorAll(rows).length);
    assertFalse(page.$.guestOsInstructionsRemove.hidden);

    // Remove remaining shared path, none left.
    guestOsBrowserProxy.resetResolver('removeGuestOsSharedPath');
    page.shadowRoot.querySelector(`${rows} cr-icon-button`).click();
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

  test('RemoveFailedRetry', async function() {
    await setPrefs({'path1': ['PvmDefault'], 'path2': ['PvmDefault']});

    // Remove shared path fails.
    guestOsBrowserProxy.removeSharedPathResult = false;
    page.shadowRoot.querySelector('.list-item cr-icon-button').click();

    await guestOsBrowserProxy.whenCalled('removeGuestOsSharedPath');
    flush();
    assertTrue(
        page.shadowRoot.querySelector('#removeSharedPathFailedDialog').open);

    // Click retry and make sure 'removeGuestOsSharedPath' is called
    // and dialog is closed/removed.
    guestOsBrowserProxy.removeSharedPathResult = true;
    page.shadowRoot.querySelector('#removeSharedPathFailedDialog')
        .querySelector('.action-button')
        .click();
    await guestOsBrowserProxy.whenCalled('removeGuestOsSharedPath');
    assertFalse(
        !!page.shadowRoot.querySelector('#removeSharedPathFailedDialog'));
  });
});
