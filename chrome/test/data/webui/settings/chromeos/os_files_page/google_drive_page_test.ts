// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {CrSettingsPrefs, GoogleDriveBrowserProxy, GoogleDrivePageCallbackRouter, GoogleDrivePageHandlerRemote, GoogleDrivePageRemote, SettingsGoogleDriveSubpageElement, SettingsPrefsElement, SettingsToggleButtonElement, Stage} from 'chrome://os-settings/chromeos/os_settings.js';
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

  observerRemote: GoogleDrivePageRemote;

  constructor() {
    super(['calculateRequiredSpace']);
    this.handler = TestMock.fromClass(GoogleDrivePageHandlerRemote);
    this.observer = new GoogleDrivePageCallbackRouter();
    this.observerRemote = this.observer.$.bindNewPipeAndPassRemote();
  }
}

/**
 * Generate the expected text for space available.
 */
function generateRequiredSpaceText(
    requiredSpace: string, remainingSpace: string): string {
  return `This will use about ${requiredSpace} leaving ${
      remainingSpace} available.`;
}

suite('<settings-google-drive-subpage>', function() {
  let page: SettingsGoogleDriveSubpageElement;
  let prefElement: SettingsPrefsElement;
  let connectDisconnectButton: CrButtonElement;
  let testBrowserProxy: GoogleDriveTestBrowserProxy;
  let bulkPinningToggle: SettingsToggleButtonElement;

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

    bulkPinningToggle =
        querySelectorShadow(page.shadowRoot!, ['#driveBulkPinning']) as
        SettingsToggleButtonElement;
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

  test(
      'clicking the toggle updates the bulk pinning preference',
      async function() {
        page.setPrefValue('drivefs.bulk_pinning_enabled', false);
        bulkPinningToggle.click();
        await assertAsync(
            () => page.getPref('drivefs.bulk_pinning_enabled').value, 5000);
      });

  test(
      'progress sent via the browser proxy updates the sub title text',
      async function() {
        page.setPrefValue('drivefs.bulk_pinning_enabled', false);

        /**
         * Helper method to retrieve the subtitle text from the bulk pinning
         * label.
         */
        const expectSubTitleText = async (fn: (text: string) => boolean) => {
          let subTitleElement: HTMLElement|null;
          await assertAsync(() => {
            subTitleElement =
                bulkPinningToggle.shadowRoot!.querySelector<HTMLElement>(
                    '#sub-label-text');
            return subTitleElement !== null && fn(subTitleElement!.innerText);
          }, 5000);
        };


        // Expect the subtitle text does not include required space when no
        // values have been returned from the page handler.
        const requiredSpaceText =
            generateRequiredSpaceText('512 MB', '1,024 KB');
        await expectSubTitleText(
            subTitle => !subTitle.includes(requiredSpaceText));

        // Mock space values and the `kSuccess` stage via the browser proxy.
        testBrowserProxy.observerRemote.onProgress({
          remainingSpace: '1,024 KB',
          requiredSpace: '512 MB',
          stage: Stage.kSuccess,
        });
        testBrowserProxy.observerRemote.$.flushForTesting();
        flush();

        // Ensure the sub title text gets updated with the space values.
        await expectSubTitleText(
            subTitle => subTitle.includes(requiredSpaceText));

        // Mock a failure case via the browser proxy.
        testBrowserProxy.observerRemote.onProgress({
          remainingSpace: '1,024 KB',
          requiredSpace: '512 MB',
          stage: Stage.kCannotGetFreeSpace,
        });
        testBrowserProxy.observerRemote.$.flushForTesting();
        flush();

        // Ensure the sub title textremoves the space values.
        await expectSubTitleText(
            subTitle => !subTitle.includes(requiredSpaceText));
      });
});
