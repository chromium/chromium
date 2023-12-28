// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {DevicePageBrowserProxyImpl, fakeGraphicsTablets, FakeInputDeviceSettingsProvider, Router, routes, setDisplayApiForTesting, setInputDeviceSettingsProviderForTesting, SettingsDevicePageElement, SettingsGraphicsTabletSubpageElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {disableAnimationsAndTransitions} from 'chrome://webui-test/test_api.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {FakeSystemDisplay} from '../fake_system_display.js';

import {getFakePrefs} from './device_page_test_util.js';
import {TestDevicePageBrowserProxy} from './test_device_page_browser_proxy.js';

suite('<settings-graphics-tablet-subpage> for device page', () => {
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

  /**
   * Set enablePeripheralCustomization feature flag to true for split tests.
   */
  function setPeripheralCustomizationEnabled(isEnabled: boolean): void {
    loadTimeData.overrideValues({
      enablePeripheralCustomization: isEnabled,
    });
  }

  suite('graphics tablet', () => {
    let graphicsTabletPage: SettingsGraphicsTabletSubpageElement;
    let inputDeviceSettingsProvider: FakeInputDeviceSettingsProvider;

    suiteSetup(() => {
      inputDeviceSettingsProvider = new FakeInputDeviceSettingsProvider();
      inputDeviceSettingsProvider.setFakeGraphicsTablets(fakeGraphicsTablets);
      setInputDeviceSettingsProviderForTesting(inputDeviceSettingsProvider);
    });

    setup(async () => {
      setPeripheralCustomizationEnabled(true);
      await init();
      const row = devicePage.shadowRoot!.querySelector<HTMLButtonElement>(
          `#main #tabletRow`);
      assertTrue(!!row);
      row.click();
      assertEquals(routes.GRAPHICS_TABLET, Router.getInstance().currentRoute);
      const page = devicePage.shadowRoot!.querySelector(
          'settings-graphics-tablet-subpage');
      assert(page);
      const element = await Promise.resolve(page);
      assertTrue(!!element);
      graphicsTabletPage = element;
    });

    teardown(() => {
      graphicsTabletPage.remove();
    });

    test('graphics tablet subpage visibility', async () => {
      assertEquals(routes.GRAPHICS_TABLET, Router.getInstance().currentRoute);
      const items = graphicsTabletPage.shadowRoot!.querySelectorAll('.device');
      // Verify that all graphics tablets are displayed and their ids are same
      // with the data in the provider.
      assertEquals(fakeGraphicsTablets.length, items.length);
      assertTrue(isVisible(items[0]!));
      assertEquals(
          fakeGraphicsTablets[0]!.id,
          Number(items[0]!.getAttribute('data-evdev-id')));
      assertTrue(isVisible(items[1]!));
      assertEquals(
          fakeGraphicsTablets[1]!.id,
          Number(items[1]!.getAttribute('data-evdev-id')));

      // Verify that the customize-tablet-buttons and customize-pen-buttons
      // crLinkRow are visible.
      const customizeTabletButtons =
          graphicsTabletPage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#customizeTabletButtons');
      assertTrue(!!customizeTabletButtons);

      // Verify clicking the customize table buttons row will be redirecting
      // to the customize table buttons subpage.
      customizeTabletButtons.click();
      await flushTasks();
      assertEquals(
          routes.CUSTOMIZE_TABLET_BUTTONS, Router.getInstance().currentRoute);

      const urlSearchQuery =
          Router.getInstance().getQueryParameters().get('graphicsTabletId');
      assertTrue(!!urlSearchQuery);
      const graphicsTabletId = Number(urlSearchQuery);
      assertFalse(isNaN(graphicsTabletId));
      assertEquals(fakeGraphicsTablets[0]!.id, graphicsTabletId);

      // Verify clicking the customize pen buttons row will be redirected
      // to the customize table buttons subpage.
      Router.getInstance().navigateTo(routes.GRAPHICS_TABLET);
      assertEquals(routes.GRAPHICS_TABLET, Router.getInstance().currentRoute);

      const customizePenButtons =
          graphicsTabletPage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#customizePenButtons');
      assertTrue(!!customizePenButtons);
      customizePenButtons.click();
      await flushTasks();

      assertEquals(
          routes.CUSTOMIZE_PEN_BUTTONS, Router.getInstance().currentRoute);
      const graphicsTabletPenId = Number(
          Router.getInstance().getQueryParameters().get('graphicsTabletId'));
      assertFalse(isNaN(graphicsTabletPenId));
      assertEquals(fakeGraphicsTablets[0]!.id, graphicsTabletPenId);
    });
  });
});
