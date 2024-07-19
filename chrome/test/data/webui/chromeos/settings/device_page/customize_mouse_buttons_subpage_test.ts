// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsCustomizeMouseButtonsSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {CrToggleElement, FakeInputDeviceSettingsProvider, fakeMice, fakeMouseButtonActions, getInputDeviceSettingsProvider, Mouse, PolicyStatus, Router, routes, setupFakeInputDeviceSettingsProvider} from 'chrome://os-settings/os_settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

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

  test(
      'navigate to per device mouse subpage when only current mouse detached',
      async () => {
        assertEquals(
            Router.getInstance().currentRoute, routes.CUSTOMIZE_MOUSE_BUTTONS);
        const mouse: Mouse = page.selectedMouse;
        assertTrue(!!mouse);
        assertEquals(mouse.id, fakeMice[0]!.id);
        // Remove fakeMice[0] from the mouse list.
        page.mouseList = [fakeMice[1]!];
        await flushTasks();
        assertEquals(
            Router.getInstance().currentRoute, routes.PER_DEVICE_MOUSE);
      });

  test('navigate to device page when all mouse detached', async () => {
    assertEquals(
        Router.getInstance().currentRoute, routes.CUSTOMIZE_MOUSE_BUTTONS);
    const mouse: Mouse = page.selectedMouse;
    assertTrue(!!mouse);
    assertEquals(mouse.id, fakeMice[0]!.id);
    // Remove all mice from the mouse list.
    page.mouseList = [];
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

  test('getMetaKeyToDisplay fetched from provider', async () => {
    const expectedMetaKey = (await provider.getMetaKeyToDisplay())?.metaKey;
    assertEquals(page.get('metaKey_'), expectedMetaKey);
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

  /**
   * Test that API are updated when mouse settings change.
   */
  test('Update API when mouse settings change', async () => {
    const mouseSwapToggleButton =
        page.shadowRoot!.querySelector<CrToggleElement>(
            '#mouseSwapToggleButton');
    assertTrue(!!mouseSwapToggleButton);
    assertEquals(
        fakeMice[0]!.settings.swapRight, mouseSwapToggleButton.checked);
    mouseSwapToggleButton.click();
    await flushTasks();
    const updatedMice = await provider.getConnectedMouseSettings();
    assertEquals(
        updatedMice[0]!.settings.swapRight, mouseSwapToggleButton.checked);
  });

  /**
   * Verifies that the policy indicator is properly reflected in the UI.
   */
  test('swap right policy reflected in UI', async () => {
    page.set('mousePolicies', {
      swapRightPolicy: {policy_status: PolicyStatus.kManaged, value: false},
    });
    await flushTasks();
    const mouseSwapToggleButton =
        page.shadowRoot!.querySelector('#mouseSwapToggleButton');
    assertTrue(!!mouseSwapToggleButton);
    let policyIndicator = mouseSwapToggleButton.shadowRoot!.querySelector(
        'cr-policy-pref-indicator');
    assertTrue(isVisible(policyIndicator));

    page.set('mousePolicies', {swapRightPolicy: undefined});
    await flushTasks();
    policyIndicator = mouseSwapToggleButton.shadowRoot!.querySelector(
        'cr-policy-pref-indicator');
    assertFalse(isVisible(policyIndicator));
  });

  test(
      'verify mouse button nudge header with metadata or no metadata',
      async () => {
        // On the first mouse subpage without metadata.
        assertEquals(
            Router.getInstance().currentRoute, routes.CUSTOMIZE_MOUSE_BUTTONS);
        assertEquals(
            'Add or locate buttons on your mouse',
            page.shadowRoot!.querySelector<HTMLDivElement>(
                                '.help-title')!.textContent!.trim());
        // Go to the second mouse subpage with metadata.
        const url = new URLSearchParams({
          'mouseId': encodeURIComponent(fakeMice[1]!.id),
        });
        await Router.getInstance().setCurrentRoute(
            routes.CUSTOMIZE_MOUSE_BUTTONS,
            /* dynamicParams= */ url, /* removeSearch= */ true);
        await flushTasks();
        assertEquals(
            Router.getInstance().currentRoute, routes.CUSTOMIZE_MOUSE_BUTTONS);
        assertEquals(
            'Locate buttons on your mouse',
            page.shadowRoot!.querySelector<HTMLDivElement>(
                                '.help-title')!.textContent!.trim());
      });
});
