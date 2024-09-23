// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {FilesSettingsCardElement, OneDriveConnectionState, SmbBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {createRouterForTesting, CrLinkRowElement, CrSettingsPrefs, OneDriveBrowserProxy, Route, Router, routes, SettingsPrefsElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {OneDriveTestBrowserProxy, ProxyOptions} from './one_drive_test_browser_proxy.js';
import {TestSmbBrowserProxy} from './test_smb_browser_proxy.js';

suite('<files-settings-card>', () => {
  const isRevampWayfindingEnabled =
      loadTimeData.getBoolean('isRevampWayfindingEnabled');
  const route =
      isRevampWayfindingEnabled ? routes.SYSTEM_PREFERENCES : routes.FILES;

  let filesSettingsCard: FilesSettingsCardElement;
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

  async function createFilesSettingsCard() {
    prefElement = document.createElement('settings-prefs');
    const fakeSettingsPrivate = new FakeSettingsPrivate(getFakePrefs());
    prefElement.initialize(fakeSettingsPrivate);
    await CrSettingsPrefs.initialized;

    filesSettingsCard = document.createElement('files-settings-card');
    filesSettingsCard.prefs = prefElement.prefs;
    document.body.appendChild(filesSettingsCard);
    await flushTasks();
  }

  function getGoogleDriveRowSubLabel(): HTMLElement {
    const subLabel =
        filesSettingsCard.shadowRoot!.getElementById('googleDriveSubLabel');
    assert(subLabel);
    return subLabel;
  }

  async function assertSubpageTriggerFocused(
      triggerSelector: string, route: Route): Promise<void> {
    const subpageTrigger =
        filesSettingsCard.shadowRoot!.querySelector<HTMLElement>(
            triggerSelector);
    assert(subpageTrigger);

    // Subpage trigger navigates to subpage for route
    subpageTrigger.click();
    assertEquals(route, Router.getInstance().currentRoute);

    // Navigate back
    const popStateEventPromise = eventToPromise('popstate', window);
    Router.getInstance().navigateToPreviousRoute();
    await popStateEventPromise;
    await waitAfterNextRender(filesSettingsCard);

    assertEquals(
        subpageTrigger, filesSettingsCard.shadowRoot!.activeElement,
        `${triggerSelector} should be focused.`);
  }

  suiteSetup(() => {
    smbBrowserProxy = new TestSmbBrowserProxy();
    SmbBrowserProxyImpl.setInstance(smbBrowserProxy);

    CrSettingsPrefs.deferInitialization = true;
  });

  setup(() => {
    loadTimeData.overrideValues({
      showOneDriveSettings: false,
      showOfficeSettings: false,
      enableDriveFsBulkPinning: false,
    });

    Router.getInstance().navigateTo(route);
  });

  teardown(() => {
    filesSettingsCard.remove();
    prefElement.remove();
    smbBrowserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  test(
      'OneDrive row is not stamped when showOfficeSettings is false',
      async () => {
        await createFilesSettingsCard();
        assertNull(filesSettingsCard.shadowRoot!.querySelector('#oneDriveRow'));
      });

  test(
      'Office row is not stamped when showOfficeSettings is false',
      async () => {
        await createFilesSettingsCard();
        assertNull(filesSettingsCard.shadowRoot!.querySelector('#officeRow'));
      });

  test('Google Drive row sublabel changes based on pref value', async () => {
    await createFilesSettingsCard();
    filesSettingsCard.setPrefValue('gdata.disabled', true);
    flush();

    const googleDriveRowSubLabel = getGoogleDriveRowSubLabel();
    assertEquals('Not signed in', googleDriveRowSubLabel.innerText);

    filesSettingsCard.setPrefValue('gdata.disabled', false);
    flush();
    assertTrue(googleDriveRowSubLabel.innerText.startsWith('Signed in as'));
  });

  test('Google Drive row is focused when returning from subpage', async () => {
    await createFilesSettingsCard();

    await assertSubpageTriggerFocused('#googleDriveRow', routes.GOOGLE_DRIVE);
  });

  suite('with showOneDriveSettings set to true', () => {
    let testOneDriveBrowserProxy: OneDriveTestBrowserProxy;

    function setupBrowserProxy(options: ProxyOptions): void {
      testOneDriveBrowserProxy = new OneDriveTestBrowserProxy(options);
      OneDriveBrowserProxy.setInstance(testOneDriveBrowserProxy);
    }

    setup(() => {
      loadTimeData.overrideValues({showOneDriveSettings: true});

      // Reinitialize Router and routes based on load time data
      const testRouter = createRouterForTesting();
      Router.resetInstanceForTesting(testRouter);
    });

    teardown(() => {
      testOneDriveBrowserProxy.handler.reset();
    });

    test('OneDrive row shows disconnected when no email set up', async () => {
      setupBrowserProxy({email: null});
      await createFilesSettingsCard();

      const oneDriveRow =
          filesSettingsCard.shadowRoot!.querySelector<CrLinkRowElement>(
              '#oneDriveRow');
      assert(oneDriveRow);
      assertEquals('Add your Microsoft account', oneDriveRow.subLabel);
    });

    test('OneDrive row shows email address', async () => {
      const email = 'email@gmail.com';
      setupBrowserProxy({email});
      await createFilesSettingsCard();

      const oneDriveRow =
          filesSettingsCard.shadowRoot!.querySelector<CrLinkRowElement>(
              '#oneDriveRow');
      assert(oneDriveRow);
      assertEquals(`Signed in as ${email}`, oneDriveRow.subLabel);
    });

    test('OneDrive row shows loading state', async () => {
      const email = 'email@gmail.com';
      setupBrowserProxy({email});
      await createFilesSettingsCard();

      const oneDriveRow =
          filesSettingsCard.shadowRoot!.querySelector<CrLinkRowElement>(
              '#oneDriveRow');
      assert(oneDriveRow);

      // Change connection status to "LOADING".
      filesSettingsCard.updateOneDriveConnectionStateForTesting(
          OneDriveConnectionState.LOADING);
      flush();
      assertEquals('Loading…', oneDriveRow.subLabel);
    });

    test('OneDrive row shows email address on OneDrive mount', async () => {
      setupBrowserProxy({email: null});
      await createFilesSettingsCard();

      const oneDriveRow =
          filesSettingsCard.shadowRoot!.querySelector<CrLinkRowElement>(
              '#oneDriveRow');
      assert(oneDriveRow);
      assertEquals('Add your Microsoft account', oneDriveRow.subLabel);

      // Simulate OneDrive mount: mount signal to observer and ability to return
      // an email address.
      const email = 'email@gmail.com';
      testOneDriveBrowserProxy.handler.setResultFor(
          'getUserEmailAddress', {email});
      testOneDriveBrowserProxy.observerRemote.onODFSMountOrUnmount();
      await flushTasks();
      assertEquals(`Signed in as ${email}`, oneDriveRow.subLabel);
    });

    test('OneDrive row removes email address on OneDrive unmount', async () => {
      const email = 'email@gmail.com';
      setupBrowserProxy({email});
      await createFilesSettingsCard();

      const oneDriveRow =
          filesSettingsCard.shadowRoot!.querySelector<CrLinkRowElement>(
              '#oneDriveRow');
      assert(oneDriveRow);
      assertEquals(`Signed in as ${email}`, oneDriveRow.subLabel);

      // Simulate OneDrive unmount: unmount signal and returns an empty email
      // address.
      testOneDriveBrowserProxy.handler.setResultFor(
          'getUserEmailAddress', {email: null});
      testOneDriveBrowserProxy.observerRemote.onODFSMountOrUnmount();
      await flushTasks();
      assertEquals('Add your Microsoft account', oneDriveRow.subLabel);
    });

    test('OneDrive row is focused when returning from subpage', async () => {
      setupBrowserProxy({email: null});
      await createFilesSettingsCard();

      await assertSubpageTriggerFocused('#oneDriveRow', routes.ONE_DRIVE);
    });
  });

  suite('with showOfficeSettings set to true', () => {
    let testOneDriveBrowserProxy: OneDriveTestBrowserProxy;

    function setupBrowserProxy(options: ProxyOptions): void {
      testOneDriveBrowserProxy = new OneDriveTestBrowserProxy(options);
      OneDriveBrowserProxy.setInstance(testOneDriveBrowserProxy);
    }

    setup(() => {
      loadTimeData.overrideValues({showOfficeSettings: true});

      // Reinitialize Router and routes based on load time data
      const testRouter = createRouterForTesting();
      Router.resetInstanceForTesting(testRouter);
    });

    teardown(() => {
      testOneDriveBrowserProxy.handler.reset();
    });

    test('Clicking office row navigates to office route', async () => {
      setupBrowserProxy({email: null});
      await createFilesSettingsCard();

      const officeRow =
          filesSettingsCard.shadowRoot!.querySelector<HTMLElement>(
              '#officeRow');
      assert(officeRow);

      officeRow.click();
      flush();
      assertEquals(routes.OFFICE, Router.getInstance().currentRoute);
    });

    test('Office row is focused when returning from subpage', async () => {
      setupBrowserProxy({email: null});
      await createFilesSettingsCard();

      await assertSubpageTriggerFocused('#officeRow', routes.OFFICE);
    });
  });

  suite('with enableDriveFsBulkPinning set to true', () => {
    setup(async () => {
      loadTimeData.overrideValues({
        enableDriveFsBulkPinning: true,
      });
    });

    test(
        'with gdata.disabled set to true, text shows appropriately',
        async () => {
          await createFilesSettingsCard();
          filesSettingsCard.setPrefValue('gdata.disabled', true);
          flush();
          assertEquals('Not signed in', getGoogleDriveRowSubLabel().innerText);
        });

    test(
        'with gdata.disabled set to false, but file sync disabled',
        async () => {
          await createFilesSettingsCard();
          filesSettingsCard.setPrefValue('drivefs.bulk_pinning_enabled', false);
          flush();

          assertTrue(
              getGoogleDriveRowSubLabel().innerText.startsWith('Signed in as'));
        });

    test(
        'with gdata.disabled set to false, and file sync enabled', async () => {
          await createFilesSettingsCard();
          filesSettingsCard.setPrefValue('drivefs.bulk_pinning_enabled', true);
          flush();

          assertEquals('File sync on', getGoogleDriveRowSubLabel().innerText);
        });

    test('cycling through the prefs updates the sublabel texts', async () => {
      await createFilesSettingsCard();
      filesSettingsCard.setPrefValue('gdata.disabled', true);
      filesSettingsCard.setPrefValue('drivefs.bulk_pinning_enabled', false);
      flush();

      const googleDriveRowSubLabel = getGoogleDriveRowSubLabel();
      assertEquals('Not signed in', googleDriveRowSubLabel.innerText);

      filesSettingsCard.setPrefValue('gdata.disabled', false);
      flush();
      assertTrue(googleDriveRowSubLabel.innerText.startsWith('Signed in as'));

      filesSettingsCard.setPrefValue('drivefs.bulk_pinning_enabled', true);
      flush();
      assertEquals('File sync on', googleDriveRowSubLabel.innerText);
    });
  });

  suite('with enableDriveFsMirrorSync set to true', () => {
    setup(async () => {
      loadTimeData.overrideValues({
        enableDriveFsMirrorSync: true,
      });
    });

    test(
        'with gdata.disabled set to true, text shows appropriately',
        async () => {
          await createFilesSettingsCard();
          filesSettingsCard.setPrefValue('gdata.disabled', true);
          flush();
          assertEquals('Not signed in', getGoogleDriveRowSubLabel().innerText);
        });

    test(
        'with gdata.disabled set to false, but mirror sync disabled',
        async () => {
          await createFilesSettingsCard();
          filesSettingsCard.setPrefValue('drivefs.enable_mirror_sync', false);
          flush();

          assertTrue(
              getGoogleDriveRowSubLabel().innerText.startsWith('Signed in as'));
        });

    test(
        'with gdata.disabled set to false, and mirror sync enabled',
        async () => {
          await createFilesSettingsCard();
          filesSettingsCard.setPrefValue('drivefs.enable_mirror_sync', true);
          flush();

          assertEquals('File sync on', getGoogleDriveRowSubLabel().innerText);
        });

    test('cycling through the prefs updates the sublabel texts', async () => {
      await createFilesSettingsCard();
      filesSettingsCard.setPrefValue('gdata.disabled', true);
      filesSettingsCard.setPrefValue('drivefs.enable_mirror_sync', false);
      flush();

      const googleDriveRowSubLabel = getGoogleDriveRowSubLabel();
      assertEquals('Not signed in', googleDriveRowSubLabel.innerText);

      filesSettingsCard.setPrefValue('gdata.disabled', false);
      flush();
      assertTrue(googleDriveRowSubLabel.innerText.startsWith('Signed in as'));

      filesSettingsCard.setPrefValue('drivefs.enable_mirror_sync', true);
      flush();
      assertEquals('File sync on', googleDriveRowSubLabel.innerText);
    });
  });

  if (isRevampWayfindingEnabled) {
    suite('when no share has been setup before', () => {
      setup(async () => {
        smbBrowserProxy.anySmbMounted = false;
      });

      test('File shares row is not visible', async () => {
        await createFilesSettingsCard();
        await smbBrowserProxy.whenCalled('hasAnySmbMountedBefore');

        const smbSharesLinkRow =
            filesSettingsCard.shadowRoot!.querySelector('#smbSharesRow');
        assertFalse(isVisible(smbSharesLinkRow));
      });

      test('Add file shares row is visible', async () => {
        await createFilesSettingsCard();
        await smbBrowserProxy.whenCalled('hasAnySmbMountedBefore');

        const addSmbSharesRow =
            filesSettingsCard.shadowRoot!.querySelector('#addSmbSharesRow');
        assertTrue(isVisible(addSmbSharesRow));
      });
    });

    suite('when file shares have been setup before', () => {
      setup(async () => {
        smbBrowserProxy.anySmbMounted = true;
      });

      test('File shares row is visible', async () => {
        await createFilesSettingsCard();
        await smbBrowserProxy.whenCalled('hasAnySmbMountedBefore');

        const smbSharesLinkRow =
            filesSettingsCard.shadowRoot!.querySelector('#smbSharesRow');
        assertTrue(isVisible(smbSharesLinkRow));
      });

      test('Add file shares row is not visible', async () => {
        await createFilesSettingsCard();
        await smbBrowserProxy.whenCalled('hasAnySmbMountedBefore');

        const addSmbSharesRow =
            filesSettingsCard.shadowRoot!.querySelector('#addSmbSharesRow');
        assertFalse(isVisible(addSmbSharesRow));
      });

      test(
          'File shares row is focused when returning from subpage',
          async () => {
            await createFilesSettingsCard();

            await assertSubpageTriggerFocused(
                '#smbSharesRow', routes.SMB_SHARES);
          });
    });
  } else {
    test('File shares row is visible', async () => {
      await createFilesSettingsCard();
      const smbSharesLinkRow =
          filesSettingsCard.shadowRoot!.querySelector('#smbSharesRow');
      assertTrue(isVisible(smbSharesLinkRow));
    });

    test('Add file shares row is not visible', async () => {
      await createFilesSettingsCard();
      const addSmbSharesRow =
          filesSettingsCard.shadowRoot!.querySelector('#addSmbSharesRow');
      assertFalse(isVisible(addSmbSharesRow));
    });

    test('File shares row is focused when returning from subpage', async () => {
      await createFilesSettingsCard();

      await assertSubpageTriggerFocused('#smbSharesRow', routes.SMB_SHARES);
    });
  }
});
