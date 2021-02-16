// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/lazy_load.js';
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {CrSettingsPrefs, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {flushTasks} from 'chrome://test/test_util.m.js';
// clang-format on

suite('OsSettingsPageTests', function() {
  /** @type {?OsSettingsPageElement} */
  let settingsPage = null;

  suiteSetup(async function() {
    settings.Router.getInstance().navigateTo(settings.routes.BASIC);
    PolymerTest.clearBody();

    const prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    return CrSettingsPrefs.initialized.then(function() {
      settingsPage = document.createElement('os-settings-page');
      settingsPage.prefs = prefElement.prefs;
      document.body.appendChild(settingsPage);
      Polymer.dom.flush();
    });
  });

  teardown(function() {
    settingsPage.remove();
    CrSettingsPrefs.resetForTesting();
    settings.Router.getInstance().resetRouteForTesting();
  });

  test('Os Settings Page created', async () => {
    assert(!!settingsPage);
  });

  test('Check settings-internet-page exists', async () => {
    const settingsInternetPage = settingsPage.$$('settings-internet-page');
    assert(!!settingsInternetPage);
  });

  test('Check os-settings-printing-page exists', async () => {
    const idleRender = settingsPage.$$('settings-idle-load');
    await idleRender.get();
    Polymer.dom.flush();
    const osSettingsPrintingPage = settingsPage.$$('os-settings-printing-page');
    assert(!!osSettingsPrintingPage);
  });

  test('Check settings-bluetooth-page exists', async () => {
    const settingsBluetoothPage = settingsPage.$$('settings-bluetooth-page');
    assert(!!settingsBluetoothPage);
  });

  test('Check os-settings-privacy-page exists', async () => {
    settingsPage.isAccountManagementFlowsV2Enabled_ = false;
    const osSettingsPrivacyPage = settingsPage.$$('os-settings-privacy-page');
    assert(!!osSettingsPrivacyPage);
    Polymer.dom.flush();
  });

  test('Check settings-multidevice-page exists', async () => {
    const settingsMultidevicePage =
        settingsPage.$$('settings-multidevice-page');
    assert(!!settingsMultidevicePage);
  });

  test('Check os-settings-people-page exists', async () => {
    const settingsPeoplePage = settingsPage.$$('os-settings-people-page');
    assert(!!settingsPeoplePage);
  });

  test('Check settings-date-time-page exists', async () => {
    const idleRender = settingsPage.$$('settings-idle-load');
    await idleRender.get();
    Polymer.dom.flush();
    const settingsDateTimePage = settingsPage.$$('settings-date-time-page');
    assert(!!settingsDateTimePage);
  });

  test('Check os-settings-languages-section exists', async () => {
    const osSettingsLangagesSection =
        settingsPage.$$('os-settings-languages-section');
    assert(!!osSettingsLangagesSection);
  });

  test('Check os-settings-a11y-page exists', async () => {
    const osSettingsA11yPage = settingsPage.$$('os-settings-a11y-page');
    assert(!!osSettingsA11yPage);
  });

  test('Check settings-kerberos-page exists', async () => {
    settingsPage.showKerberosSection = true;
    const idleRender = settingsPage.$$('settings-idle-load');
    assert(!!idleRender);
    await idleRender.get();
    Polymer.dom.flush();

    const settingsKerberosPage = settingsPage.$$('settings-kerberos-page');
    assert(!!settingsKerberosPage);
    Polymer.dom.flush();
  });

  test('Check settings-device-page exists', async () => {
    settingsPage.showCrostini = true;
    settingsPage.allowCrostini_ = true;
    const settingsDevicePage = settingsPage.$$('settings-device-page');
    assert(!!settingsDevicePage);
    Polymer.dom.flush();
  });

  test('Check os-settings-files-page exists', async () => {
    settingsPage.isGuestMode_ = false;
    const idleRender = settingsPage.$$('settings-idle-load');
    await idleRender.get();
    Polymer.dom.flush();
    const settingsFilesPage = settingsPage.$$('os-settings-files-page');
    assert(!!settingsFilesPage);
  });

  test('Check settings-personalization-page exists', async () => {
    const settingsPersonalizationPage =
        settingsPage.$$('settings-personalization-page');
    assert(!!settingsPersonalizationPage);
  });

  test('Check os-settings-search-page exists', async () => {
    const osSettingsSearchPage = settingsPage.$$('os-settings-search-page');
    assert(!!osSettingsSearchPage);
  });

  test('Check os-settings-apps-page exists', async () => {
    settingsPage.showAndroidApps = true;
    settingsPage.showPluginVm = true;
    settingsPage.havePlayStoreApp = true;
    Polymer.dom.flush();
    const osSettingsAppsPage = settingsPage.$$('os-settings-apps-page');
    assert(!!osSettingsAppsPage);
  });

  test('Check settings-on-startup-page exists', async () => {
    settingsPage.showStartup = true;
    Polymer.dom.flush();
    const settingsOnStartupPage = settingsPage.$$('settings-on-startup-page');
    assert(!!settingsOnStartupPage);
  });

  test('Check settings-crostini-page exists', async () => {
    settingsPage.showCrostini = true;
    const idleRender = settingsPage.$$('settings-idle-load');
    await idleRender.get();
    Polymer.dom.flush();
    const osSettingsCrostiniPage = settingsPage.$$('settings-crostini-page');
    assert(!!osSettingsCrostiniPage);
  });

  test('Check os-settings-reset-page exists', async () => {
    settingsPage.showReset = true;
    Polymer.dom.flush();
    const osSettingsResetPage = settingsPage.$$('os-settings-reset-page');
    assert(!!osSettingsResetPage);
  });
});
