// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {OsSettingsFilesPageElement, SmbBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {CrSettingsPrefs, Router, routes, SettingsPrefsElement} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestSmbBrowserProxy} from './test_smb_browser_proxy.js';

suite('<os-settings-files-page>', () => {
  let filesPage: OsSettingsFilesPageElement;
  let prefElement: SettingsPrefsElement;
  let smbBrowserProxy: TestSmbBrowserProxy;

  /**
   * Returns a list of fake preferences that are used at some point in any test,
   * if another element is added that requires a new pref ensure to add it here.
   */
  function getFakePrefs() {
    return [
      {
        key: 'gdata.disabled',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      },
      {
        key: 'gdata.cellular.disabled',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      },
      {
        key: 'drivefs.bulk_pinning_enabled',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      },
      {
        key: 'drivefs.enable_mirror_sync',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      },
      // The OneDrive preferences that are required when navigating to the
      // officeFiles page route.
      {
        key: 'filebrowser.office.always_move_to_onedrive',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      },
      {
        key: 'filebrowser.office.always_move_to_drive',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      },
      {
        key: 'network_file_shares.allowed.value',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
    ];
  }

  async function createFilesPage() {
    prefElement = document.createElement('settings-prefs');
    const fakeSettingsPrivate = new FakeSettingsPrivate(getFakePrefs());
    prefElement.initialize(fakeSettingsPrivate);
    await CrSettingsPrefs.initialized;

    filesPage = document.createElement('os-settings-files-page');
    filesPage.prefs = prefElement.prefs;
    document.body.appendChild(filesPage);
    await flushTasks();
  }

  suiteSetup(() => {
    smbBrowserProxy = new TestSmbBrowserProxy();
    SmbBrowserProxyImpl.setInstance(smbBrowserProxy);

    CrSettingsPrefs.deferInitialization = true;
  });

  setup(() => {
    loadTimeData.overrideValues({
      showOfficeSettings: false,
      enableDriveFsBulkPinning: false,
      showGoogleDriveSettingsPage: false,
    });
  });

  teardown(() => {
    filesPage.remove();
    prefElement.remove();
    smbBrowserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  test('Files settings card is visible', async () => {
    Router.getInstance().navigateTo(routes.FILES);
    await createFilesPage();

    const filesSettingsCard =
        filesPage.shadowRoot!.querySelector('files-settings-card');
    assertTrue(isVisible(filesSettingsCard));
  });
});
