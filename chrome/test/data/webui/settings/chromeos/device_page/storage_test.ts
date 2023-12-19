// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsStorageElement} from 'chrome://os-settings/lazy_load.js';
import {DevicePageBrowserProxyImpl, Route, Router, routes, setDisplayApiForTesting, SettingsDevicePageElement, StorageSpaceState} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {disableAnimationsAndTransitions} from 'chrome://webui-test/test_api.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {FakeSystemDisplay} from '../fake_system_display.js';

import {getFakePrefs} from './device_page_test_util.js';
import {TestDevicePageBrowserProxy} from './test_device_page_browser_proxy.js';

suite('<settings-storage> for device page', () => {
  let devicePage: SettingsDevicePageElement;
  let fakeSystemDisplay: FakeSystemDisplay;
  let browserProxy: TestDevicePageBrowserProxy;

  suiteSetup(() => {
    // Disable animations so sub-pages open within one event loop.
    disableAnimationsAndTransitions();
  });

  /**
   * Set enableInputDeviceSettingsSplit feature flag to true for split tests.
   */
  function setDeviceSplitEnabled(isEnabled: boolean): void {
    loadTimeData.overrideValues({
      enableInputDeviceSettingsSplit: isEnabled,
    });
  }

  setup(async () => {
    fakeSystemDisplay = new FakeSystemDisplay();
    setDisplayApiForTesting(fakeSystemDisplay);

    Router.getInstance().navigateTo(routes.BASIC);

    browserProxy = new TestDevicePageBrowserProxy();
    DevicePageBrowserProxyImpl.setInstanceForTesting(browserProxy);
    setDeviceSplitEnabled(true);
    // Allow the light DOM to be distributed to os-settings-animated-pages.
    await flushTasks();
  });

  teardown(() => {
    devicePage.remove();
    browserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  async function init(): Promise<void> {
    devicePage = document.createElement('settings-device-page');
    devicePage.prefs = getFakePrefs();
    document.body.appendChild(devicePage);
    flush();
  }

  function showAndGetDeviceSubpage(
      subpage: string, expectedRoute: Route): HTMLElement {
    const row = devicePage.shadowRoot!.querySelector<HTMLButtonElement>(
        `#main #${subpage}Row`);
    assertTrue(!!row);
    row.click();
    assertEquals(expectedRoute, Router.getInstance().currentRoute);
    const page = devicePage.shadowRoot!.querySelector<HTMLElement>(
        'settings-' + subpage);
    assertTrue(!!page);
    return page;
  }

  suite('storage', () => {
    let storagePage: SettingsStorageElement;

    /**
     * Simulate storage size stat callback.
     */
    function sendStorageSizeStat(
        usedSize: string, availableSize: string, usedRatio: number,
        spaceState: number): void {
      webUIListenerCallback('storage-size-stat-changed', {
        usedSize: usedSize,
        availableSize: availableSize,
        usedRatio: usedRatio,
        spaceState: spaceState,
      });
      flush();
    }

    function getStorageItemLabelFromId(id: string): string {
      const rowItem = storagePage.shadowRoot!.querySelector('#' + id);
      assertTrue(!!rowItem);
      const label = rowItem.shadowRoot!.querySelector<HTMLElement>('#label');
      assertTrue(!!label);
      return label.innerText;
    }

    function getStorageItemSubLabelFromId(id: string): string {
      const rowItem = storagePage.shadowRoot!.querySelector('#' + id);
      assertTrue(!!rowItem);
      const subLabel =
          rowItem.shadowRoot!.querySelector<HTMLElement>('#subLabel');
      assertTrue(!!subLabel);
      return subLabel.innerText;
    }

    async function setupPage(): Promise<void> {
      await init();
      storagePage = showAndGetDeviceSubpage('storage', routes.STORAGE) as
          SettingsStorageElement;
      storagePage['stopPeriodicUpdate_']();
    }

    setup(async () => {
      await setupPage();
    });

    teardown(() => {
      storagePage.remove();
    });

    test('storage stats size', async () => {
      // Low available storage space.
      sendStorageSizeStat('9.1 GB', '0.9 GB', 0.91, StorageSpaceState.LOW);
      assertEquals('91%', storagePage.$.inUseLabelArea.style.width);
      assertEquals('9%', storagePage.$.availableLabelArea.style.width);
      assertTrue(
          isVisible(storagePage.shadowRoot!.querySelector('#lowMessage')));
      assertFalse(isVisible(
          storagePage.shadowRoot!.querySelector('#criticallyLowMessage')));
      assertTrue(!!storagePage.shadowRoot!.querySelector('#bar.space-low'));
      assertNull(
          storagePage.shadowRoot!.querySelector('#bar.space-critically-low'));

      let inUseArea = storagePage.$.inUseLabelArea.querySelector<HTMLElement>(
          '.storage-size');
      assertTrue(!!inUseArea);
      assertEquals('9.1 GB', inUseArea.innerText);

      let availableArea =
          storagePage.$.availableLabelArea.querySelector<HTMLElement>(
              '.storage-size');
      assertTrue(!!availableArea);
      assertEquals('0.9 GB', availableArea.innerText);

      // Critically low available storage space.
      sendStorageSizeStat(
          '9.7 GB', '0.3 GB', 0.97, StorageSpaceState.CRITICALLY_LOW);
      assertEquals('97%', storagePage.$.inUseLabelArea.style.width);
      assertEquals('3%', storagePage.$.availableLabelArea.style.width);
      assertFalse(
          isVisible(storagePage.shadowRoot!.querySelector('#lowMessage')));
      assertTrue(isVisible(
          storagePage.shadowRoot!.querySelector('#criticallyLowMessage')));
      assertNull(storagePage.shadowRoot!.querySelector('#bar.space-low'));
      assertTrue(
          !!storagePage.shadowRoot!.querySelector('#bar.space-critically-low'));

      inUseArea = storagePage.$.inUseLabelArea.querySelector<HTMLElement>(
          '.storage-size');
      assertTrue(!!inUseArea);
      assertEquals('9.7 GB', inUseArea.innerText);

      availableArea =
          storagePage.$.availableLabelArea.querySelector<HTMLElement>(
              '.storage-size');
      assertTrue(!!availableArea);
      assertEquals('0.3 GB', availableArea.innerText);

      // Normal storage usage.
      sendStorageSizeStat('2.5 GB', '7.5 GB', 0.25, StorageSpaceState.NORMAL);
      assertEquals('25%', storagePage.$.inUseLabelArea.style.width);
      assertEquals('75%', storagePage.$.availableLabelArea.style.width);
      assertFalse(
          isVisible(storagePage.shadowRoot!.querySelector('#lowMessage')));
      assertFalse(isVisible(
          storagePage.shadowRoot!.querySelector('#criticallyLowMessage')));
      assertNull(storagePage.shadowRoot!.querySelector('#bar.space-low'));
      assertNull(
          storagePage.shadowRoot!.querySelector('#bar.space-critically-low'));

      inUseArea = storagePage.$.inUseLabelArea.querySelector<HTMLElement>(
          '.storage-size');
      assertTrue(!!inUseArea);
      assertEquals('2.5 GB', inUseArea.innerText);

      availableArea =
          storagePage.$.availableLabelArea.querySelector<HTMLElement>(
              '.storage-size');
      assertTrue(!!availableArea);
      assertEquals('7.5 GB', availableArea.innerText);
    });

    test('system size', async () => {
      const systemSizeLabel =
          storagePage.shadowRoot!.querySelector<HTMLElement>(
              '#systemSizeLabel');
      assertTrue(!!systemSizeLabel);
      assertEquals('System', systemSizeLabel.innerText);

      let systemSizeSubLabel =
          storagePage.shadowRoot!.querySelector<HTMLElement>(
              '#systemSizeSubLabel');
      assertTrue(!!systemSizeSubLabel);
      assertEquals('Calculating…', systemSizeSubLabel.innerText);

      // Send system size callback.
      webUIListenerCallback('storage-system-size-changed', '8.4 GB');
      flush();
      systemSizeSubLabel = storagePage.shadowRoot!.querySelector<HTMLElement>(
          '#systemSizeSubLabel');
      assertTrue(!!systemSizeSubLabel);
      assertEquals('8.4 GB', systemSizeSubLabel.innerText);

      // In guest mode, the system row should be hidden.
      storagePage.set('isEphemeralUser_', true);
      flush();
      assertFalse(
          isVisible(storagePage.shadowRoot!.querySelector('#systemSize')));
    });

    test('apps extensions size', () => {
      assertEquals(
          'Apps and extensions', getStorageItemLabelFromId('appsSize'));
      assertEquals('Calculating…', getStorageItemSubLabelFromId('appsSize'));

      // Send apps size callback.
      webUIListenerCallback('storage-apps-size-changed', '59.5 KB');
      flush();
      assertEquals('59.5 KB', getStorageItemSubLabelFromId('appsSize'));
    });

    test('other users size', () => {
      // The other users row is visible by default, displaying
      // "calculating...".
      assertTrue(
          isVisible(storagePage.shadowRoot!.querySelector('#otherUsersSize')));
      assertEquals('Other users', getStorageItemLabelFromId('otherUsersSize'));
      assertEquals(
          'Calculating…', getStorageItemSubLabelFromId('otherUsersSize'));

      // Simulate absence of other users.
      webUIListenerCallback('storage-other-users-size-changed', '0 B', true);
      flush();
      assertFalse(
          isVisible(storagePage.shadowRoot!.querySelector('#otherUsersSize')));

      // Send other users callback with a size that is not null.
      webUIListenerCallback(
          'storage-other-users-size-changed', '322 MB', false);
      flush();
      assertTrue(
          isVisible(storagePage.shadowRoot!.querySelector('#otherUsersSize')));
      assertEquals('322 MB', getStorageItemSubLabelFromId('otherUsersSize'));

      // If the user is in Guest mode, the row is not visible.
      storagePage.set('isEphemeralUser_', true);
      webUIListenerCallback(
          'storage-other-users-size-changed', '322 MB', false);
      flush();
      assertFalse(
          isVisible(storagePage.shadowRoot!.querySelector('#otherUsersSize')));
    });

    test('drive offline size', async () => {
      interface Params {
        enableDriveFsBulkPinning: boolean;
        showGoogleDriveSettingsPage: boolean;
        isDriveEnabled: boolean;
        isVisible: boolean;
      }

      async function assertDriveOfflineSizeVisibility(params: Params):
          Promise<void> {
        loadTimeData.overrideValues({
          enableDriveFsBulkPinning: params.enableDriveFsBulkPinning,
          showGoogleDriveSettingsPage: params.showGoogleDriveSettingsPage,
        });
        await setupPage();
        devicePage.set('prefs.gdata.disabled.value', !params.isDriveEnabled);
        await flushTasks();
        const expectedState =
            (params.isVisible) ? 'be visible' : 'not be visible';
        assertEquals(
            params.isVisible,
            isVisible(
                storagePage.shadowRoot!.querySelector('#driveOfflineSize')),
            `Expected #driveOfflineSize to ${expectedState} with params: ${
                JSON.stringify(params)}`);
      }

      await assertDriveOfflineSizeVisibility({
        enableDriveFsBulkPinning: false,
        showGoogleDriveSettingsPage: false,
        isDriveEnabled: false,
        isVisible: false,
      });

      await assertDriveOfflineSizeVisibility({
        enableDriveFsBulkPinning: false,
        showGoogleDriveSettingsPage: false,
        isDriveEnabled: true,
        isVisible: false,
      });

      await assertDriveOfflineSizeVisibility({
        enableDriveFsBulkPinning: false,
        showGoogleDriveSettingsPage: true,
        isDriveEnabled: false,
        isVisible: false,
      });

      await assertDriveOfflineSizeVisibility({
        enableDriveFsBulkPinning: true,
        showGoogleDriveSettingsPage: false,
        isDriveEnabled: false,
        isVisible: false,
      });

      await assertDriveOfflineSizeVisibility({
        enableDriveFsBulkPinning: true,
        showGoogleDriveSettingsPage: true,
        isDriveEnabled: false,
        isVisible: false,
      });

      await assertDriveOfflineSizeVisibility({
        enableDriveFsBulkPinning: false,
        showGoogleDriveSettingsPage: true,
        isDriveEnabled: true,
        isVisible: true,
      });

      await assertDriveOfflineSizeVisibility({
        enableDriveFsBulkPinning: true,
        showGoogleDriveSettingsPage: false,
        isDriveEnabled: true,
        isVisible: true,
      });

      await assertDriveOfflineSizeVisibility({
        enableDriveFsBulkPinning: true,
        showGoogleDriveSettingsPage: true,
        isDriveEnabled: true,
        isVisible: true,
      });
    });
  });
});
