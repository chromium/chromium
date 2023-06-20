// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {OsSettingsFilesPageElement} from 'chrome://os-settings/lazy_load.js';
import {OneDriveBrowserProxy, Router, routes, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util_ts.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {assertAsync} from '../utils.js';

import {OneDriveTestBrowserProxy, ProxyOptions} from './one_drive_test_browser_proxy.js';

suite('<os-settings-files-page>', () => {
  /* The <os-settings-files-page> app. */
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

  test(
      'OneDrive and Office rows are hidden when showOfficeSettings is false',
      async () => {
        assertEquals(
            null, filesPage.shadowRoot!.querySelector('#OneDriveLink'));
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
    /* The BrowserProxy element to make assertions on when mojo methods are
       called. */
    let testOneDriveProxy: OneDriveTestBrowserProxy;

    setup(() => {
      loadTimeData.overrideValues({
        showOfficeSettings: true,
      });
      flush();
    });

    teardown(() => {
      filesPage.remove();
      Router.getInstance().resetRouteForTesting();
      testOneDriveProxy.handler.reset();
    });

    async function setupFilesPage(options: ProxyOptions) {
      testOneDriveProxy = new OneDriveTestBrowserProxy(options);
      OneDriveBrowserProxy.setInstance(testOneDriveProxy);
      filesPage = document.createElement('os-settings-files-page');
      document.body.appendChild(filesPage);
      await filesPage.initPromise;
    }

    test('OneDrive row shows Disconnected', async () => {
      await setupFilesPage({email: null});
      const oneDriveRow = filesPage.shadowRoot!.querySelector<CrLinkRowElement>(
          '#OneDriveLink');
      assert(oneDriveRow);
      assertEquals('Disconnected', oneDriveRow.subLabel);
    });

    test('OneDrive row shows email address', async () => {
      const email = 'email@gmail.com';
      await setupFilesPage({
        email: email,
      });
      const oneDriveRow = filesPage.shadowRoot!.querySelector<CrLinkRowElement>(
          '#OneDriveLink');
      assert(oneDriveRow);
      assertEquals('Signed in as ' + email, oneDriveRow.subLabel);
    });

    test('OneDrive row adds email address on OneDrive mount', async () => {
      await setupFilesPage({email: null});
      const oneDriveRow = filesPage.shadowRoot!.querySelector<CrLinkRowElement>(
          '#OneDriveLink');
      assert(oneDriveRow);
      assertEquals('Disconnected', oneDriveRow.subLabel);

      // Simulate OneDrive mount: mount signal to observer and ability to return
      // an email address.
      const email = 'email@gmail.com';
      testOneDriveProxy.handler.setResultFor(
          'getUserEmailAddress', {email: email});
      testOneDriveProxy.observerRemote.onODFSMountOrUnmount();

      await assertAsync(() => oneDriveRow.subLabel === 'Signed in as ' + email);
    });

    test('OneDrive row removes email address on OneDrive unmount', async () => {
      const email = 'email@gmail.com';
      await setupFilesPage({
        email: email,
      });
      const oneDriveRow = filesPage.shadowRoot!.querySelector<CrLinkRowElement>(
          '#OneDriveLink');
      assert(oneDriveRow);
      assertEquals('Signed in as ' + email, oneDriveRow.subLabel);

      // Simulate OneDrive unmount: unmount signal and returns an empty email
      // address.
      testOneDriveProxy.handler.setResultFor(
          'getUserEmailAddress', {email: null});
      testOneDriveProxy.observerRemote.onODFSMountOrUnmount();

      await assertAsync(() => oneDriveRow.subLabel === 'Disconnected');
    });

    test('Navigates to OFFICE route on click', async () => {
      await setupFilesPage({email: null});
      const officeRow =
          filesPage.shadowRoot!.querySelector<HTMLElement>('#office');
      assert(officeRow);

      officeRow.click();
      flush();
      assertEquals(Router.getInstance().currentRoute, routes.OFFICE);
    });
  });
});
