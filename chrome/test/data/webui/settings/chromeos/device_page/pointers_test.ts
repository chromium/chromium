// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsPointersElement} from 'chrome://os-settings/lazy_load.js';
import {CrToggleElement, DevicePageBrowserProxyImpl, LocalizedLinkElement, Route, Router, routes, settingMojom, SettingsSliderElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

import {getFakePrefs} from './device_page_test_util.js';
import {TestDevicePageBrowserProxy} from './test_device_page_browser_proxy.js';

suite('<settings-pointers>', () => {
  let pointersPage: SettingsPointersElement;
  let browserProxy: TestDevicePageBrowserProxy;

  setup(async () => {
    browserProxy = new TestDevicePageBrowserProxy();
    DevicePageBrowserProxyImpl.setInstanceForTesting(browserProxy);

    Router.getInstance().navigateTo(routes.POINTERS);

    clearBody();
    pointersPage = document.createElement('settings-pointers');
    pointersPage.hasMouse = true;
    pointersPage.hasPointingStick = true;
    pointersPage.hasTouchpad = true;
    pointersPage.hasHapticTouchpad = true;
    pointersPage.prefs = getFakePrefs();
    document.body.appendChild(pointersPage);
    await flushTasks();
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
  });

  function expectReverseScrollValue(expected: boolean): void {
    const reverseScrollToggle =
        pointersPage.shadowRoot!.querySelector<CrToggleElement>(
            '#enableReverseScrollingToggle');
    assertTrue(!!reverseScrollToggle);
    assertEquals(expected, reverseScrollToggle.checked);
    assertEquals(
        expected,
        pointersPage.get('prefs.settings.touchpad.natural_scroll.value'));
  }

  /**
   * Checks that the deep link to a setting focuses the correct element.
   * @param deepLinkElement The element that should be focused by
   *                                   the deep link
   * @param elementDesc A human-readable description of the element,
   *                              for assertion messages
   */
  async function checkDeepLink(
      route: Route, settingId: string, deepLinkElement: HTMLElement,
      elementDesc: string): Promise<void> {
    const params = new URLSearchParams();
    params.append('settingId', settingId);
    Router.getInstance().navigateTo(route, params);

    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        `${elementDesc} should be focused for settingId=${settingId}.`);
  }

  function assertElementIsVisible(selector: string, visible: boolean) {
    const element = pointersPage.shadowRoot!.querySelector(selector);
    if (visible) {
      assertTrue(isVisible(element), `${selector} should be visible.`);
    } else {
      assertFalse(isVisible(element), `${selector} should not be visible.`);
    }
  }

  test('touchpad setting visibility', async () => {
    pointersPage.hasTouchpad = true;
    await flushTasks();
    [{selector: '#mouse', visible: true},
     {selector: '#mouse h2', visible: true},
     {selector: '#pointingStick', visible: true},
     {selector: '#pointingStick h2', visible: true},
     {selector: '#touchpad', visible: true},
     {selector: '#touchpad h2', visible: true},
    ].forEach(({selector, visible}) => {
      assertElementIsVisible(selector, visible);
    });

    pointersPage.hasTouchpad = false;
    await flushTasks();
    [{selector: '#mouse', visible: true},
     {selector: '#mouse h2', visible: true},
     {selector: '#pointingStick', visible: true},
     {selector: '#pointingStick h2', visible: true},
     {selector: '#touchpad', visible: false},
     {selector: '#touchpad h2', visible: false},
    ].forEach(({selector, visible}) => {
      assertElementIsVisible(selector, visible);
    });
  });

  test('pointing stick setting visibility', async () => {
    pointersPage.hasPointingStick = true;
    await flushTasks();
    [{selector: '#mouse', visible: true},
     {selector: '#mouse h2', visible: true},
     {selector: '#pointingStick', visible: true},
     {selector: '#pointingStick h2', visible: true},
     {selector: '#touchpad', visible: true},
     {selector: '#touchpad h2', visible: true},
    ].forEach(({selector, visible}) => {
      assertElementIsVisible(selector, visible);
    });

    pointersPage.hasPointingStick = false;
    await flushTasks();
    [{selector: '#mouse', visible: true},
     {selector: '#mouse h2', visible: true},
     {selector: '#pointingStick', visible: false},
     {selector: '#pointingStick h2', visible: false},
     {selector: '#touchpad', visible: true},
     {selector: '#touchpad h2', visible: true},
    ].forEach(({selector, visible}) => {
      assertElementIsVisible(selector, visible);
    });
  });

  test('mouse setting visibility', async () => {
    pointersPage.hasMouse = true;
    await flushTasks();
    [{selector: '#mouse', visible: true},
     {selector: '#mouse h2', visible: true},
     {selector: '#pointingStick', visible: true},
     {selector: '#pointingStick h2', visible: true},
     {selector: '#touchpad', visible: true},
     {selector: '#touchpad h2', visible: true},
    ].forEach(({selector, visible}) => {
      assertElementIsVisible(selector, visible);
    });

    pointersPage.hasMouse = false;
    await flushTasks();
    [{selector: '#mouse', visible: false},
     {selector: '#mouse h2', visible: false},
     {selector: '#pointingStick', visible: true},
     {selector: '#pointingStick h2', visible: true},
     {selector: '#touchpad', visible: true},
     {selector: '#touchpad h2', visible: true},
    ].forEach(({selector, visible}) => {
      assertElementIsVisible(selector, visible);
    });
  });

  test('mouse', () => {
    assertTrue(isVisible(pointersPage.shadowRoot!.querySelector('#mouse')));

    const slider =
        pointersPage.shadowRoot!.querySelector<SettingsSliderElement>(
            '#mouse settings-slider');
    assertTrue(!!slider);
    assertEquals(4, slider.pref.value);

    const crSlider = slider.shadowRoot!.querySelector('cr-slider');
    assertTrue(!!crSlider);

    pressAndReleaseKeyOn(crSlider, 37, [], 'ArrowLeft');
    assertEquals(
        3, pointersPage.get('prefs.settings.mouse.sensitivity2.value'));

    pointersPage.set('prefs.settings.mouse.sensitivity2.value', 5);
    assertEquals(5, slider.pref.value);
  });

  test('touchpad', () => {
    assertTrue(isVisible(pointersPage.shadowRoot!.querySelector('#touchpad')));

    const enableTapToClick =
        pointersPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#touchpad #enableTapToClick');
    assertTrue(!!enableTapToClick);
    assertTrue(enableTapToClick.checked);

    const enableTapDragging =
        pointersPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#touchpad #enableTapDragging');
    assertTrue(!!enableTapDragging);
    assertFalse(enableTapDragging.checked);

    const slider =
        pointersPage.shadowRoot!.querySelector<SettingsSliderElement>(
            '#touchpad settings-slider');
    assertTrue(!!slider);
    assertEquals(3, slider.pref.value);

    const crSlider = slider.shadowRoot!.querySelector('cr-slider');
    assertTrue(!!crSlider);

    pressAndReleaseKeyOn(crSlider, 39 /* right */, [], 'ArrowRight');
    assertEquals(
        4, pointersPage.get('prefs.settings.touchpad.sensitivity2.value'));

    pointersPage.set('prefs.settings.touchpad.sensitivity2.value', 2);
    assertEquals(2, slider.pref.value);
  });

  test('haptic touchpad', () => {
    const toggle = pointersPage.shadowRoot!.querySelector<CrToggleElement>(
        '#touchpadHapticFeedbackToggle');
    assertTrue(!!toggle);
    assertTrue(toggle.checked);

    const slider =
        pointersPage.shadowRoot!.querySelector<SettingsSliderElement>(
            '#touchpadHapticClickSensitivity');
    assertTrue(!!slider);
    assertEquals(3, slider.pref.value);

    const crSlider = slider.shadowRoot!.querySelector('cr-slider');
    assertTrue(!!crSlider);

    pressAndReleaseKeyOn(crSlider, 39 /* right */, [], 'ArrowRight');
    assertEquals(
        5,
        pointersPage.get(
            'prefs.settings.touchpad.haptic_click_sensitivity.value'));

    pointersPage.set(
        'prefs.settings.touchpad.haptic_click_sensitivity.value', 1);
    assertEquals(1, slider.pref.value);
  });

  test('link doesn\'t activate control', () => {
    expectReverseScrollValue(false);

    // Tapping the link shouldn't enable the radio button.
    const reverseScrollLabel =
        pointersPage.shadowRoot!.querySelector<LocalizedLinkElement>(
            '#enableReverseScrollingLabel');
    assertTrue(!!reverseScrollLabel);
    const anchor = reverseScrollLabel.$.container.querySelector('a');
    assertTrue(!!anchor);
    // Prevent actually opening a link, which would block test.
    anchor.removeAttribute('href');
    anchor.click();
    expectReverseScrollValue(false);

    // Check specifically clicking toggle changes pref.
    const reverseScrollToggle =
        pointersPage.shadowRoot!.querySelector<CrToggleElement>(
            '#enableReverseScrollingToggle');
    assertTrue(!!reverseScrollToggle);
    reverseScrollToggle.click();
    expectReverseScrollValue(true);
    pointersPage.set('prefs.settings.touchpad.natural_scroll.value', false);
    expectReverseScrollValue(false);

    // Check specifically clicking the row changes pref.
    const reverseScrollSettings =
        pointersPage.shadowRoot!.querySelector<HTMLElement>(
            '#reverseScrollRow');
    assertTrue(!!reverseScrollSettings);
    reverseScrollSettings.click();
    expectReverseScrollValue(true);
    pointersPage.set('prefs.settings.touchpad.natural_scroll.value', false);
    expectReverseScrollValue(false);
  });

  test('pointing stick acceleration toggle', () => {
    const toggle =
        pointersPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#pointingStickAcceleration');
    assertTrue(!!toggle);
    assertTrue(toggle.pref!.value);
    toggle.click();
    assertFalse(
        pointersPage.get('prefs.settings.pointing_stick.acceleration.value'));

    pointersPage.set('prefs.settings.pointing_stick.acceleration.value', true);
    assertTrue(toggle.pref!.value);
  });

  test('pointing stick speed slider', () => {
    const slider =
        pointersPage.shadowRoot!.querySelector<SettingsSliderElement>(
            '#pointingStick settings-slider');
    assertTrue(!!slider);
    assertEquals(4, slider.pref.value);

    const crSlider = slider.shadowRoot!.querySelector('cr-slider');
    assertTrue(!!crSlider);

    pressAndReleaseKeyOn(crSlider, 37, [], 'ArrowLeft');
    assertEquals(
        3, pointersPage.get('prefs.settings.pointing_stick.sensitivity.value'));

    pointersPage.set('prefs.settings.pointing_stick.sensitivity.value', 5);
    assertEquals(5, slider.pref.value);
  });

  test('Deep link to pointing stick primary button setting', async () => {
    const dropdown = pointersPage.shadowRoot!.querySelector(
        '#pointingStickSwapButtonDropdown');
    assertTrue(!!dropdown);
    const select = dropdown.shadowRoot!.querySelector('select');
    assertTrue(!!select);
    await checkDeepLink(
        routes.POINTERS,
        settingMojom.Setting.kPointingStickSwapPrimaryButtons.toString(),
        select, 'Pointing stick primary button dropdown');
  });

  test('Deep link to pointing stick acceleration setting', async () => {
    const element =
        pointersPage.shadowRoot!.querySelector('#pointingStickAcceleration');
    assertTrue(!!element);
    const toggle = element.shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!toggle);
    await checkDeepLink(
        routes.POINTERS,
        settingMojom.Setting.kPointingStickAcceleration.toString(), toggle,
        'Pointing stick acceleration slider');
  });

  test('Deep link to pointing stick speed setting', async () => {
    const speedSlider =
        pointersPage.shadowRoot!.querySelector('#pointingStickSpeedSlider');
    assertTrue(!!speedSlider);
    const crSlider = speedSlider.shadowRoot!.querySelector('cr-slider');
    assertTrue(!!crSlider);
    await checkDeepLink(
        routes.POINTERS, settingMojom.Setting.kPointingStickSpeed.toString(),
        crSlider, 'Pointing stick speed slider');
  });

  test('Deep link to touchpad speed', async () => {
    const settingsSlider =
        pointersPage.shadowRoot!.querySelector('#touchpadSensitivity');
    assertTrue(!!settingsSlider);
    const crSlider = settingsSlider.shadowRoot!.querySelector('cr-slider');
    assertTrue(!!crSlider);
    await checkDeepLink(
        routes.POINTERS, settingMojom.Setting.kTouchpadSpeed.toString(),
        crSlider, 'Touchpad speed slider');
  });
});
