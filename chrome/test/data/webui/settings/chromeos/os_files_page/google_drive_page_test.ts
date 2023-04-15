// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {CrSettingsPrefs, GoogleDriveBrowserProxy, GoogleDrivePageCallbackRouter, GoogleDrivePageHandlerRemote, SettingsGoogleDriveSubpageElement, SettingsPrefsElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {assertAsync, querySelectorShadow} from '../utils.js';

/**
 * A fake BrowserProxy implementation that enables switching out the real one to
 * mock various mojo responses.
 */
class GoogleDriveTestBrowserProxy extends TestBrowserProxy implements
    GoogleDriveBrowserProxy {
  handler: TestMock<GoogleDrivePageHandlerRemote>&GoogleDrivePageHandlerRemote;

  observer: GoogleDrivePageCallbackRouter;

  constructor() {
    super(['calculateRequiredSpace']);
    this.handler = TestMock.fromClass(GoogleDrivePageHandlerRemote);
    this.observer = new GoogleDrivePageCallbackRouter();
  }
}

suite('<settings-google-drive-subpage>', function() {
  let page: SettingsGoogleDriveSubpageElement;
  let prefElement: SettingsPrefsElement;
  let connectDisconnectButton: CrButtonElement;
  let testBrowserProxy: GoogleDriveTestBrowserProxy;

  setup(async function() {
    testBrowserProxy = new GoogleDriveTestBrowserProxy();
    GoogleDriveBrowserProxy.setInstance(testBrowserProxy);

    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    page = document.createElement('settings-google-drive-subpage');
    page.prefs = prefElement.prefs;
    document.body.appendChild(page);
    flush();

    connectDisconnectButton =
        querySelectorShadow(
            page.shadowRoot!, ['#driveConnectDisconnect', 'cr-button']) as
        CrButtonElement;
  });

  teardown(function() {
    page.remove();
    prefElement.remove();
  });

  test('drive connect label is updated when pref is changed', function() {
    // Update the preference and ensure the text has the value "Connect
    // account".
    page.setPrefValue('gdata.disabled', true);
    flush();
    assertEquals(
        connectDisconnectButton!.textContent!.trim(), 'Connect account');

    // Update the preference and ensure the text has the value "Disconnect".
    page.setPrefValue('gdata.disabled', false);
    flush();
    assertEquals(connectDisconnectButton!.textContent!.trim(), 'Disconnect');
  });

  test('confirming drive disconnect updates pref', async function() {
    page.setPrefValue('gdata.disabled', false);
    flush();

    connectDisconnectButton.click();

    const getDisconnectConfirmationButton = () => {
      return querySelectorShadow(
                 page.shadowRoot!,
                 [
                   'settings-disconnect-drive-confirmation-dialog',
                   '.action-button',
                 ]) as CrButtonElement |
          null;
    };

    // Wait for the disconnect confirmation button to be visible.
    await assertAsync(() => getDisconnectConfirmationButton() !== null);
    const disconnectButton = getDisconnectConfirmationButton()!;
    disconnectButton.click();

    // Ensure after clicking the disconnect button the preference is true
    // (timeout after 5s).
    await assertAsync(() => page.getPref('gdata.disabled').value, 5000);
  });

  test(
      'cancelling drive disconnect confirmation dialog doesnt update pref',
      async function() {
        page.setPrefValue('gdata.disabled', false);
        flush();

        connectDisconnectButton.click();

        const getCancelConfirmationDialogButton = () => {
          return querySelectorShadow(
                     page.shadowRoot!,
                     [
                       'settings-disconnect-drive-confirmation-dialog',
                       '.cancel-button',
                     ]) as CrButtonElement |
              null;
        };

        // Wait for the disconnect confirmation button to be visible.
        await assertAsync(() => getCancelConfirmationDialogButton() !== null);
        const cancelButton = getCancelConfirmationDialogButton()!;
        cancelButton.click();

        // Ensure after cancelling the dialog the preference is unchanged.
        await assertAsync(() => !page.getPref('gdata.disabled').value, 5000);
      });
});
