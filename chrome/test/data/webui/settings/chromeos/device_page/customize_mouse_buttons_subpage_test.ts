// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsCustomizeMouseButtonsSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {FakeInputDeviceSettingsProvider, fakeMice, fakeMouseButtonActions, getInputDeviceSettingsProvider, Mouse, Router, routes, setupFakeInputDeviceSettingsProvider} from 'chrome://os-settings/os_settings.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<settings-customize-mouse-buttons-subpage>', () => {
  let page: SettingsCustomizeMouseButtonsSubpageElement;
  let provider: FakeInputDeviceSettingsProvider;

  setup(async () => {
    setupFakeInputDeviceSettingsProvider();
    provider =
        getInputDeviceSettingsProvider() as FakeInputDeviceSettingsProvider;

    page = document.createElement('settings-customize-mouse-buttons-subpage');
    page.mouseList = fakeMice;
    // Set the current route with mouseId as search param and notify
    // the observer to update mouse settings.
    const url =
        new URLSearchParams('mouseId=' + encodeURIComponent(fakeMice[0]!.id));
    await Router.getInstance().setCurrentRoute(
        routes.CUSTOMIZE_MOUSE_BUTTONS,
        /* dynamicParams= */ url, /* removeSearch= */ true);

    document.body.appendChild(page);
    return flushTasks();
  });

  teardown(() => {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('navigate to device page when mouse detached', async () => {
    assertEquals(
        Router.getInstance().currentRoute, routes.CUSTOMIZE_MOUSE_BUTTONS);
    const mouse: Mouse = page.selectedMouse;
    assertTrue(!!mouse);
    assertEquals(mouse.id, fakeMice[0]!.id);
    // Remove fakeMice[0] from the mouse list.
    page.mouseList = [fakeMice[1]!];
    await flushTasks();
    assertEquals(Router.getInstance().currentRoute, routes.DEVICE);
  });

  test('button action list fetched from provider', async () => {
    const mouse: Mouse = page.selectedMouse;
    assertTrue(!!mouse);
    assertEquals(mouse.id, fakeMice[0]!.id);

    const buttonActionList = page.get('buttonActionList_');
    const expectedActionList = fakeMouseButtonActions;
    assertDeepEquals(buttonActionList, expectedActionList);
  });


  test('button name change triggers settings update', async () => {
    const provider = page.get('inputDeviceSettingsProvider_');
    assertTrue(!!provider);
    assertEquals(provider.getSetMouseSettingsCallCount(), 0);
    const buttonName = page!.selectedMouse!.settings!.buttonRemappings[0]!.name;
    assertEquals(buttonName, 'Back Button');
    page.set(
        `selectedMouse.settings.buttonRemappings.0.name`, 'new button name');
    await flushTasks();
    assertEquals(provider.getSetMouseSettingsCallCount(), 0);
    page.dispatchEvent(new CustomEvent('button-remapping-changed', {
      bubbles: true,
      composed: true,
    }));
    await flushTasks();
    assertEquals(provider.getSetMouseSettingsCallCount(), 1);
  });

  test('starts observing buttons when opened', async () => {
    let observed_devices: number[] = provider.getObservedDevices();
    assertEquals(1, observed_devices.length);
    assertEquals(fakeMice[0]!.id, observed_devices[0]);

    await Router.getInstance().navigateTo(routes.DEVICE);
    observed_devices = provider.getObservedDevices();
    assertEquals(0, observed_devices.length);
  });
});
