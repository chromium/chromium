// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/lazy_load.js';

// #import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {flush} from'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {waitAfterNextRender} from 'chrome://test/test_util.m.js';
// clang-format on


suite('FilesPageTests', function() {
  /** @type {SettingsFilesPageElement} */
  let filesPage = null;

  setup(function() {
    PolymerTest.clearBody();
    filesPage = document.createElement('os-settings-files-page');
    document.body.appendChild(filesPage);
    Polymer.dom.flush();
  });

  teardown(function() {
    filesPage.remove();
    settings.Router.getInstance().resetRouteForTesting();
  });

  test('Disconnect Google Drive account,pref disabled/enabled', async () => {
    // The default state of the pref is disabled.
    const disconnectGoogleDrive =
        assert(filesPage.$$('#disconnectGoogleDriveAccount'));
    assertFalse(disconnectGoogleDrive.checked);

    disconnectGoogleDrive.$$('cr-toggle').click();
    Polymer.dom.flush();
    assertTrue(disconnectGoogleDrive.checked);
  });

  test('Smb Shares, Navigates to SMB_SHARES route on click', async () => {
    const smbShares = assert(filesPage.$$('#smbShares'));

    smbShares.click();
    Polymer.dom.flush();
    assertEquals(
        settings.Router.getInstance().getCurrentRoute(),
        settings.routes.SMB_SHARES);
  });

  test('Deep link to disconnect Google Drive', async () => {
    loadTimeData.overrideValues({
      isDeepLinkingEnabled: true,
    });

    const params = new URLSearchParams;
    params.append('settingId', '1300');
    settings.Router.getInstance().navigateTo(settings.routes.FILES, params);

    Polymer.dom.flush();

    const deepLinkElement =
        filesPage.$$('#disconnectGoogleDriveAccount').$$('cr-toggle');
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Disconnect Drive toggle should be focused for settingId=1300.');
  });
});