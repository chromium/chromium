// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsOneDriveSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {OneDriveBrowserProxy} from 'chrome://os-settings/os_settings.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {OneDriveTestBrowserProxy, ProxyOptions} from './one_drive_test_browser_proxy.js';

suite('<one-google-drive-subpage>', function() {
  /* The <one-google-drive-subpage> page. */
  let oneDrivePage: SettingsOneDriveSubpageElement;
  /* The BrowserProxy element to make assertions on when mojo methods are
     called. */
  let testOneDriveProxy: OneDriveTestBrowserProxy;

  teardown(function() {
    oneDrivePage.remove();
  });

  async function setupOneDrivePage(options: ProxyOptions) {
    testOneDriveProxy = new OneDriveTestBrowserProxy(options);
    OneDriveBrowserProxy.setInstance(testOneDriveProxy);
    oneDrivePage = document.createElement('settings-one-drive-subpage');
    document.body.appendChild(oneDrivePage);
    await oneDrivePage.initPromise;
    flush();
  }

  test(
      'display user email address and disconnect button when signed in',
      async () => {
        const email = 'email@gmail.com';
        await setupOneDrivePage({email});
        const signedInAsLabelElement =
            oneDrivePage.shadowRoot!.querySelector<HTMLDivElement>(
                '#signedInAsLabel')!;
        const connectDisconnectButton =
            oneDrivePage.shadowRoot!.querySelector<CrButtonElement>(
                '#oneDriveConnectDisconnect')!;
        assertEquals('Signed in as ' + email, signedInAsLabelElement.innerText);
        assertEquals('Disconnect', connectDisconnectButton.textContent!.trim());
      });

  test('display disconnected and connect button when signed out', async () => {
    await setupOneDrivePage({
      email: null,
    });
    const signedInAsLabelElement =
        oneDrivePage.shadowRoot!.querySelector<HTMLDivElement>(
            '#signedInAsLabel')!;
    const connectDisconnectButton =
        oneDrivePage.shadowRoot!.querySelector<CrButtonElement>(
            '#oneDriveConnectDisconnect')!;
    assertEquals('Disconnected', signedInAsLabelElement.innerText);
    assertEquals(
        'Connect account', connectDisconnectButton.textContent!.trim());
  });
});
