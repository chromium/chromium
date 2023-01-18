// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('FilesPageTests', function() {
  /** @type {SettingsFilesPageElement} */
  let filesPage = null;

  setup(function() {
    loadTimeData.overrideValues({
      showOfficeSettings: false,
    });
    PolymerTest.clearBody();
    filesPage = document.createElement('os-settings-files-page');
    document.body.appendChild(filesPage);
    flush();
  });

  teardown(function() {
    filesPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Disconnect Google Drive account,pref disabled/enabled', async () => {
    // The default state of the pref is disabled.
    const disconnectGoogleDrive = assert(
        filesPage.shadowRoot.querySelector('#disconnectGoogleDriveAccount'));
    assertFalse(disconnectGoogleDrive.checked);

    disconnectGoogleDrive.shadowRoot.querySelector('cr-toggle').click();
    flush();
    assertTrue(disconnectGoogleDrive.checked);
  });

  test('Smb Shares, Navigates to SMB_SHARES route on click', async () => {
    const smbShares = assert(filesPage.shadowRoot.querySelector('#smbShares'));

    smbShares.click();
    flush();
    assertEquals(Router.getInstance().getCurrentRoute(), routes.SMB_SHARES);
  });

  test('Office row is hidden when showOfficeSettings is false', async () => {
    assertEquals(null, filesPage.shadowRoot.querySelector('#office'));
  });

  test('Deep link to disconnect Google Drive', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1300');
    Router.getInstance().navigateTo(routes.FILES, params);

    flush();

    const deepLinkElement =
        filesPage.shadowRoot.querySelector('#disconnectGoogleDriveAccount')
            .shadowRoot.querySelector('cr-toggle');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Disconnect Drive toggle should be focused for settingId=1300.');
  });

  suite('with showOfficeSettings enabled', () => {
    setup(function() {
      loadTimeData.overrideValues({
        showOfficeSettings: true,
      });
      PolymerTest.clearBody();
      filesPage = document.createElement('os-settings-files-page');
      document.body.appendChild(filesPage);
      flush();
    });

    teardown(function() {
      filesPage.remove();
      Router.getInstance().resetRouteForTesting();
    });

    test('Navigates to OFFICE route on click', async () => {
      const officeRow = assert(filesPage.shadowRoot.querySelector('#office'));

      officeRow.click();
      flush();
      assertEquals(Router.getInstance().getCurrentRoute(), routes.OFFICE);
    });
  });
});
