// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {DeviceTabVisibilityEntry, SettingsDeviceTabVisibilityPageElement} from 'chrome://settings/lazy_load.js';
import {DeviceTabVisibilityBrowserProxyImpl, SyncedDeviceFormFactor} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestDeviceTabVisibilityBrowserProxy} from './test_device_tab_visibility_browser_proxy.js';

function createDummyPhoneDevice(): DeviceTabVisibilityEntry {
  return {
    sessionTag: 'session_phone',
    deviceName: 'Pixel 9',
    lastActiveTime: 'Active now',
    formFactor: SyncedDeviceFormFactor.PHONE,
    visible: true,
  };
}

function createDummyTabletDevice(): DeviceTabVisibilityEntry {
  return {
    sessionTag: 'session_tablet',
    deviceName: 'Pixel Tablet',
    lastActiveTime: 'Active 14 minutes ago',
    formFactor: SyncedDeviceFormFactor.TABLET,
    visible: false,
  };
}

function createDummyDesktopDevice(): DeviceTabVisibilityEntry {
  return {
    sessionTag: 'session_desktop',
    deviceName: 'Macbook Pro',
    lastActiveTime: 'Active 2 days ago',
    formFactor: SyncedDeviceFormFactor.DESKTOP,
    visible: true,
  };
}

