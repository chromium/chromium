// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {AccountManagerBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {CrSettingsPrefs, OsSettingsUiElement, Router, routes, setNearbyShareSettingsForTesting} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/chromeos/nearby_share/shared/fake_nearby_share_settings.js';

import {TestAccountManagerBrowserProxy} from '../os_people_page/test_account_manager_browser_proxy.js';

suite('<os-settings-ui> toolbar', () => {
  const isRevampWayfindingEnabled =
      loadTimeData.getBoolean('isRevampWayfindingEnabled');
  let ui: OsSettingsUiElement;
  let fakeNearbySettings: FakeNearbyShareSettings;
  let testAccountManagerBrowserProxy: TestAccountManagerBrowserProxy;

  suiteSetup(() => {
    fakeNearbySettings = new FakeNearbyShareSettings();
    setNearbyShareSettingsForTesting(fakeNearbySettings);

    // Setup fake accounts. There must be a device account available for the
    // Accounts menu item in <os-settings-menu>.
    testAccountManagerBrowserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(
        testAccountManagerBrowserProxy);
  });

  async function createElement(): Promise<OsSettingsUiElement> {
    const element = document.createElement('os-settings-ui');
    document.body.appendChild(element);
    await CrSettingsPrefs.initialized;
    flush();
    return element;
  }

  setup(() => {
    Router.getInstance().navigateTo(routes.BASIC);
  });

  teardown(() => {
    ui.remove();
    testAccountManagerBrowserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  if (!isRevampWayfindingEnabled) {
    test('Toolbar shadow is always shown for subpages', async () => {
      ui = await createElement();
      const shadowEl = ui.shadowRoot!.querySelector('#cr-container-shadow-top');
      assertTrue(!!shadowEl, 'Shadow container element should exist');

      assertFalse(
          shadowEl.classList.contains('has-shadow'),
          'Main page should not show shadow ' + shadowEl.className);

      Router.getInstance().navigateTo(routes.POWER);
      flush();
      assertTrue(
          shadowEl.classList.contains('has-shadow'),
          'Sub-page should show shadow ' + shadowEl.className);
    });
  }

  test('Menu icon shows only in narrow mode', async () => {
    ui = await createElement();
    flush();

    const toolbar = ui.shadowRoot!.querySelector('settings-toolbar');
    assertTrue(!!toolbar, 'Toolbar should exist');

    ui.isNarrow = true;
    assertTrue(toolbar.showMenu);

    ui.isNarrow = false;
    assertFalse(toolbar.showMenu);
  });

  suite('When in kiosk mode', () => {
    setup(() => {
      loadTimeData.overrideValues({
        isKioskModeActive: true,
      });
    });

    test('Toolbar is hidden in kiosk mode', async () => {
      ui = await createElement();
      const toolbar = ui.shadowRoot!.querySelector('settings-toolbar');
      assertNull(toolbar);
    });
  });
});
