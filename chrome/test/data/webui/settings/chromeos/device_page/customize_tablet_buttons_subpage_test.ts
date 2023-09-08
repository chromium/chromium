// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SettingsCustomizeTabletButtonsSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {fakeGraphicsTabletButtonActions, fakeGraphicsTablets, FakeInputDeviceSettingsProvider, getInputDeviceSettingsProvider, GraphicsTablet, Router, routes, setupFakeInputDeviceSettingsProvider} from 'chrome://os-settings/os_settings.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<settings-customize-tablet-buttons-subpage>', () => {
  let page: SettingsCustomizeTabletButtonsSubpageElement;
  let provider: FakeInputDeviceSettingsProvider;

  setup(async () => {
    setupFakeInputDeviceSettingsProvider();
    provider =
        getInputDeviceSettingsProvider() as FakeInputDeviceSettingsProvider;

    page = document.createElement('settings-customize-tablet-buttons-subpage');
    page.graphicsTablets = fakeGraphicsTablets;
    // Set the current route with mouseId as search param and notify
    // the observer to update mouse settings.
    const url = new URLSearchParams(
        {'graphicsTabletId': encodeURIComponent(fakeGraphicsTablets[0]!.id)});
    await Router.getInstance().setCurrentRoute(
        routes.CUSTOMIZE_TABLET_BUTTONS,
        /* dynamicParams= */ url, /* removeSearch= */ true);

    document.body.appendChild(page);
    return flushTasks();
  });

  teardown(() => {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('navigate to device page when graphics tablet detached', async () => {
    assertEquals(
        Router.getInstance().currentRoute, routes.CUSTOMIZE_TABLET_BUTTONS);
    const graphicsTablet: GraphicsTablet = page.selectedTablet;
    assertTrue(!!graphicsTablet);
    assertEquals(graphicsTablet.id, fakeGraphicsTablets[0]!.id);
    // Remove fakeMice[0] from the mouse list.
    page.graphicsTablets = [fakeGraphicsTablets[1]!];
    await flushTasks();
    assertEquals(Router.getInstance().currentRoute, routes.DEVICE);
  });

  test('button action list fetched from provider', async () => {
    const graphicsTablet: GraphicsTablet = page.selectedTablet;
    assertTrue(!!graphicsTablet);
    assertEquals(graphicsTablet.id, fakeGraphicsTablets[0]!.id);

    const buttonActionList = page.get('buttonActionList_');
    const expectedActionList = fakeGraphicsTabletButtonActions;
    assertDeepEquals(buttonActionList, expectedActionList);
  });

  test('button name change triggers settings update', async () => {
    const provider = page.get('inputDeviceSettingsProvider_');
    assertTrue(!!provider);
    assertEquals(provider.getSetGraphicsTabletSettingsCallCount(), 0);
    const buttonName =
        page!.selectedTablet!.settings!.tabletButtonRemappings[0]!.name;
    assertEquals(buttonName, 'Back Button');
    page.set(
        `selectedTablet.settings.tabletButtonRemappings.0.name`,
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
});