// Verifies the Device tab visibility subpage UI and toggle interactions.
suite('DeviceTabVisibilityPage', function() {
  let page: SettingsDeviceTabVisibilityPageElement;
  let browserProxy: TestDeviceTabVisibilityBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestDeviceTabVisibilityBrowserProxy();
    DeviceTabVisibilityBrowserProxyImpl.setInstance(browserProxy);
  });

  async function createPage(): Promise<void> {
    page = document.createElement('settings-device-tab-visibility-page');
    document.body.appendChild(page);
    await browserProxy.whenCalled('getDeviceTabVisibilityList');
    await microtasksFinished();
  }

  function assertDeviceRow(
      row: HTMLElement, expectedIcon: string, expectedName: string,
      expectedLastActiveTime: string, expectedChecked: boolean) {
    assertEquals(expectedIcon, row.querySelector('cr-icon')!.icon);
    assertEquals(
        expectedName, row.querySelector('.device-name')!.textContent.trim());
    const secondaryText = row.querySelector('.cr-secondary-text')!;
    assertEquals(expectedLastActiveTime, secondaryText.textContent.trim());
    const toggle = row.querySelector('cr-toggle');
    assertTrue(!!toggle);
    assertEquals(expectedChecked, toggle.checked);
    assertEquals(expectedName, toggle.getAttribute('aria-label'));
    assertEquals(secondaryText.id, toggle.getAttribute('aria-describedby'));
  }

  // Verifies that the empty state message and description link are displayed
  // when no synced foreign devices exist.
  test('ShowsEmptyStateWhenNoDevices', async function() {
    browserProxy.setDevices([]);
    await createPage();

    // The description with its external device-activity link is visible.
    assertTrue(isChildVisible(page, '#description'));
    const link = page.shadowRoot.querySelector('a');
    assertTrue(!!link);
    assertEquals('_blank', link.target);

    // The empty-state label is shown and no device rows are rendered.
    assertTrue(isChildVisible(page, '#noDevicesLabel'));
    const rows = page.shadowRoot.querySelectorAll('.device-row');
    assertEquals(0, rows.length);
  });

  // Verifies that foreign devices are rendered with their respective names,
  // form-factor icons, and initial toggle states.
  test('RendersDeviceListAndIcons', async function() {
    const phoneDevice = createDummyPhoneDevice();
    const tabletDevice = createDummyTabletDevice();
    const desktopDevice = createDummyDesktopDevice();
    browserProxy.setDevices([phoneDevice, tabletDevice, desktopDevice]);
    await createPage();

    // The empty-state label is hidden and each form factor maps to its
    // corresponding icon (`settings:smartphone`, `settings:tablet`,
    // `cr:computer`).
    assertFalse(isChildVisible(page, '#noDevicesLabel'));
    const rows = page.shadowRoot.querySelectorAll<HTMLElement>('.device-row');
    assertEquals(3, rows.length);
    assertDeviceRow(
        rows[0]!, 'settings:smartphone', phoneDevice.deviceName,
        phoneDevice.lastActiveTime, phoneDevice.visible);
    assertDeviceRow(
        rows[1]!, 'settings:tablet', tabletDevice.deviceName,
        tabletDevice.lastActiveTime, tabletDevice.visible);
    assertDeviceRow(
        rows[2]!, 'cr:computer', desktopDevice.deviceName,
        desktopDevice.lastActiveTime, desktopDevice.visible);
  });

  // Verifies that toggling a device row notifies the browser proxy with the
  // session tag and new visibility state.
  test('ToggleDeviceVisibilityCallsProxy', async function() {
    const phoneDevice = createDummyPhoneDevice();
    const tabletDevice = createDummyTabletDevice();
    browserProxy.setDevices([phoneDevice, tabletDevice]);
    await createPage();

    const toggles = page.shadowRoot.querySelectorAll('cr-toggle');
    assertEquals(2, toggles.length);
    const phoneToggle = toggles[0]!;
    const tabletToggle = toggles[1]!;

    // Clicking the visible phone toggle flips visibility to `false`.
    phoneToggle.click();
    let [sessionTag, visible] =
        await browserProxy.whenCalled('setDeviceTabVisibility');
    assertEquals(phoneDevice.sessionTag, sessionTag);
    assertFalse(visible);
    browserProxy.resetResolver('setDeviceTabVisibility');

    // Clicking the hidden tablet toggle flips visibility to `true`.
    tabletToggle.click();
    [sessionTag, visible] =
        await browserProxy.whenCalled('setDeviceTabVisibility');
    assertEquals(tabletDevice.sessionTag, sessionTag);
    assertTrue(visible);
  });

  // Verifies that WebUI listener updates dynamically refresh the rendered
  // device list across empty-to-populated, in-place visibility change, and
  // populated-to-empty transitions.
  test('UpdatesListOnWebUiListenerEvent', async function() {
    // Start with an empty device list.
    browserProxy.setDevices([]);
    await createPage();
    assertTrue(isChildVisible(page, '#noDevicesLabel'));

    // Simulate the backend broadcasting an updated device list via the WebUI
    // listener.
    webUIListenerCallback('device-tab-visibility-list-changed', [
      {
        sessionTag: 'session_desktop',
        deviceName: 'Chromebook',
        lastActiveTime: 'Active 1 hour ago',
        formFactor: SyncedDeviceFormFactor.DESKTOP,
        visible: false,
      },
    ]);
    await microtasksFinished();

    // The empty-state label hides and the new device row is rendered.
    assertFalse(isChildVisible(page, '#noDevicesLabel'));
    let rows = page.shadowRoot.querySelectorAll<HTMLElement>('.device-row');
    assertEquals(1, rows.length);
    assertDeviceRow(
        rows[0]!, 'cr:computer', 'Chromebook', 'Active 1 hour ago', false);

    // Simulate an in-place visibility and active-time update for the existing
    // device.
    webUIListenerCallback('device-tab-visibility-list-changed', [
      {
        sessionTag: 'session_desktop',
        deviceName: 'Chromebook',
        lastActiveTime: 'Active now',
        formFactor: SyncedDeviceFormFactor.DESKTOP,
        visible: true,
      },
    ]);
    await microtasksFinished();

    rows = page.shadowRoot.querySelectorAll<HTMLElement>('.device-row');
    assertEquals(1, rows.length);
    assertDeviceRow(rows[0]!, 'cr:computer', 'Chromebook', 'Active now', true);

    // Simulate all foreign sessions being removed, returning to the empty
    // state.
    webUIListenerCallback('device-tab-visibility-list-changed', []);
    await microtasksFinished();

    assertTrue(isChildVisible(page, '#noDevicesLabel'));
    assertEquals(0, page.shadowRoot.querySelectorAll('.device-row').length);
  });
});
