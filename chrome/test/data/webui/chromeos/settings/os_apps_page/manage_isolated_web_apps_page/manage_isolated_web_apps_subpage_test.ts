// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {ManageIsolatedWebAppsSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('Manage Isolated Web Apps', () => {
  let page: ManageIsolatedWebAppsSubpageElement;

  function createPage(): void {
    page = document.createElement('settings-manage-isolated-web-apps-subpage');
    document.body.appendChild(page);
    assertTrue(!!page);
    flush();
  }

  function makeFakePrefs(isolatedWebAppsEnabled = false): {[key: string]: any} {
    return {
      ash: {
        isolated_web_apps_enabled: {
          key: 'ash.isolated_web_apps_enabled',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: isolatedWebAppsEnabled,
        },
      },
    };
  }

  setup(() => {
    loadTimeData.overrideValues({showManageIsolatedWebAppsRow: true});
  });

  teardown(() => {
    page.remove();
  });

  test('Enable Isolated Web Apps toggle is visible', () => {
    createPage();
    const enableIsolatedWebAppsToggle =
        page.shadowRoot!.querySelector('#enableIsolatedWebAppsToggleButton');
    assertTrue(!!enableIsolatedWebAppsToggle);
  });

  test(
      'Clicking the Enable isolated web apps button toggles the pref value',
      () => {
        createPage();
        page.prefs = makeFakePrefs(true);

        const enableIsolatedWebAppsToggle =
            page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#enableIsolatedWebAppsToggleButton');
        assertTrue(!!enableIsolatedWebAppsToggle);
        assertTrue(enableIsolatedWebAppsToggle.checked);
        assertTrue(page.prefs['ash'].isolated_web_apps_enabled.value);

        enableIsolatedWebAppsToggle.click();
        assertFalse(enableIsolatedWebAppsToggle.checked);
        assertFalse(page.prefs['ash'].isolated_web_apps_enabled.value);

        enableIsolatedWebAppsToggle.click();
        assertTrue(enableIsolatedWebAppsToggle.checked);
        assertTrue(page.prefs['ash'].isolated_web_apps_enabled.value);
      });
});
