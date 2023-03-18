// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {CrSettingsPrefs, Router, routes, setContactManagerForTesting, setNearbyShareSettingsForTesting} from 'chrome://os-settings/chromeos/os_settings.js';
import {setBluetoothConfigForTesting} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeBluetoothConfig} from 'chrome://webui-test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';
import {FakeContactManager} from 'chrome://webui-test/nearby_share/shared/fake_nearby_contact_manager.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/nearby_share/shared/fake_nearby_share_settings.js';

suite('<os-settings-page>', function() {
  /** @type {?OsSettingsPageElement} */
  let settingsPage = null;

  /** @type {?SettingsPrefsElement} */
  let prefElement = null;

  /** @type {!FakeContactManager} */
  let fakeContactManager = null;
  /** @type {!FakeNearbyShareSettings} */
  let fakeSettings = null;

  suiteSetup(async function() {
    fakeContactManager = new FakeContactManager();
    setContactManagerForTesting(fakeContactManager);
    fakeContactManager.setupContactRecords();

    fakeSettings = new FakeNearbyShareSettings();
    setNearbyShareSettingsForTesting(fakeSettings);

    // Using the real CrosBluetoothConfig will crash due to no
    // SessionManager.
    setBluetoothConfigForTesting(new FakeBluetoothConfig());

    PolymerTest.clearBody();
    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);
    await CrSettingsPrefs.initialized;
  });

  /** @return {OsSettingsPageElement} */
  function init() {
    const element = document.createElement('os-settings-page');
    element.prefs = prefElement.prefs;
    document.body.appendChild(element);
    flush();
    return element;
  }

  suite('Basic pages', () => {
    suiteSetup(() => {
      Router.getInstance().navigateTo(routes.BASIC);
      settingsPage = init();

      // For Kerberos page
      settingsPage.showKerberosSection = true;
    });

    suiteTeardown(() => {
      settingsPage.remove();
      CrSettingsPrefs.resetForTesting();
      Router.getInstance().resetRouteForTesting();
    });

    const basicPages = [
      'settings-internet-page',
      'os-settings-bluetooth-page',
      'settings-multidevice-page',
      'settings-kerberos-page',
      'os-settings-people-page',
      'settings-device-page',
      'settings-personalization-page',
      'os-settings-search-page',
      'os-settings-privacy-page',
      'os-settings-apps-page',
      'os-settings-a11y-page',
    ];
    basicPages.forEach((pageName) => {
      test(`${pageName} exists`, () => {
        const pageElement = settingsPage.shadowRoot.querySelector(pageName);
        assertTrue(!!pageElement, `Element <${pageName}> was not found.`);
      });
    });
  });

  suite('Advanced pages', () => {
    suiteSetup(async () => {
      Router.getInstance().navigateTo(routes.BASIC);
      settingsPage = init();
      const idleRender =
          settingsPage.shadowRoot.querySelector('settings-idle-load');
      await idleRender.get();

      // For reset page
      settingsPage.showReset = true;
    });

    suiteTeardown(() => {
      settingsPage.remove();
      CrSettingsPrefs.resetForTesting();
      Router.getInstance().resetRouteForTesting();
    });

    const advancedPages = [
      'settings-date-time-page',
      'os-settings-languages-section',
      'os-settings-files-page',
      'os-settings-printing-page',
      'settings-crostini-page',
      'os-settings-reset-page',
    ];
    advancedPages.forEach((pageName) => {
      test(`${pageName} exists`, async () => {
        const pageElement = settingsPage.shadowRoot.querySelector(pageName);
        assertTrue(!!pageElement, `Element <${pageName}> was not found.`);
      });
    });
  });

  suite('In Guest mode', () => {
    suiteSetup(async () => {
      Router.getInstance().navigateTo(routes.BASIC);
      loadTimeData.overrideValues({isGuest: true});
      settingsPage = init();

      // For Kerberos page
      settingsPage.showKerberosSection = true;
      // For reset page
      settingsPage.showReset = true;

      const idleRender =
          settingsPage.shadowRoot.querySelector('settings-idle-load');
      await idleRender.get();
    });

    suiteTeardown(() => {
      settingsPage.remove();
      CrSettingsPrefs.resetForTesting();
      Router.getInstance().resetRouteForTesting();
    });

    const visiblePages = [
      'settings-internet-page',
      'os-settings-bluetooth-page',
      'settings-kerberos-page',
      'settings-device-page',
      'os-settings-search-page',
      'os-settings-privacy-page',
      'os-settings-apps-page',
      'os-settings-a11y-page',
      'settings-date-time-page',
      'os-settings-languages-section',
      'os-settings-printing-page',
      'settings-crostini-page',
      'os-settings-reset-page',
    ];
    visiblePages.forEach((pageName) => {
      test(`${pageName} should exist`, async () => {
        const pageElement = settingsPage.shadowRoot.querySelector(pageName);
        assertTrue(
            !!pageElement, `Element <${pageName}> should exist in Guest mode.`);
      });
    });

    const hiddenPages = [
      'settings-multidevice-page',
      'os-settings-people-page',
      'settings-personalization-page',
      'os-settings-files-page',
    ];
    hiddenPages.forEach((pageName) => {
      test(`${pageName} should not exist`, async () => {
        const pageElement = settingsPage.shadowRoot.querySelector(pageName);
        assertEquals(
            null, pageElement,
            `Element <${pageName}> should not exist in Guest mode.`);
      });
    });
  });
});
