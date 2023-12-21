// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {ConfirmationDialogType, CrButtonElement, CrSettingsPrefs, GoogleDriveBrowserProxy, GoogleDrivePageCallbackRouter, GoogleDrivePageHandlerRemote, GoogleDrivePageRemote, PaperTooltipElement, SettingsGoogleDriveSubpageElement, SettingsPrefsElement, SettingsToggleButtonElement, Stage} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
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
    super(
        ['calculateRequiredSpace', 'getContentCacheSize', 'clearPinnedFiles']);
    this.handler = TestMock.fromClass(GoogleDrivePageHandlerRemote);
    this.observer = new GoogleDrivePageCallbackRouter();
    this.observerRemote = this.observer.$.bindNewPipeAndPassRemote();
  }
}

/**
 * Generate the expected text for space available.
 */
function generateRequiredSpaceText(
    requiredSpace: string, freeSpace: string): string {
  return `This will use about ${requiredSpace}. You currently have ${
      freeSpace} available.`;
}

suite('<settings-google-drive-subpage>', function() {
  let page: SettingsGoogleDriveSubpageElement;
  let prefElement: SettingsPrefsElement;
  let connectDisconnectButton: CrButtonElement;
  let testBrowserProxy: GoogleDriveTestBrowserProxy;
  let bulkPinningToggle: SettingsToggleButtonElement;
  let offlineStorageSubtitle: HTMLDivElement;
  let clearOfflineStorageButton: CrButtonElement;
  let driveDisabledOverCellularToggle: SettingsToggleButtonElement;

  /**
   * Helper to ensure a confirmation dialog is showing, retrieve a button in the
   * dialog and click it, then assert the dialog has disappeared.
   */
  const clickConfirmationDialogButton =
      async(selector: string): Promise<void> => {
    const getButton = () => querySelectorShadow(
                                page.shadowRoot!,
                                [
                                  'settings-drive-confirmation-dialog',
                                  selector,
                                ]) as CrButtonElement |
        null;

    // Ensure some dialog is showing.
    await assertAsync(
        () => page.dialogType !== ConfirmationDialogType.NONE, 5000);

    // Ensure the button requested is showing.
    await assertAsync(() => getButton() !== null);

    // Click the button and wait for the dialog to disappear.
    getButton()!.click();
    await assertAsync(
        () => page.dialogType === ConfirmationDialogType.NONE, 5000);
  };

  const getClearOfflineStorageTooltipText = (): string =>
      page.shadowRoot!
          .querySelector<PaperTooltipElement>(
              '#cleanUpStorageTooltip')!.textContent!.trim();

  setup(async () => {
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

    offlineStorageSubtitle = page.shadowRoot!.querySelector<HTMLDivElement>(
        '#drive-offline-storage-row .secondary')!;

    clearOfflineStorageButton = page.shadowRoot!.querySelector<CrButtonElement>(
        '#drive-offline-storage-row cr-button')!;

    driveDisabledOverCellularToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#driveEnableDriveOnMeteredNetworkToggle')!;
  });

  teardown(function() {
    page.remove();
    prefElement.remove();
  });

  suite('with bulk pinning disabled', () => {
    suiteSetup(async () => {
      loadTimeData.overrideValues({
        enableDriveFsBulkPinning: false,
      });
    });

    test('file sync should not show when bulk pinning disabled', async () => {
      assertEquals(bulkPinningToggle, null);
    });


    test('drive connect label is updated when pref is changed', function() {
      // Update the preference and ensure the text has the value "Connect
      // account".
      page.setPrefValue('gdata.disabled', true);
      flush();
      assertEquals('Connect', connectDisconnectButton!.textContent!.trim());

      // Update the preference and ensure the text has the value "Remove Drive
      // access".
      page.setPrefValue('gdata.disabled', false);
      flush();
      assertEquals(
          'Remove Drive access', connectDisconnectButton!.textContent!.trim());
    });

    test('confirming drive disconnect updates pref', async () => {
      page.setPrefValue('gdata.disabled', false);
      flush();

      // Click the connect disconnect button.
      connectDisconnectButton.click();

      // Click the disconnect button.
      await clickConfirmationDialogButton('.action-button');

      // Ensure after clicking the disconnect button the preference is true
      // (timeout after 5s).
      await assertAsync(() => page.getPref('gdata.disabled').value, 5000);
    });

    test(
        'cancelling drive disconnect confirmation dialog doesnt update pref',
        async () => {
          page.setPrefValue('gdata.disabled', false);
          flush();

          // Click the connect disconnect button.
          connectDisconnectButton.click();

          // Wait for the disconnect confirmation button to be visible.
          await clickConfirmationDialogButton('.cancel-button');

          // Ensure after cancelling the dialog the preference is unchanged.
          await assertAsync(() => !page.getPref('gdata.disabled').value, 5000);
        });


    test('free space shows the offline value returned', async () => {
      // Send back a normal pinned size result.
      testBrowserProxy.handler.setResultFor(
          'getContentCacheSize', {size: '100 MB'});
      page.onNavigated();
      await assertAsync(
          () => offlineStorageSubtitle.innerText === 'Using 100 MB');

      // Mock an empty pinned size (size is there but an empty string).
      testBrowserProxy.handler.setResultFor('getContentCacheSize', {size: ''});
      page.onNavigated();
      await assertAsync(() => offlineStorageSubtitle.innerText === 'Unknown');
    });


    test('when clear offline files clicked show dialog', async () => {
      page.setPrefValue('drivefs.bulk_pinning_enabled', false);
      testBrowserProxy.handler.setResultFor(
          'getContentCacheSize', {size: '100 MB'});
      page.onNavigated();
      await assertAsync(() => !clearOfflineStorageButton.disabled);

      clearOfflineStorageButton.click();
      await assertAsync(
          () => page.dialogType ===
              ConfirmationDialogType.BULK_PINNING_CLEAN_UP_STORAGE,
          5000);
      await clickConfirmationDialogButton('.cancel-button');
      assertEquals(
          testBrowserProxy.handler.getCallCount('clearPinnedFiles'), 0);

      clearOfflineStorageButton.click();
      await assertAsync(
          () => page.dialogType ===
              ConfirmationDialogType.BULK_PINNING_CLEAN_UP_STORAGE,
          5000);
      await clickConfirmationDialogButton('.action-button');
      await assertAsync(
          () =>
              testBrowserProxy.handler.getCallCount('clearPinnedFiles') === 1);
    });

    test('clean up storage button is disabled at 0 B', async () => {
      testBrowserProxy.handler.setResultFor(
          'getContentCacheSize', {size: '0 B'});
      page.onNavigated();
      await assertAsync(() => clearOfflineStorageButton.disabled);

      testBrowserProxy.handler.setResultFor(
          'getContentCacheSize', {size: '100 MB'});
      page.onNavigated();
      await assertAsync(() => !clearOfflineStorageButton.disabled);

      testBrowserProxy.handler.setResultFor(
          'getContentCacheSize', {size: '0 B'});
      page.onNavigated();
      await assertAsync(() => clearOfflineStorageButton.disabled);

      assertEquals(
          'No offline storage to clean up',
          getClearOfflineStorageTooltipText());
    });

    test('disabling drive over cellular toggles pref', async () => {
      page.setPrefValue('gdata.cellular.disabled', false);
      flush();

      // Click the connect disconnect button.
      driveDisabledOverCellularToggle.click();
      await assertAsync(
          () => page.getPref('gdata.cellular.disabled').value, 5000);

      driveDisabledOverCellularToggle.click();
      await assertAsync(
          () => !page.getPref('gdata.cellular.disabled').value, 5000);
    });
  });

  suite('with bulk pinning enabled', () => {
    suiteSetup(async () => {
      loadTimeData.overrideValues({
        enableDriveFsBulkPinning: true,
      });
    });

    test('removing drive access also disables bulk pinning', async () => {
      page.setPrefValue('gdata.disabled', false);
      page.setPrefValue('drivefs.bulk_pinning_enabled', true);
      flush();

      // Click the connect disconnect button.
      connectDisconnectButton.click();

      // Wait for the disconnect confirmation button to be visible.
      await clickConfirmationDialogButton('.action-button');

      // Once disabled the pref must be updated for both drive disabled and for
      // bulk pinning to be disabled.
      await assertAsync(() => page.getPref('gdata.disabled').value, 5000);
      assertFalse(page.getPref('drivefs.bulk_pinning_enabled').value);
    });

    test(
        'clicking the toggle updates the bulk pinning preference', async () => {
          page.setPrefValue('drivefs.bulk_pinning_enabled', false);
          flush();

          // Toggle the bulk pinning toggle.
          bulkPinningToggle.click();

          // Ensure the bulk pinning preference is enabled (timeout after 5s).
          await assertAsync(
              () => page.getPref('drivefs.bulk_pinning_enabled').value, 5000);
        });

    test(
        'clicking the toggle whilst listing files shows a dialog', async () => {
          page.setPrefValue('drivefs.bulk_pinning_enabled', false);
          flush();

          testBrowserProxy.observerRemote.onProgress({
            freeSpace: '1,024 KB',
            requiredSpace: '512 MB',
            stage: Stage.kListingFiles,
            listedFiles: BigInt(100),
            isError: false,
          });
          testBrowserProxy.observerRemote.$.flushForTesting();
          flush();

          // Wait until the `onProgress` changes have been received.
          await assertAsync(() => page.listedFiles === 100n);

          // Toggle the bulk pinning toggle.
          bulkPinningToggle.click();

          // Wait for the clisting files dialog to appear and then close it.
          await assertAsync(
              () => page.dialogType ===
                  ConfirmationDialogType.BULK_PINNING_LISTING_FILES,
              5000);
          await clickConfirmationDialogButton('.cancel-button');

          // Assert the bulk pinning pref was not enabled and the toggle was not
          // checked.
          assertFalse(
              page.getPref('drivefs.bulk_pinning_enabled').value,
              'Pinning pref should be false');
          assertFalse(
              bulkPinningToggle.checked, 'Pinning toggle should be false');
        });

    test(
        'progress sent via the browser proxy updates the sub title text',
        async () => {
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
            freeSpace: '1,024 KB',
            requiredSpace: '512 MB',
            stage: Stage.kSuccess,
            listedFiles: BigInt(100),
            isError: false,
          });
          testBrowserProxy.observerRemote.$.flushForTesting();
          flush();

          // Ensure the sub title text gets updated with the space values.
          await expectSubTitleText(
              subTitle => subTitle.includes(requiredSpaceText));

          // Mock a failure case via the browser proxy.
          testBrowserProxy.observerRemote.onProgress({
            freeSpace: '1,024 KB',
            requiredSpace: '512 MB',
            stage: Stage.kCannotGetFreeSpace,
            listedFiles: BigInt(0),
            isError: true,
          });
          testBrowserProxy.observerRemote.$.flushForTesting();
          flush();

          // Ensure the sub title textremoves the space values.
          await expectSubTitleText(
              subTitle => !subTitle.includes(requiredSpaceText));
        });

    test('disabling bulk pinning shows confirmation dialog', async () => {
      page.setPrefValue('drivefs.bulk_pinning_enabled', true);
      flush();

      // Click the bulk pinning toggle.
      bulkPinningToggle.click();

      // Wait for the confirmation dialog to appear and click the cancel button.
      await clickConfirmationDialogButton('.cancel-button');

      // Expect the preference to not be changed and the toggle to stay checked.
      assertTrue(
          page.getPref('drivefs.bulk_pinning_enabled').value,
          'Pinning pref should be true');
      assertTrue(bulkPinningToggle.checked, 'Pinning toggle should be true');

      // Click the bulk pinning toggle.
      bulkPinningToggle.click();

      // Wait for the confirmation dialog to appear and click the "Turn off"
      // button.
      await assertAsync(
          () => page.dialogType === ConfirmationDialogType.BULK_PINNING_DISABLE,
          5000);
      await clickConfirmationDialogButton('.action-button');

      assertFalse(
          page.getPref('drivefs.bulk_pinning_enabled').value,
          'Pinning pref should be false');
      assertFalse(bulkPinningToggle.checked, 'Pinning toggle should be false');
    });

    test(
        'atempting to enable bulk pinning when no free space shows dialog',
        async () => {
          page.setPrefValue('drivefs.bulk_pinning_enabled', false);

          // Mock space values and the `kNotEnoughSpace` stage via the browser
          // proxy.
          testBrowserProxy.observerRemote.onProgress({
            freeSpace: '512 MB',
            requiredSpace: '1,024 MB',
            stage: Stage.kNotEnoughSpace,
            listedFiles: BigInt(100),
            isError: true,
          });
          testBrowserProxy.observerRemote.$.flushForTesting();
          flush();

          // Wait for the page to update the progress information.
          await assertAsync(() => page.freeSpace === '512 MB');

          // Click the bulk pinning toggle.
          bulkPinningToggle.click();

          // Wait for the confirmation dialog to appear and assert the toggle
          // hasn't been enabled when the dialog is visible, then click the
          // "Cancel" button.
          await assertAsync(
              () => page.dialogType ===
                  ConfirmationDialogType.BULK_PINNING_NOT_ENOUGH_SPACE,
              5000);
          await assertAsync(() => !bulkPinningToggle.checked);
          await clickConfirmationDialogButton('.cancel-button');

          // Wait for the dialog to be dismissed, then assert the toggle hasn't
          // been checked and the preference hasn't been set.
          await assertAsync(
              () => page.dialogType === ConfirmationDialogType.NONE, 5000);
          assertFalse(
              page.getPref('drivefs.bulk_pinning_enabled').value,
              'Pinning pref should be false');
          assertFalse(
              bulkPinningToggle.checked,
              'Pinning toggle should not be toggled');
        });

    test(
        'attempting to enable bulk pinning when' +
            'unknown error occurs show dialog',
        async () => {
          page.setPrefValue('drivefs.bulk_pinning_enabled', false);

          // Mock space values and the `kNotEnoughSpace` stage via the browser
          // proxy.
          testBrowserProxy.observerRemote.onProgress({
            freeSpace: 'x',
            requiredSpace: 'y',
            stage: Stage.kCannotGetFreeSpace,
            listedFiles: BigInt(0),
            isError: true,
          });
          testBrowserProxy.observerRemote.$.flushForTesting();
          flush();

          // Wait for the page to update the progress information.
          await assertAsync(() => page.freeSpace === 'x');

          // Click the bulk pinning toggle.
          bulkPinningToggle.click();

          // Wait for the confirmation dialog to appear and assert the toggle
          // hasn't been enabled when the dialog is visible, then click the
          // "Cancel" button.
          await assertAsync(
              () => page.dialogType ===
                  ConfirmationDialogType.BULK_PINNING_UNEXPECTED_ERROR,
              5000);
          await assertAsync(() => !bulkPinningToggle.checked);
          await clickConfirmationDialogButton('.cancel-button');

          // Wait for the dialog to be dismissed, then assert the toggle hasn't
          // been checked and the preference hasn't been set.
          await assertAsync(
              () => page.dialogType === ConfirmationDialogType.NONE, 5000);
          assertFalse(
              page.getPref('drivefs.bulk_pinning_enabled').value,
              'Pinning pref should be false');
          assertFalse(
              bulkPinningToggle.checked,
              'Pinning toggle should not be toggled');
        });

    test('clear offline files disabled when bulk pinning enabled', async () => {
      page.setPrefValue('drivefs.bulk_pinning_enabled', false);
      testBrowserProxy.handler.setResultFor(
          'getContentCacheSize', {size: '100 MB'});
      page.onNavigated();
      testBrowserProxy.observerRemote.onProgress({
        freeSpace: 'x',
        requiredSpace: 'y',
        stage: Stage.kStopped,
        listedFiles: BigInt(100),
        isError: false,
      });
      testBrowserProxy.observerRemote.$.flushForTesting();
      await assertAsync(() => !clearOfflineStorageButton.disabled);

      page.setPrefValue('drivefs.bulk_pinning_enabled', true);
      testBrowserProxy.handler.setResultFor(
          'getContentCacheSize', {size: '100 MB'});
      page.onNavigated();
      testBrowserProxy.observerRemote.onProgress({
        freeSpace: 'x',
        requiredSpace: 'y',
        stage: Stage.kSyncing,
        listedFiles: BigInt(100),
        isError: false,
      });
      testBrowserProxy.observerRemote.$.flushForTesting();

      // Wait until the page has the right content cache size and the offline
      // storage button is disabled.
      await assertAsync(
          () => page.contentCacheSize === '100 MB' &&
              clearOfflineStorageButton.disabled);
      assertEquals(
          'Can’t clean up storage while file sync is on',
          getClearOfflineStorageTooltipText());
    });

    test(
        'disabling bulk pinning whilst offline shows confirmation dialog',
        async () => {
          page.setPrefValue('drivefs.bulk_pinning_enabled', true);
          flush();

          // Mock space values and the `kPausedOffline` stage via the browser
          // proxy.
          testBrowserProxy.observerRemote.onProgress({
            freeSpace: '512 MB',
            requiredSpace: '1,024 MB',
            stage: Stage.kPausedOffline,
            listedFiles: BigInt(100),
            isError: false,
          });
          testBrowserProxy.observerRemote.$.flushForTesting();
          flush();

          // Wait for the stage to propagate to the page.
          await assertAsync(() => page.stage === Stage.kPausedOffline, 1000);

          // Click the bulk pinning toggle.
          bulkPinningToggle.click();

          // Wait for the confirmation dialog to appear and click the "Turn off"
          // button.
          await assertAsync(
              () => page.dialogType ===
                  ConfirmationDialogType.BULK_PINNING_DISABLE,
              5000);
          await clickConfirmationDialogButton('.action-button');

          assertFalse(
              page.getPref('drivefs.bulk_pinning_enabled').value,
              'Pinning pref should be false');
          assertFalse(
              bulkPinningToggle.checked, 'Pinning toggle should be false');
        });
  });
});
