// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {CrSettingsPrefs, OsSettingsUiElement, Router, routes, setNearbyShareSettingsForTesting} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/nearby_share/shared/fake_nearby_share_settings.js';

suite('<os-settings-ui> toolbar', () => {
  let ui: OsSettingsUiElement;
  let fakeNearbySettings: FakeNearbyShareSettings;

  suiteSetup(() => {
    fakeNearbySettings = new FakeNearbyShareSettings();
    setNearbyShareSettingsForTesting(fakeNearbySettings);
  });

  async function createElement(): Promise<OsSettingsUiElement> {
    const element = document.createElement('os-settings-ui');
    document.body.appendChild(element);
    await CrSettingsPrefs.initialized;
    flush();
    return element;
  }

  teardown(() => {
    ui.remove();
    Router.getInstance().resetRouteForTesting();
  });

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

  test('Menu icon shows only in narrow mode', async () => {
    ui = await createElement();
    flush();

    const toolbar = ui.shadowRoot!.querySelector('os-toolbar');
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
      const toolbar = ui.shadowRoot!.querySelector('os-toolbar');
      assertNull(toolbar);
    });
  });
});
