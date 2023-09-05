// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {OsSettingsFilesPageElement, SmbBrowserProxy, SmbBrowserProxyImpl, SmbMountResult} from 'chrome://os-settings/lazy_load.js';
import {CrLinkRowElement, CrSettingsPrefs, OneDriveBrowserProxy, Router, routes, SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util_ts.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {assertAsync} from '../utils.js';

import {OneDriveTestBrowserProxy, ProxyOptions} from './one_drive_test_browser_proxy.js';

class TestSmbBrowserProxy extends TestBrowserProxy implements SmbBrowserProxy {
  smbMountResult = SmbMountResult.SUCCESS;
  anySmbMounted = false;

  constructor() {
    super([
      'hasAnySmbMountedBefore',
    ]);
  }

  smbMount(
      smbUrl: string, smbName: string, username: string, password: string,
      authMethod: string, shouldOpenFileManagerAfterMount: boolean,
      saveCredentials: boolean): Promise<SmbMountResult> {
    this.methodCalled(
        'smbMount', smbUrl, smbName, username, password, authMethod,
        shouldOpenFileManagerAfterMount, saveCredentials);
    return Promise.resolve(this.smbMountResult);
  }

  startDiscovery(): void {
    this.methodCalled('startDiscovery');
  }

  updateCredentials(mountId: string, username: string, password: string): void {
    this.methodCalled('updateCredentials', mountId, username, password);
  }

  hasAnySmbMountedBefore(): Promise<boolean> {
    this.methodCalled('hasAnySmbMountedBefore');
    return Promise.resolve(this.anySmbMounted);
  }
}

suite('<os-settings-files-page>', () => {
  /* The <os-settings-files-page> app. */
  let filesPage: OsSettingsFilesPageElement;

  /* The element that maintains all the preferences. */
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

  /**
   * Helper function to tear down the elements on the page and reset the route
   * for testing.
   */
  function teardownFilesPage() {
    filesPage?.remove();
    prefElement?.remove();
    Router.getInstance()?.resetRouteForTesting();
  }

  /**
   * Ensures any existing files page and prefs elements are torn down and then
   * sets up the files page with the prefs element associated.
   */
  async function resetFilesPageWithLoadTimeData(data: {[key: string]: any}) {
    smbBrowserProxy.resetResolver('hasAnySmbMountedBefore');
    teardownFilesPage();

    loadTimeData.overrideValues(data);
    prefElement = document.createElement('settings-prefs');
    const settingsPrivate = new FakeSettingsPrivate(getFakePrefs()) as
        unknown as typeof chrome.settingsPrivate;
    prefElement.initialize(settingsPrivate);

    await CrSettingsPrefs.initialized;
    filesPage = document.createElement('os-settings-files-page');
    filesPage.prefs = prefElement.prefs;
    document.body.appendChild(filesPage);
    await filesPage.initPromise;
  }

  suiteSetup(() => {
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(async () => {
    smbBrowserProxy = new TestSmbBrowserProxy();
    SmbBrowserProxyImpl.setInstance(smbBrowserProxy);

    await resetFilesPageWithLoadTimeData({
      showOfficeSettings: false,
      enableDriveFsBulkPinning: false,
      showGoogleDriveSettingsPage: false,
    });
  });

  teardown(teardownFilesPage);

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

  test('Smb Shares row is focused after returning from subpage', async () => {
    Router.getInstance().navigateTo(routes.FILES);

    // Sub-page trigger navigates to Smb Shares subpage
    const triggerSelector = '#smbSharesRow';
    const subpageTrigger =
        filesPage.shadowRoot!.querySelector<HTMLElement>(triggerSelector);
    assert(subpageTrigger);
    subpageTrigger.click();
    flush();
    assertEquals(routes.SMB_SHARES, Router.getInstance().currentRoute);

    // Navigate back
    const popStateEventPromise = eventToPromise('popstate', window);
    Router.getInstance().navigateToPreviousRoute();
    await popStateEventPromise;
    await waitAfterNextRender(filesPage);

    assertEquals(
        subpageTrigger, filesPage.shadowRoot!.activeElement,
        `${triggerSelector} should be focused.`);
  });

  test(
      'OneDrive and Office rows are hidden when showOfficeSettings is false',
      async () => {
        assertEquals(
            null, filesPage.shadowRoot!.querySelector('#OneDriveLink'));
        assertEquals(null, filesPage.shadowRoot!.querySelector('#office'));
      });

  test(
      'Google Drive row is hidden when isBulkPinningEnabled_ is disabled',
      async () => {
        assertEquals(
            null, filesPage.shadowRoot!.querySelector('#GoogleDriveLink'));
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

    async function setupFilesPage(options: ProxyOptions) {
      testOneDriveProxy = new OneDriveTestBrowserProxy(options);
      OneDriveBrowserProxy.setInstance(testOneDriveProxy);
      return resetFilesPageWithLoadTimeData({showOfficeSettings: true});
    }

    teardown(() => {
      testOneDriveProxy.handler.reset();
    });

    test('OneDrive row shows Disconnected', async () => {
      await setupFilesPage({email: null});
      const oneDriveRow = filesPage.shadowRoot!.querySelector<CrLinkRowElement>(
          '#OneDriveLink');
      assert(oneDriveRow);
      assertEquals('Add your Microsoft account', oneDriveRow.subLabel);
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
      assertEquals('Add your Microsoft account', oneDriveRow.subLabel);

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

      await assertAsync(
          () => oneDriveRow.subLabel === 'Add your Microsoft account');
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

  suite('with enableDriveFsBulkPinning set to true', () => {
    let googleDriveRow: CrLinkRowElement;

    setup(async () => {
      await resetFilesPageWithLoadTimeData({
        enableDriveFsBulkPinning: true,
        showGoogleDriveSettingsPage: false,
      });

      googleDriveRow = filesPage.shadowRoot!.querySelector<CrLinkRowElement>(
          '#GoogleDriveLink')!;
      assert(googleDriveRow);
    });

    test('with gdata.disabled set to true, text shows appropriately', () => {
      filesPage.setPrefValue('gdata.disabled', true);
      flush();

      assertEquals('Not signed in', googleDriveRow.subLabel);
    });

    test('with gdata.disabled set to false, but file sync disabled', () => {
      filesPage.setPrefValue('drivefs.bulk_pinning_enabled', false);
      flush();

      assertTrue(googleDriveRow.subLabel.startsWith('Signed in as'));
    });

    test('with gdata.disabled set to false, and file sync enabled', () => {
      filesPage.setPrefValue('drivefs.bulk_pinning_enabled', true);
      flush();

      assertEquals('File sync on', googleDriveRow.subLabel);
    });

    test('cycling through the prefs updates the sublabel texts', () => {
      filesPage.setPrefValue('gdata.disabled', true);
      filesPage.setPrefValue('drivefs.bulk_pinning_enabled', false);
      flush();

      assertEquals('Not signed in', googleDriveRow.subLabel);

      filesPage.setPrefValue('gdata.disabled', false);
      flush();

      assertTrue(googleDriveRow.subLabel.startsWith('Signed in as'));

      filesPage.setPrefValue('drivefs.bulk_pinning_enabled', true);
      flush();

      assertEquals('File sync on', googleDriveRow.subLabel);
    });
  });

  suite(
      'with flag isRevampWayfindingEnabled enabled and no SMB mounted before,',
      () => {
        setup(async () => {
          // Must set before the page gets reconstructed.
          smbBrowserProxy.anySmbMounted = false;

          await resetFilesPageWithLoadTimeData({
            isRevampWayfindingEnabled: true,
          });

          // Called once files page has been created.
          await smbBrowserProxy.whenCalled('hasAnySmbMountedBefore');
        });

        test(
            'the Add File Share button only appears on the Files page when no file\
         share has been set-up.',
            () => {
              const addFilesButton =
                  filesPage.shadowRoot!.querySelector('#smbSharesRowButton');
              const addFilesRow =
                  filesPage.shadowRoot!.querySelector('#smbSharesRow');

              assertTrue(isVisible(addFilesButton));
              assertFalse(isVisible(addFilesRow));
            });
      });

  suite('with flag isRevampWayfindingEnabled enabled', () => {
    setup(async () => {
      // Must set before the page gets reconstructed.
      smbBrowserProxy.anySmbMounted = true;

      await resetFilesPageWithLoadTimeData({
        isRevampWayfindingEnabled: true,
      });

      // Called once files page has been created.
      await smbBrowserProxy.whenCalled('hasAnySmbMountedBefore');
    });

    test(
        'the Network Files share clickable row appears when file share has been\
         previously set-up.',
        () => {
          const addFilesButton =
              filesPage.shadowRoot!.querySelector('#smbSharesRowButton');
          const addFilesRow =
              filesPage.shadowRoot!.querySelector('#smbSharesRow');

          assertTrue(isVisible(addFilesRow));
          assertFalse(isVisible(addFilesButton));
        });
  });

  suite('with flag isRevampWayfindingEnabled disabled', () => {
    setup(async () => {
      await resetFilesPageWithLoadTimeData({
        isRevampWayfindingEnabled: false,
      });
    });

    test(
        'the Network Files share clickable row appears on the Files page',
        () => {
          const addFilesButton =
              filesPage.shadowRoot!.querySelector('#smbSharesRowButton');
          const addFilesRow =
              filesPage.shadowRoot!.querySelector('#smbSharesRow');

          assertFalse(isVisible(addFilesButton));
          assertTrue(isVisible(addFilesRow));
        });
  });

  suite('with showGoogleDriveSettingsPage  set to true', async () => {
    let googleDriveRow: CrLinkRowElement;

    setup(async () => {
      await resetFilesPageWithLoadTimeData({
        enableDriveFsBulkPinning: true,
        showGoogleDriveSettingsPage: false,
      });

      googleDriveRow = filesPage.shadowRoot!.querySelector<CrLinkRowElement>(
          '#GoogleDriveLink')!;
      assert(googleDriveRow);
    });

    test('cycling through the prefs updates the sublabel texts', () => {
      filesPage.setPrefValue('gdata.disabled', true);
      flush();

      assertEquals('Not signed in', googleDriveRow.subLabel);

      filesPage.setPrefValue('gdata.disabled', false);
      flush();

      assertTrue(googleDriveRow.subLabel.startsWith('Signed in as'));
    });
  });
});
