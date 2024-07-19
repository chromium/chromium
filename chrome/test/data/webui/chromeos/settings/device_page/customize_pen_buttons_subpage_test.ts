// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SettingsCustomizePenButtonsSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {fakeGraphicsTabletButtonActions, fakeGraphicsTablets, FakeInputDeviceSettingsProvider, getInputDeviceSettingsProvider, GraphicsTablet, Router, routes, setupFakeInputDeviceSettingsProvider} from 'chrome://os-settings/os_settings.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<settings-customize-pen-buttons-subpage>', () => {
  let page: SettingsCustomizePenButtonsSubpageElement;
  let provider: FakeInputDeviceSettingsProvider;

  setup(async () => {
    setupFakeInputDeviceSettingsProvider();
    provider =
        getInputDeviceSettingsProvider() as FakeInputDeviceSettingsProvider;

    page = document.createElement('settings-customize-pen-buttons-subpage');
    page.graphicsTablets = fakeGraphicsTablets;
    // Set the current route with graphicsTabletId as search param and notify
    // the observer to update graphics tablet settings.
    const url = new URLSearchParams(
        {'graphicsTabletId': encodeURIComponent(fakeGraphicsTablets[0]!.id)});
    await Router.getInstance().setCurrentRoute(
        routes.CUSTOMIZE_PEN_BUTTONS,
        /* dynamicParams= */ url, /* removeSearch= */ true);

    document.body.appendChild(page);
    return flushTasks();
  });

  teardown(() => {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test(
      'navigate to graphics tablet subpage when only current device detached',
      async () => {
        assertEquals(
            Router.getInstance().currentRoute, routes.CUSTOMIZE_PEN_BUTTONS);
        const graphicsTablet: GraphicsTablet = page.selectedTablet;
        assertTrue(!!graphicsTablet);
        assertEquals(graphicsTablet.id, fakeGraphicsTablets[0]!.id);
        // Remove fakeGraphicsTablets[0] from the graphics tablet list.
        page.graphicsTablets = [fakeGraphicsTablets[1]!];
        await flushTasks();
        assertEquals(Router.getInstance().currentRoute, routes.GRAPHICS_TABLET);
      });

  test(
      'navigate to device page when all graphics tablets detached',
      async () => {
        assertEquals(
            Router.getInstance().currentRoute, routes.CUSTOMIZE_PEN_BUTTONS);
        const graphicsTablet: GraphicsTablet = page.selectedTablet;
        assertTrue(!!graphicsTablet);
        assertEquals(graphicsTablet.id, fakeGraphicsTablets[0]!.id);
        // Remove all graphics tablets from the graphics tablet list.
        page.graphicsTablets = [];
        await flushTasks();
        assertEquals(Router.getInstance().currentRoute, routes.DEVICE);
      });

  test('button action list fetched from provider', async () => {
    const observed_devices: number[] = provider.getObservedDevices();
    assertEquals(1, observed_devices.length);

    const graphicsTablet: GraphicsTablet = page.selectedTablet;
    assertTrue(!!graphicsTablet);
    assertEquals(graphicsTablet.id, fakeGraphicsTablets[0]!.id);

    const buttonActionList = page.get('buttonActionList_');
    const expectedActionList = fakeGraphicsTabletButtonActions;
    assertDeepEquals(buttonActionList, expectedActionList);
  });

  test('getMetaKeyToDisplay fetched from provider', async () => {
    const expectedMetaKey = (await provider.getMetaKeyToDisplay())?.metaKey;
    assertEquals(page.get('metaKey_'), expectedMetaKey);
  });

  test('button name change triggers settings update', async () => {
    const provider = page.get('inputDeviceSettingsProvider_');
    assertTrue(!!provider);
    assertEquals(provider.getSetGraphicsTabletSettingsCallCount(), 0);
    const buttonName =
        page!.selectedTablet!.settings!.penButtonRemappings[0]!.name;
    assertEquals(buttonName, 'Undo');
    page.set(
        `selectedTablet.settings.penButtonRemappings.0.name`,
        'new button name');
    await flushTasks();
    assertEquals(provider.getSetGraphicsTabletSettingsCallCount(), 0);
    page.dispatchEvent(new CustomEvent('button-remapping-changed', {
      bubbles: true,
      composed: true,
    }));
    await flushTasks();
    assertEquals(provider.getSetGraphicsTabletSettingsCallCount(), 1);
  });

  test('starts observing buttons when opened', async () => {
    let observed_devices: number[] = provider.getObservedDevices();
    assertEquals(1, observed_devices.length);
    assertEquals(fakeGraphicsTablets[0]!.id, observed_devices[0]);

    await Router.getInstance().navigateTo(routes.DEVICE);
    observed_devices = provider.getObservedDevices();
    assertEquals(0, observed_devices.length);
  });

  test(
      'verify pen button nudge header with metadata or no metadata',
      async () => {
        // On the first pen subpage without metadata.
        assertEquals(
            Router.getInstance().currentRoute, routes.CUSTOMIZE_PEN_BUTTONS);
        assertEquals(
            'Add or locate buttons on your pen',
            page.shadowRoot!.querySelector<HTMLDivElement>(
                                '.help-title')!.textContent!.trim());
        // Go to the second pen subpage with metadata.
        const url = new URLSearchParams({
          'graphicsTabletId': encodeURIComponent(fakeGraphicsTablets[1]!.id),
        });
        await Router.getInstance().setCurrentRoute(
            routes.CUSTOMIZE_PEN_BUTTONS,
            /* dynamicParams= */ url, /* removeSearch= */ true);
        await flushTasks();
        assertEquals(
            Router.getInstance().currentRoute, routes.CUSTOMIZE_PEN_BUTTONS);
        assertEquals(
            'Locate buttons on your pen',
            page.shadowRoot!.querySelector<HTMLDivElement>(
                                '.help-title')!.textContent!.trim());
      });
});
