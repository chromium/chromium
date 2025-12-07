// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import type {SettingsCursorAndTouchpadPageElement} from 'chrome://os-settings/lazy_load.js';
import {DisableTouchpadMode} from 'chrome://os-settings/lazy_load.js';
import type {CrLinkRowElement, SettingsDropdownMenuElement, SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {createRouterForTesting, CrSettingsPrefs, DevicePageBrowserProxyImpl, Router, routes, settingMojom} from 'chrome://os-settings/os_settings.js';
import type {CrToggleElement} from 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import {assert, assertNotReachedCase} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender, waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestDevicePageBrowserProxy} from '../device_page/test_device_page_browser_proxy.js';
import {clearBody} from '../utils.js';

const DEFAULT_BLACK_CURSOR_COLOR = 0;
const RED_CURSOR_COLOR = 0xd93025;

/**
 * Possible control types for settings.
 */
enum ControlType {
  DROPDOWN = 'dropdown',
  TOGGLE = 'toggle',
}

suite('<settings-cursor-and-touchpad-page>', () => {
  let page: SettingsCursorAndTouchpadPageElement;
  let deviceBrowserProxy: TestDevicePageBrowserProxy;
  let prefElement: SettingsPrefsElement;
  const disableInternalTouchpadFeatureEnabled =
      loadTimeData.getBoolean('isAccessibilityDisableTouchpadEnabled');

  async function initPage() {
    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    page = document.createElement('settings-cursor-and-touchpad-page');
    page.prefs = prefElement.prefs;
    document.body.appendChild(page);
    flush();
  }

  setup(() => {
    deviceBrowserProxy = new TestDevicePageBrowserProxy();
    deviceBrowserProxy.hasMouse = true;
    deviceBrowserProxy.hasTouchpad = true;
    deviceBrowserProxy.hasPointingStick = false;
    DevicePageBrowserProxyImpl.setInstanceForTesting(deviceBrowserProxy);

    clearBody();
    Router.getInstance().navigateTo(routes.A11Y_CURSOR_AND_TOUCHPAD);
  });

  teardown(() => {
    page.remove();
    prefElement.remove();
    Router.getInstance().resetRouteForTesting();
  });

  function setUpDeviceBrowserProxy(
      hasMouse: boolean, hasTouchpad: boolean, hasPointingStick: boolean) {
    deviceBrowserProxy.hasMouse = hasMouse;
    deviceBrowserProxy.hasTouchpad = hasTouchpad;
    deviceBrowserProxy.hasPointingStick = hasPointingStick;
  }

  async function setUpNavigationTest(
      hasMouse: boolean, hasTouchpad: boolean,
      hasPointingStick: boolean): Promise<void> {
    const testRouter = createRouterForTesting();
    Router.resetInstanceForTesting(testRouter);
    await initPage();
    setUpDeviceBrowserProxy(hasMouse, hasTouchpad, hasPointingStick);
  }

  function getFaceGazePageRow(): CrLinkRowElement|null {
    return page.shadowRoot!.querySelector<CrLinkRowElement>('#faceGazePageRow');
  }

  function getDisableInternalTouchpadDropdown() {
    const disableInternalTouchpadDropdown =
        page.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
            '#disableInternalTouchpadDropdown');
    assert(disableInternalTouchpadDropdown);

    return disableInternalTouchpadDropdown;
  }

  async function getDisableInternalTouchpadSelectElement() {
    const disableInternalTouchpadDropdown =
        getDisableInternalTouchpadDropdown();
    await waitAfterNextRender(disableInternalTouchpadDropdown);
    const disableInternalTouchpadSelectElement =
        disableInternalTouchpadDropdown.shadowRoot!.querySelector('select');
    assert(disableInternalTouchpadSelectElement);
    return disableInternalTouchpadSelectElement;
  }

  test('cursor color prefs and dropdown synced', async () => {
    await initPage();

    // Make sure cursor color dropdown is black, matching default pref state.
    const cursorColorDropdown =
        page.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
            '#cursorColorDropdown');
    assert(cursorColorDropdown);
    await waitAfterNextRender(cursorColorDropdown);
    const cursorColorSelectElement =
        cursorColorDropdown.shadowRoot!.querySelector('select');
    assert(cursorColorSelectElement);
    assertEquals(
        'SETTINGS_DROPDOWN_NOT_FOUND_ITEM', cursorColorSelectElement.value);

    // Turn cursor color to red, and verify pref is also red.
    cursorColorSelectElement.value = String(RED_CURSOR_COLOR);
    cursorColorSelectElement.dispatchEvent(new CustomEvent('change'));
    const cursorColorPref = page.getPref('settings.a11y.cursor_color');
    const cursorColorEnabledPref =
        page.getPref('settings.a11y.cursor_color_enabled');
    assertEquals(RED_CURSOR_COLOR, cursorColorPref.value);
    assertTrue(cursorColorEnabledPref.value);

    // Turn cursor color back to default, and verify pref is also default.
    cursorColorSelectElement.value = String(DEFAULT_BLACK_CURSOR_COLOR);
    cursorColorSelectElement.dispatchEvent(new CustomEvent('change'));
    assertEquals(DEFAULT_BLACK_CURSOR_COLOR, cursorColorPref.value);
    assertFalse(cursorColorEnabledPref.value);
  });

  test(
      'should focus pointerSubpageButton button when returning from touchpad subpage',
      async () => {
        const testRouter = createRouterForTesting();
        Router.resetInstanceForTesting(testRouter);
        const selector = '#pointerSubpageButton';
        const route = routes.PER_DEVICE_TOUCHPAD;
        // Only a touchpad is connected, no mouse.
        deviceBrowserProxy.hasMouse = false;
        deviceBrowserProxy.hasTouchpad = true;
        await initPage();
        flush();
        const router = Router.getInstance();

        const subpageButton =
            page.shadowRoot!.querySelector<HTMLElement>(selector);
        assert(subpageButton);

        subpageButton.click();
        assertEquals(route, router.currentRoute);
        assertNotEquals(
            subpageButton, page.shadowRoot!.activeElement,
            `${selector} should not be focused`);

        const popStateEventPromise = eventToPromise('popstate', window);
        router.navigateToPreviousRoute();
        await popStateEventPromise;
        await waitAfterNextRender(page);

        assertEquals(routes.A11Y_CURSOR_AND_TOUCHPAD, router.currentRoute);
        assertEquals(
            subpageButton, page.shadowRoot!.activeElement,
            `${selector} should be focused`);
      });

  test('Click pointerSubpageButton to navigate to mouse subpage', async () => {
    await setUpNavigationTest(
        /*hasMouse=*/ true, /*hasTouchpad=*/ false,
        /*hasPointingStick=*/ false);
    const row =
        page.shadowRoot!.querySelector<HTMLElement>('#pointerSubpageButton');
    assert(row);
    assertFalse(row.hidden);

    row.click();
    assertEquals(routes.PER_DEVICE_MOUSE, Router.getInstance().currentRoute);
  });

  test(
      'Click pointerSubpageButton to navigate to touchpad subpage',
      async () => {
        await setUpNavigationTest(
            /*hasMouse=*/ false, /*hasTouchpad=*/ true,
            /*hasPointingStick=*/ false);
        const row = page.shadowRoot!.querySelector<HTMLElement>(
            '#pointerSubpageButton');
        assert(row);
        assertFalse(row.hidden);

        row.click();
        assertEquals(
            routes.PER_DEVICE_TOUCHPAD, Router.getInstance().currentRoute);
      });

  test(
      'Click pointerSubpageButton to navigate to pointing stick subpage',
      async () => {
        await setUpNavigationTest(
            /*hasMouse=*/ false, /*hasTouchpad=*/ false,
            /*hasPointingStick=*/ true);
        const row = page.shadowRoot!.querySelector<HTMLElement>(
            '#pointerSubpageButton');
        assert(row);
        assertFalse(row.hidden);

        row.click();
        assertEquals(
            routes.PER_DEVICE_POINTING_STICK,
            Router.getInstance().currentRoute);
      });

  test('Click pointerSubpageButton to navigate to device subpage', async () => {
    // All Mouse, touchpad and pointing stick are connected.
    await setUpNavigationTest(
        /*hasMouse=*/ true, /*hasTouchpad=*/ true,
        /*hasPointingStick=*/ true);
    const row =
        page.shadowRoot!.querySelector<HTMLElement>('#pointerSubpageButton');
    assert(row);
    assertFalse(row.hidden);

    row.click();
    assertEquals(routes.DEVICE, Router.getInstance().currentRoute);

    Router.getInstance().navigateToPreviousRoute();
    // Touchpad and pointing stick are connected.
    setUpDeviceBrowserProxy(
        /*hasMouse=*/ false, /*hasTouchpad=*/ true, /*hasPointingStick=*/ true);
    row.click();
    assertEquals(routes.DEVICE, Router.getInstance().currentRoute);

    Router.getInstance().navigateToPreviousRoute();
    // Mouse and pointing stick are connected.
    setUpDeviceBrowserProxy(
        /*hasMouse=*/ true, /*hasTouchpad=*/ false, /*hasPointingStick=*/ true);
    row.click();
    assertEquals(routes.DEVICE, Router.getInstance().currentRoute);

    Router.getInstance().navigateToPreviousRoute();
    // Mouse and touchpad are connected.
    setUpDeviceBrowserProxy(
        /*hasMouse=*/ true, /*hasTouchpad=*/ true, /*hasPointingStick=*/ false);
    row.click();
    assertEquals(routes.DEVICE, Router.getInstance().currentRoute);

    Router.getInstance().navigateToPreviousRoute();
    // No device is connected.
    setUpDeviceBrowserProxy(
        /*hasMouse=*/ false, /*hasTouchpad=*/ false,
        /*hasPointingStick=*/ false);
    row.click();
    assertEquals(routes.DEVICE, Router.getInstance().currentRoute);
  });

  test('Pointers row only visible if mouse/touchpad present', async () => {
    await initPage();
    const row =
        page.shadowRoot!.querySelector<HTMLElement>('#pointerSubpageButton');
    assert(row);
    assertFalse(row.hidden);

    // Has touchpad, doesn't have mouse ==> not hidden.
    deviceBrowserProxy.hasMouse = false;
    assertFalse(row.hidden);

    // Doesn't have either ==> hidden.
    deviceBrowserProxy.hasTouchpad = false;
    assertTrue(row.hidden);

    // Has mouse, doesn't have touchpad ==> not hidden.
    deviceBrowserProxy.hasMouse = true;
    assertFalse(row.hidden);

    // Has both ==> not hidden.
    deviceBrowserProxy.hasTouchpad = true;
    assertFalse(row.hidden);
  });

  test('tablet mode buttons visible', async () => {
    loadTimeData.overrideValues({
      isKioskModeActive: false,
      showTabletModeShelfNavigationButtonsSettings: true,
    });
    await initPage();
    flush();

    assertTrue(isVisible(page.shadowRoot!.querySelector(
        '#shelfNavigationButtonsEnabledControl')));
  });

  test('toggle tablet mode buttons', async () => {
    loadTimeData.overrideValues({
      isKioskModeActive: false,
      showTabletModeShelfNavigationButtonsSettings: true,
    });
    await initPage();
    flush();

    const navButtonsToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#shelfNavigationButtonsEnabledControl');
    assert(navButtonsToggle);
    assertTrue(isVisible(navButtonsToggle));
    // The default pref value is false.
    assertFalse(navButtonsToggle.checked);

    // Clicking the toggle should update the toggle checked value, and the
    // backing preference.
    navButtonsToggle.click();
    flush();

    assertTrue(navButtonsToggle.checked);
    assertFalse(navButtonsToggle.disabled);
    assertTrue(
        page.prefs.settings.a11y.tablet_mode_shelf_nav_buttons_enabled.value);

    navButtonsToggle.click();
    flush();

    assertFalse(navButtonsToggle.checked);
    assertFalse(navButtonsToggle.disabled);
    assertFalse(
        page.prefs.settings.a11y.tablet_mode_shelf_nav_buttons_enabled.value);
  });

  test('tablet mode buttons toggle disabled with spoken feedback', async () => {
    loadTimeData.overrideValues({
      isKioskModeActive: false,
      showTabletModeShelfNavigationButtonsSettings: true,
    });

    await initPage();
    flush();

    // Enable spoken feedback (ChromeVox).
    page.setPrefValue('settings.accessibility', true);

    const navButtonsToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#shelfNavigationButtonsEnabledControl');
    assert(navButtonsToggle);
    assertTrue(isVisible(navButtonsToggle));

    // If spoken feedback is enabled, the shelf nav buttons toggle should be
    // disabled and checked.
    assertTrue(navButtonsToggle.disabled);
    assertTrue(navButtonsToggle.checked);

    // Clicking the toggle should have no effect.
    navButtonsToggle.click();
    flush();

    assertTrue(navButtonsToggle.disabled);
    assertTrue(navButtonsToggle.checked);
    assertFalse(
        page.prefs.settings.a11y.tablet_mode_shelf_nav_buttons_enabled.value);

    // The toggle should be enabled if the spoken feedback gets disabled.
    page.set('prefs.settings.accessibility.value', false);
    flush();

    assertFalse(!!navButtonsToggle.disabled);
    assertFalse(navButtonsToggle.checked);
    assertFalse(
        page.prefs.settings.a11y.tablet_mode_shelf_nav_buttons_enabled.value);

    // Clicking the toggle should update the backing pref.
    navButtonsToggle.click();
    flush();

    assertFalse(!!navButtonsToggle.disabled);
    assertTrue(navButtonsToggle.checked);
    assertTrue(
        page.prefs.settings.a11y.tablet_mode_shelf_nav_buttons_enabled.value);
  });

  test('some parts are hidden in kiosk mode', async () => {
    loadTimeData.overrideValues({
      isKioskModeActive: true,
      showTabletModeShelfNavigationButtonsSettings: true,
    });
    await initPage();
    // Add mouse and touchpad to show some hidden settings.
    deviceBrowserProxy.hasMouse = true;
    deviceBrowserProxy.hasTouchpad = true;
    flush();

    // Shelf navigation buttons are not shown in kiosk mode, even if
    // showTabletModeShelfNavigationButtonsSettings is true.
    assertFalse(isVisible(page.shadowRoot!.querySelector(
        '#shelfNavigationButtonsEnabledControl')));

    const subpageLinks = page.root!.querySelectorAll('cr-link-row');
    subpageLinks.forEach(subpageLink => {
      assertFalse(
          isVisible(subpageLink), `expected ${subpageLink.id} to be invisible`);
    });
  });

  test('large cursor options appear when large cursor enabled', async () => {
    await initPage();
    const largeCursorToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#largeCursorEnabledControl');
    assert(largeCursorToggle);
    const largeCursorSizeSlider =
        page.shadowRoot!.querySelector('#largeCursorSizeSlider');
    assertFalse(isVisible(largeCursorSizeSlider));
    assertTrue(isVisible(largeCursorToggle));
    assertFalse(largeCursorToggle.checked);
    assertFalse(page.prefs.settings.a11y.large_cursor_enabled.value);
    largeCursorToggle.click();

    await waitBeforeNextRender(page);
    flush();
    assertTrue(largeCursorToggle.checked);
    assertTrue(page.prefs.settings.a11y.large_cursor_enabled.value);
    assertTrue(isVisible(largeCursorSizeSlider));
  });

  const settingsControls = [
    {
      id: 'autoClickToggle',
      prefKey: 'settings.a11y.autoclick',
      defaultValue: false,
      alternateValue: true,
      type: ControlType.TOGGLE,
    },
    {
      id: 'delayBeforeClickDropdown',
      prefKey: 'settings.a11y.autoclick_delay_ms',
      defaultValue: 1000,
      alternateValue: 2000,
      type: ControlType.DROPDOWN,
    },
    {
      id: 'autoClickStabilizePositionToggle',
      prefKey: 'settings.a11y.autoclick_stabilize_position',
      defaultValue: false,
      alternateValue: true,
      type: ControlType.TOGGLE,
    },
    {
      id: 'autoclickMovementThresholdDropdown',
      prefKey: 'settings.a11y.autoclick_movement_threshold',
      defaultValue: 20,
      alternateValue: 5,
      type: ControlType.DROPDOWN,
    },
    {
      id: 'autoClickRevertToLeftClickToggle',
      prefKey: 'settings.a11y.autoclick_revert_to_left_click',
      defaultValue: true,
      alternateValue: false,
      type: ControlType.TOGGLE,
    },
  ];

  settingsControls.forEach(control => {
    const {id, prefKey, defaultValue, alternateValue, type} = control;

    test(`Autoclick ${type} ${id} syncs to Pref: ${prefKey}`, async () => {
      await initPage();

      // Ensure control exists.
      const control = page.shadowRoot!.querySelector<HTMLElement>(`#${id}`);
      assert(control);

      // Ensure pref is set to the default value.
      let pref = page.getPref(prefKey);
      assertEquals(defaultValue, pref.value);

      // Update control to alternate value.
      switch (type) {
        case ControlType.TOGGLE:
          control.click();
          break;
        case ControlType.DROPDOWN:
          await waitAfterNextRender(control);
          const controlElement = control.shadowRoot!.querySelector('select');
          assert(controlElement);
          controlElement.value = String(alternateValue);
          controlElement.dispatchEvent(new CustomEvent('change'));
          break;
        default:
          assertNotReachedCase(type);
      }

      // Ensure pref is set to the alternate value.
      pref = page.getPref(prefKey);
      assertEquals(alternateValue, pref.value);
    });
  });

  test(
      'cursor highlight pref enabled when cursor highlight enabled',
      async () => {
        await initPage();
        const cursorHighlightToggle =
            page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#cursorHighlightToggle');
        assert(cursorHighlightToggle);
        assertTrue(isVisible(cursorHighlightToggle));
        assertFalse(cursorHighlightToggle.checked);
        assertFalse(page.prefs.settings.a11y.cursor_highlight.value);
        cursorHighlightToggle.click();

        await waitBeforeNextRender(page);
        flush();
        assertTrue(cursorHighlightToggle.checked);
        assertTrue(page.prefs.settings.a11y.cursor_highlight.value);
      });

  test('overscroll setting enabled', async () => {
    await initPage();
    const overscrollToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#overscrollToggle');

    // Setting is visible.
    assert(overscrollToggle);
    assertTrue(isVisible(overscrollToggle));

    // Pref has default value.
    assertTrue(overscrollToggle.checked);
    assertTrue(page.prefs.settings.a11y.overscroll_history_navigation.value);

    overscrollToggle.click();

    await waitBeforeNextRender(page);
    flush();
    assertFalse(overscrollToggle.checked);
    assertFalse(page.prefs.settings.a11y.overscroll_history_navigation.value);
  });

  test('kOverscrollSetting is deep-linkable', async () => {
    await initPage();

    const setting = settingMojom.Setting.kOverscrollEnabled;
    const params = new URLSearchParams();
    params.append('settingId', setting.toString());
    Router.getInstance().navigateTo(routes.A11Y_CURSOR_AND_TOUCHPAD, params);

    const deepLinkElement =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#overscrollToggle');

    assert(deepLinkElement);

    await waitAfterNextRender(deepLinkElement);

    assertEquals(
        deepLinkElement, page.shadowRoot!.activeElement,
        `Element should be focused for settingId=${setting}'`);
  });

  test('face control feature shows', async () => {
    loadTimeData.overrideValues({isKioskModeActive: false});

    await initPage();
    const faceGazePageRow = getFaceGazePageRow();
    assertTrue(!!faceGazePageRow);
    assertTrue(isVisible(faceGazePageRow));

    assertFalse(page.prefs.settings.a11y.face_gaze.enabled.value);
  });

  test('can reach face control settings from row', async () => {
    loadTimeData.overrideValues({isKioskModeActive: false});

    await initPage();
    const faceGazePageRow = getFaceGazePageRow();
    assertTrue(!!faceGazePageRow);
    assertTrue(isVisible(faceGazePageRow));

    assertFalse(page.prefs.settings.a11y.face_gaze.enabled.value);

    // Clicking on it should update the route.
    faceGazePageRow.click();
    assertEquals(
        routes.MANAGE_FACEGAZE_SETTINGS, Router.getInstance().currentRoute);
  });

  test('Mouse keys feature disabled.', async () => {
    loadTimeData.overrideValues({
      isAccessibilityMouseKeysEnabled: false,
    });

    await initPage();

    // Toggle shouldn't be available if flag is disabled.
    const enableMouseKeysToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#mouseKeysToggle');
    assertNull(enableMouseKeysToggle);
  });

  test('Mouse keys: toggle is in sync with pref', async () => {
    loadTimeData.overrideValues({
      isAccessibilityMouseKeysEnabled: true,
    });
    await initPage();

    // If the flag is enabled, check that the UI works.
    assertFalse(page.prefs.settings.a11y.mouse_keys.enabled.value);

    // We should use primary keys by default.
    assertTrue(page.prefs.settings.a11y.mouse_keys.use_primary_keys.value);

    const enableMouseKeysToggle =
        page.shadowRoot!.querySelector<CrToggleElement>('#mouseKeysToggle');
    assert(enableMouseKeysToggle);
    assertTrue(isVisible(enableMouseKeysToggle));

    enableMouseKeysToggle.click();
    await waitBeforeNextRender(page);
    flush();

    assertTrue(page.prefs.settings.a11y.mouse_keys.enabled.value);
  });

  if (disableInternalTouchpadFeatureEnabled) {
    test(
        'Disable touchpad dropdown and pref sync in default mode', async () => {
          await initPage();
          const disableInternalTouchpadSelectElement =
              await getDisableInternalTouchpadSelectElement();
          // Make sure disable touchpad dropdown is set to never disabled,
          // matching default pref state.
          assertEquals(
              String(DisableTouchpadMode.NEVER),
              disableInternalTouchpadSelectElement.value);
        });

    test(
        'Disable touchpad dropdown and pref sync when always disabled',
        async () => {
          await initPage();
          const disableInternalTouchpadSelectElement =
              await getDisableInternalTouchpadSelectElement();
          assert(disableInternalTouchpadSelectElement);
          await waitAfterNextRender(disableInternalTouchpadSelectElement);
          // Change disable internal touchpad dropdown to always disabled,
          // and verify the pref is also set to always disabled.
          disableInternalTouchpadSelectElement.value =
              String(DisableTouchpadMode.ALWAYS);
          disableInternalTouchpadSelectElement.dispatchEvent(
              new CustomEvent('change'));
          const disableInternalTouchpadModePref =
              page.getPref('settings.a11y.disable_trackpad_mode');
          await waitAfterNextRender(page);
          assertEquals(
              DisableTouchpadMode.ALWAYS,
              disableInternalTouchpadModePref.value);
          assertTrue(isVisible(
              page.shadowRoot!.querySelector('#reEnableTouchpadLabel')));
        });

    test(
        'Disable touchpad dropdown and pref sync in mouse connected mode',
        async () => {
          await initPage();
          const disableInternalTouchpadSelectElement =
              await getDisableInternalTouchpadSelectElement();

          // Change disable internal touchpad dropdown to disable when mouse is
          // connected, and verify the pref is also set to always disabled.
          disableInternalTouchpadSelectElement.value =
              String(DisableTouchpadMode.ON_MOUSE_CONNECTED);
          disableInternalTouchpadSelectElement.dispatchEvent(
              new CustomEvent('change'));
          const disableInternalTouchpadModePref =
              page.getPref('settings.a11y.disable_trackpad_mode');

          await waitAfterNextRender(page);
          assertEquals(
              DisableTouchpadMode.ON_MOUSE_CONNECTED,
              disableInternalTouchpadModePref.value);
          assertTrue(isVisible(
              page.shadowRoot!.querySelector('#reEnableTouchpadLabel')));
        });

    test(
        'Disable touchpad dropdown and pref sync when never disabled',
        async () => {
          await initPage();
          const disableInternalTouchpadSelectElement =
              await getDisableInternalTouchpadSelectElement();

          // Change disable internal touchpad dropdown to Never,
          // and verify pref is also set to Never.
          disableInternalTouchpadSelectElement.value =
              String(DisableTouchpadMode.NEVER);
          disableInternalTouchpadSelectElement.dispatchEvent(
              new CustomEvent('change'));
          const disableInternalTouchpadModePref =
              page.getPref('settings.a11y.disable_trackpad_mode');

          await waitAfterNextRender(page);
          assertEquals(
              DisableTouchpadMode.NEVER, disableInternalTouchpadModePref.value);
          assertFalse(isVisible(
              page.shadowRoot!.querySelector('#reEnableTouchpadLabel')));
        });

    test('Disable touchpad dropdown is deep linkable', async () => {
      await initPage();
      const params = new URLSearchParams();
      const settingId = settingMojom.Setting.kDisableTouchpad.toString();
      params.append('settingId', settingId);
      Router.getInstance().navigateTo(routes.A11Y_CURSOR_AND_TOUCHPAD, params);
      flush();

      const dropdown = getDisableInternalTouchpadDropdown();
      await waitAfterNextRender(dropdown);
      assertEquals(
          dropdown, page.shadowRoot!.activeElement,
          `Disable touchpad dropdown should be focused for settingId=${
              settingId}.`);
    });
  } else {
    test('disable internal touchpad feature disabled', async () => {
      await initPage();
      const disableInternalTouchpadDropdown =
          page.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
              '#disableInternalTouchpadDropdown');

      // No setting visible.
      assertNull(disableInternalTouchpadDropdown);

      // Pref has default value.
      assertEquals(
          page.prefs.settings.a11y.disable_trackpad_mode.value,
          DisableTouchpadMode.NEVER);
      assertFalse(page.prefs.settings.a11y.disable_trackpad_enabled.value);
    });
  }
});
