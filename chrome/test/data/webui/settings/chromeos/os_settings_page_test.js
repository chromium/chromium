// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {CrSettingsPrefs, Router, routes, setContactManagerForTesting, setNearbyShareSettingsForTesting} from 'chrome://os-settings/chromeos/os_settings.js';
import {setBluetoothConfigForTesting} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FakeBluetoothConfig} from 'chrome://webui-test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';

import {FakeContactManager} from 'chrome://webui-test/nearby_share/shared/fake_nearby_contact_manager.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/nearby_share/shared/fake_nearby_share_settings.js';

suite('OsSettingsPageTests', function() {
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

    Router.getInstance().navigateTo(routes.BASIC);
    PolymerTest.clearBody();

    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    return CrSettingsPrefs.initialized;
  });

  teardown(function() {
    settingsPage.remove();
    CrSettingsPrefs.resetForTesting();
    Router.getInstance().resetRouteForTesting();
  });

  function init() {
    // Using the real CrosBluetoothConfig will crash due to no
    // SessionManager.
    setBluetoothConfigForTesting(new FakeBluetoothConfig());

    settingsPage = document.createElement('os-settings-page');
    settingsPage.prefs = prefElement.prefs;
    document.body.appendChild(settingsPage);
    flush();
  }

  test('Os Settings Page created', async () => {
    init();
    assert(!!settingsPage);
  });

  test('Check settings-internet-page exists', async () => {
    init();
    const settingsInternetPage =
        settingsPage.shadowRoot.querySelector('settings-internet-page');
    assert(!!settingsInternetPage);
  });

  test('Check os-settings-printing-page exists', async () => {
    init();
    const idleRender =
        settingsPage.shadowRoot.querySelector('settings-idle-load');
    await idleRender.get();
    flush();
    const osSettingsPrintingPage =
        settingsPage.shadowRoot.querySelector('os-settings-printing-page');
    assert(!!osSettingsPrintingPage);
  });

  test('Check os-settings-bluetooth-page exists', async () => {
    init();
    const osSettingsBluetoothPage =
        settingsPage.shadowRoot.querySelector('os-settings-bluetooth-page');
    assert(!!osSettingsBluetoothPage);
  });

  test('Check os-settings-privacy-page exists', async () => {
    init();
    const osSettingsPrivacyPage =
        settingsPage.shadowRoot.querySelector('os-settings-privacy-page');
    assert(!!osSettingsPrivacyPage);
    flush();
  });

  test('Check settings-multidevice-page exists', async () => {
    init();
    const settingsMultidevicePage =
        settingsPage.shadowRoot.querySelector('settings-multidevice-page');
    assert(!!settingsMultidevicePage);
  });

  test('Check os-settings-people-page exists', async () => {
    init();
    const settingsPeoplePage =
        settingsPage.shadowRoot.querySelector('os-settings-people-page');
    assert(!!settingsPeoplePage);
  });

  test('Check settings-date-time-page exists', async () => {
    init();
    const idleRender =
        settingsPage.shadowRoot.querySelector('settings-idle-load');
    await idleRender.get();
    flush();
    const settingsDateTimePage =
        settingsPage.shadowRoot.querySelector('settings-date-time-page');
    assert(!!settingsDateTimePage);
  });

  test('Check os-settings-languages-section exists', async () => {
    init();
    const idleRender =
        settingsPage.shadowRoot.querySelector('settings-idle-load');
    assert(!!idleRender);
    await idleRender.get();
    flush();
    const osSettingsLangagesSection =
        settingsPage.shadowRoot.querySelector('os-settings-languages-section');
    assert(!!osSettingsLangagesSection);
  });

  test('Check os-settings-a11y-page exists', async () => {
    init();
    const idleRender =
        settingsPage.shadowRoot.querySelector('settings-idle-load');
    assert(!!idleRender);
    await idleRender.get();
    flush();
    const osSettingsA11yPage =
        settingsPage.shadowRoot.querySelector('os-settings-a11y-page');
    assert(!!osSettingsA11yPage);
  });

  test('Check settings-kerberos-page exists', async () => {
    init();
    settingsPage.showKerberosSection = true;
    const idleRender =
        settingsPage.shadowRoot.querySelector('settings-idle-load');
    assert(!!idleRender);
    await idleRender.get();
    flush();

    const settingsKerberosPage =
        settingsPage.shadowRoot.querySelector('settings-kerberos-page');
    assert(!!settingsKerberosPage);
    flush();
  });

  test('Check settings-device-page exists', async () => {
    init();
    settingsPage.showCrostini = true;
    settingsPage.allowCrostini_ = true;
    const settingsDevicePage =
        settingsPage.shadowRoot.querySelector('settings-device-page');
    assert(!!settingsDevicePage);
    flush();
  });

  test('Check os-settings-files-page exists', async () => {
    init();
    settingsPage.isGuestMode_ = false;
    const idleRender =
        settingsPage.shadowRoot.querySelector('settings-idle-load');
    await idleRender.get();
    flush();
    const settingsFilesPage =
        settingsPage.shadowRoot.querySelector('os-settings-files-page');
    assert(!!settingsFilesPage);
  });

  test('Check settings-personalization-page exists', async () => {
    init();
    const settingsPersonalizationPage =
        settingsPage.shadowRoot.querySelector('settings-personalization-page');
    assert(!!settingsPersonalizationPage);
  });

  test('Check os-settings-search-page exists', async () => {
    init();
    const osSettingsSearchPage =
        settingsPage.shadowRoot.querySelector('os-settings-search-page');
    assert(!!osSettingsSearchPage);
  });

  test('Check os-settings-apps-page exists', async () => {
    init();
    settingsPage.showAndroidApps = true;
    settingsPage.showPluginVm = true;
    settingsPage.havePlayStoreApp = true;
    flush();
    const osSettingsAppsPage =
        settingsPage.shadowRoot.querySelector('os-settings-apps-page');
    assert(!!osSettingsAppsPage);
  });

  test('Check settings-crostini-page exists', async () => {
    init();
    const idleRender =
        settingsPage.shadowRoot.querySelector('settings-idle-load');
    await idleRender.get();
    flush();
    const osSettingsCrostiniPage =
        settingsPage.shadowRoot.querySelector('settings-crostini-page');
    assert(!!osSettingsCrostiniPage);
  });

  test('Check os-settings-reset-page exists', async () => {
    init();
    const idleRender =
        settingsPage.shadowRoot.querySelector('settings-idle-load');
    assert(!!idleRender);
    await idleRender.get();

    settingsPage.showReset = true;
    flush();
    const osSettingsResetPage =
        settingsPage.shadowRoot.querySelector('os-settings-reset-page');
    assert(!!osSettingsResetPage);
  });
});
