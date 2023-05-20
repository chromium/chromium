// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {OsSettingsFilesPageElement} from 'chrome://os-settings/lazy_load.js';
import {Router, routes, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util_ts.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

suite('<os-settings-files-page>', () => {
  let filesPage: OsSettingsFilesPageElement;

  setup(() => {
    loadTimeData.overrideValues({
      showOfficeSettings: false,
    });
    filesPage = document.createElement('os-settings-files-page');
    document.body.appendChild(filesPage);
    flush();
  });

  teardown(() => {
    filesPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Disconnect Google Drive account,pref disabled/enabled', async () => {
    // The default state of the pref is disabled.
    const disconnectGoogleDrive =
        filesPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#disconnectGoogleDriveAccount');
    assert(disconnectGoogleDrive);
    assertFalse(disconnectGoogleDrive.checked);

    disconnectGoogleDrive.shadowRoot!.querySelector('cr-toggle')!.click();
    flush();
    assertTrue(disconnectGoogleDrive.checked);
  });

  test('Smb Shares, Navigates to SMB_SHARES route on click', async () => {
    const smbShares =
        filesPage.shadowRoot!.querySelector<HTMLElement>('#smbShares');
    assert(smbShares);

    smbShares.click();
    flush();
    assertEquals(Router.getInstance().currentRoute, routes.SMB_SHARES);
  });

  test('Office row is hidden when showOfficeSettings is false', async () => {
    assertEquals(null, filesPage.shadowRoot!.querySelector('#office'));
  });

  test('Deep link to disconnect Google Drive', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1300');
    Router.getInstance().navigateTo(routes.FILES, params);

    flush();

    const deepLinkElement =
        filesPage.shadowRoot!.querySelector('#disconnectGoogleDriveAccount')!
            .shadowRoot!.querySelector('cr-toggle');
    assert(deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Disconnect Drive toggle should be focused for settingId=1300.');
  });

  suite('with showOfficeSettings enabled', () => {
    setup(() => {
      loadTimeData.overrideValues({
        showOfficeSettings: true,
      });
      filesPage = document.createElement('os-settings-files-page');
      document.body.appendChild(filesPage);
      flush();
    });

    teardown(() => {
      filesPage.remove();
      Router.getInstance().resetRouteForTesting();
    });

    test('Navigates to OFFICE route on click', async () => {
      const officeRow =
          filesPage.shadowRoot!.querySelector<HTMLElement>('#office');
      assert(officeRow);

      officeRow.click();
      flush();
      assertEquals(Router.getInstance().currentRoute, routes.OFFICE);
    });
  });
});
