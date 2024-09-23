// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {DevicePageBrowserProxyImpl, fakeGraphicsTablets, fakeGraphicsTablets2, GraphicsTablet, Router, routes, SettingsGraphicsTabletSubpageElement} from 'chrome://os-settings/os_settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

import {getFakePrefs} from './device_page_test_util.js';
import {TestDevicePageBrowserProxy} from './test_device_page_browser_proxy.js';

suite('<settings-graphics-tablet-subpage>', () => {
  let graphicsTabletPage: SettingsGraphicsTabletSubpageElement;
  let browserProxy: TestDevicePageBrowserProxy;

  setup(async () => {
    browserProxy = new TestDevicePageBrowserProxy();
    DevicePageBrowserProxyImpl.setInstanceForTesting(browserProxy);

    Router.getInstance().navigateTo(routes.GRAPHICS_TABLET);
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
    clearBody();
  });

  async function initializeGraphicsTablet(
      fakeGraphicsTablets: GraphicsTablet[]) {
    graphicsTabletPage =
        document.createElement('settings-graphics-tablet-subpage');
    graphicsTabletPage.graphicsTablets = fakeGraphicsTablets;
    graphicsTabletPage.prefs = getFakePrefs();
    document.body.appendChild(graphicsTabletPage);
    return flushTasks();
  }

  test(
      'hide customize tablet buttons row for device has metadata but no tablet buttons',
      async () => {
        await initializeGraphicsTablet(fakeGraphicsTablets2);
        assertEquals(routes.GRAPHICS_TABLET, Router.getInstance().currentRoute);

        const customizeTabletButtons =
            graphicsTabletPage.shadowRoot!.querySelector<HTMLButtonElement>(
                '#customizeTabletButtons');
        const customizePenButtons =
            graphicsTabletPage.shadowRoot!.querySelector<HTMLButtonElement>(
                '#customizePenButtons');
        assertFalse(isVisible(customizeTabletButtons));
        assertTrue(isVisible(customizePenButtons));
      });

  test('graphics tablet subpage visibility', async () => {
    await initializeGraphicsTablet(fakeGraphicsTablets);
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
