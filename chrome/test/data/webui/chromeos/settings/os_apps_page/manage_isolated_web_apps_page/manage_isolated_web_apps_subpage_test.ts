// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import type {ManageIsolatedWebAppsSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {CrSettingsPrefs, type SettingsPrefsElement} from 'chrome://os-settings/os_settings.js';
import type {SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('Manage Isolated Web Apps', () => {
  let page: ManageIsolatedWebAppsSubpageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(async () => {
    settingsPrefs = document.createElement('settings-prefs');
    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;
  });

  function createPage(): void {
    page = document.createElement('settings-manage-isolated-web-apps-subpage');
    document.body.appendChild(page);
    assertTrue(!!page, 'Page should be created');
    flush();
  }

  function getDefaultPrefs(): {[key: string]: any} {
    return {
      ash: {
        isolated_web_apps_enabled: {
          key: 'ash.isolated_web_apps_enabled',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
      },
      profile: {
        isolated_web_app: {
          install: {
            user_install_enabled: {
              key: 'profile.isolated_web_app.install.user_install_enabled',
              type: chrome.settingsPrivate.PrefType.BOOLEAN,
              value: true,
            },
          },
        },
      },
    };
  }

  function setProfilePref(value: boolean, enforced: boolean): void {
    const prefUpdate = {
      key: 'profile.isolated_web_app.install.user_install_enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: value,
      enforcement: enforced ? chrome.settingsPrivate.Enforcement.ENFORCED :
                              undefined,
      controlledBy: enforced ?
          chrome.settingsPrivate.ControlledBy.DEVICE_POLICY :
          undefined,
    };
    page.set(
        'prefs.profile.isolated_web_app.install.user_install_enabled',
        prefUpdate);
    flush();
  }

  function setUserPref(value: boolean): void {
    page.set('prefs.ash.isolated_web_apps_enabled.value', value);
    flush();
  }

  setup(async () => {
    loadTimeData.overrideValues({showManageIsolatedWebAppsRow: true});
    settingsPrefs.prefs = getDefaultPrefs();

    createPage();
    page.prefs = settingsPrefs.prefs!;
    await waitBeforeNextRender(page);
  });

  teardown(() => {
    page.remove();
  });

  test('Enable Isolated Web Apps toggle is visible', () => {
    const enableIsolatedWebAppsToggle =
        page.shadowRoot!.querySelector('#enableIsolatedWebAppsToggleButton');
    assertTrue(!!enableIsolatedWebAppsToggle);
  });

  test('Clicking the toggle updates the User pref value', async () => {
    setUserPref(true);
    await waitBeforeNextRender(page);

    const enableIsolatedWebAppsToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#enableIsolatedWebAppsToggleButton');
    assertTrue(!!enableIsolatedWebAppsToggle);
    assertTrue(enableIsolatedWebAppsToggle.checked);
    assertTrue(page.prefs['ash'].isolated_web_apps_enabled.value);

    enableIsolatedWebAppsToggle.click();
    await waitBeforeNextRender(page);
    assertFalse(enableIsolatedWebAppsToggle.checked);
    assertFalse(page.prefs['ash'].isolated_web_apps_enabled.value);

    enableIsolatedWebAppsToggle.click();
    await waitBeforeNextRender(page);
    assertTrue(enableIsolatedWebAppsToggle.checked);
    assertTrue(page.prefs['ash'].isolated_web_apps_enabled.value);
  });

  test(
      'Toggle is disabled when IWA user install admin policy set to false',
      async () => {
        setUserPref(true);
        assertTrue(page.prefs['ash'].isolated_web_apps_enabled.value);

        setProfilePref(/*value=*/ false, /*enforced=*/ true);
        await waitBeforeNextRender(page);

        const enableIsolatedWebAppsToggle =
            page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#enableIsolatedWebAppsToggleButton');
        assertTrue(!!enableIsolatedWebAppsToggle);

        assertTrue(enableIsolatedWebAppsToggle.disabled);
        assertFalse(enableIsolatedWebAppsToggle.checked);
        assertTrue(!!enableIsolatedWebAppsToggle.shadowRoot!.querySelector(
            'cr-policy-pref-indicator'));
      });

  test(
      'Toggle is enabled and toggable when admin policy enables user installs',
      async () => {
        setUserPref(false);
        setProfilePref(/*value=*/ true, /*enforced=*/ true);
        await waitBeforeNextRender(page);

        const enableIsolatedWebAppsToggle =
            page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#enableIsolatedWebAppsToggleButton');
        assertTrue(!!enableIsolatedWebAppsToggle);
        assertFalse(enableIsolatedWebAppsToggle.disabled);
        assertFalse(enableIsolatedWebAppsToggle.checked);

        const icon = enableIsolatedWebAppsToggle.shadowRoot!.querySelector(
            'cr-policy-pref-indicator');
        assertFalse(isVisible(icon));

        enableIsolatedWebAppsToggle.click();
        await waitBeforeNextRender(page);
        assertTrue(enableIsolatedWebAppsToggle.checked);
        assertTrue(page.prefs['ash'].isolated_web_apps_enabled.value);
      });
});
