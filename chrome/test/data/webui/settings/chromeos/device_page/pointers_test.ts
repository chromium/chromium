// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsPointersElement} from 'chrome://os-settings/lazy_load.js';
import {CrToggleElement, DevicePageBrowserProxyImpl, LocalizedLinkElement, Route, Router, routes, setDisplayApiForTesting, settingMojom, SettingsDevicePageElement, SettingsSliderElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {disableAnimationsAndTransitions} from 'chrome://webui-test/test_api.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {FakeSystemDisplay} from '../fake_system_display.js';

import {getFakePrefs} from './device_page_test_util.js';
import {TestDevicePageBrowserProxy} from './test_device_page_browser_proxy.js';

suite('<settings-pointers> for device page', () => {
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

  function expectReverseScrollValue(
      pointersPage: HTMLElement, expected: boolean): void {
    const reverseScrollToggle =
        pointersPage.shadowRoot!.querySelector<CrToggleElement>(
            '#enableReverseScrollingToggle');
    assertTrue(!!reverseScrollToggle);
    assertEquals(expected, reverseScrollToggle.checked);
    assertEquals(
        expected,
        devicePage.get('prefs.settings.touchpad.natural_scroll.value'));
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

  suite('pointers', () => {
    let pointersPage: SettingsPointersElement;

    setup(async () => {
      setDeviceSplitEnabled(false);
      await init();
      const page = showAndGetDeviceSubpage('pointers', routes.POINTERS) as
          SettingsPointersElement;
      pointersPage = page;
    });

    teardown(() => {
      pointersPage.remove();
    });

    test('subpage responds to pointer attach/detach', async () => {
      assertEquals(routes.POINTERS, Router.getInstance().currentRoute);
      [pointersPage.shadowRoot!.querySelector('#mouse'),
       pointersPage.shadowRoot!.querySelector('#mouse h2'),
       pointersPage.shadowRoot!.querySelector('#pointingStick'),
       pointersPage.shadowRoot!.querySelector('#pointingStick h2'),
       pointersPage.shadowRoot!.querySelector('#touchpad'),
       pointersPage.shadowRoot!.querySelector('#touchpad h2')]
          .forEach(element => assertTrue(isVisible(element)));
      webUIListenerCallback('has-touchpad-changed', false);
      await flushTasks();
      assertEquals(routes.POINTERS, Router.getInstance().currentRoute);
      [pointersPage.shadowRoot!.querySelector('#mouse'),
       pointersPage.shadowRoot!.querySelector('#mouse h2'),
       pointersPage.shadowRoot!.querySelector('#pointingStick'),
       pointersPage.shadowRoot!.querySelector('#pointingStick h2')]
          .forEach(element => assertTrue(isVisible(element)));
      assertFalse(
          isVisible(pointersPage.shadowRoot!.querySelector('#touchpad')));
      assertFalse(
          isVisible(pointersPage.shadowRoot!.querySelector('#touchpad h2')));

      webUIListenerCallback('has-pointing-stick-changed', false);
      await flushTasks();
      assertEquals(routes.POINTERS, Router.getInstance().currentRoute);
      assertTrue(isVisible(pointersPage.shadowRoot!.querySelector('#mouse')));
      [pointersPage.shadowRoot!.querySelector('#mouse h2'),
       pointersPage.shadowRoot!.querySelector('#pointingStick'),
       pointersPage.shadowRoot!.querySelector('#pointingStick h2'),
       pointersPage.shadowRoot!.querySelector('#touchpad'),
       pointersPage.shadowRoot!.querySelector('#touchpad h2')]
          .forEach(element => assertFalse(isVisible(element)));

      webUIListenerCallback('has-mouse-changed', false);
      await flushTasks();
      assertEquals(routes.DEVICE, Router.getInstance().currentRoute);
      assertFalse(isVisible(
          devicePage.shadowRoot!.querySelector('#main #pointersRow')));

      webUIListenerCallback('has-touchpad-changed', true);
      await flushTasks();
      assertTrue(isVisible(
          devicePage.shadowRoot!.querySelector('#main #pointersRow')));

      showAndGetDeviceSubpage('pointers', routes.POINTERS);
      [pointersPage.shadowRoot!.querySelector('#mouse'),
       pointersPage.shadowRoot!.querySelector('#mouse h2'),
       pointersPage.shadowRoot!.querySelector('#pointingStick'),
       pointersPage.shadowRoot!.querySelector('#pointingStick h2'),
       pointersPage.shadowRoot!.querySelector('#touchpad h2')]
          .forEach(element => assertFalse(isVisible(element)));
      assertTrue(
          isVisible(pointersPage.shadowRoot!.querySelector('#touchpad')));

      webUIListenerCallback('has-mouse-changed', true);
      assertEquals(routes.POINTERS, Router.getInstance().currentRoute);
      [pointersPage.shadowRoot!.querySelector('#mouse'),
       pointersPage.shadowRoot!.querySelector('#mouse h2'),
       pointersPage.shadowRoot!.querySelector('#touchpad'),
       pointersPage.shadowRoot!.querySelector('#touchpad h2')]
          .forEach(element => assertTrue(isVisible(element)));
      assertFalse(
          isVisible(pointersPage.shadowRoot!.querySelector('#pointingStick')));
      assertFalse(isVisible(
          pointersPage.shadowRoot!.querySelector('#pointingStick h2')));
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
      assertEquals(3, devicePage.prefs!['settings'].mouse.sensitivity2.value);

      pointersPage.set('prefs.settings.mouse.sensitivity2.value', 5);
      assertEquals(5, slider.pref.value);
    });

    test('touchpad', () => {
      assertTrue(
          isVisible(pointersPage.shadowRoot!.querySelector('#touchpad')));

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
          4, devicePage.prefs!['settings'].touchpad.sensitivity2.value);

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
          devicePage.prefs!['settings']
              .touchpad.haptic_click_sensitivity.value);

      pointersPage.set(
          'prefs.settings.touchpad.haptic_click_sensitivity.value', 1);
      assertEquals(1, slider.pref.value);
    });

    test('link doesn\'t activate control', () => {
      expectReverseScrollValue(pointersPage, false);

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
      expectReverseScrollValue(pointersPage, false);

      // Check specifically clicking toggle changes pref.
      const reverseScrollToggle =
          pointersPage.shadowRoot!.querySelector<CrToggleElement>(
              '#enableReverseScrollingToggle');
      assertTrue(!!reverseScrollToggle);
      reverseScrollToggle.click();
      expectReverseScrollValue(pointersPage, true);
      devicePage.set('prefs.settings.touchpad.natural_scroll.value', false);
      expectReverseScrollValue(pointersPage, false);

      // Check specifically clicking the row changes pref.
      const reverseScrollSettings =
          pointersPage.shadowRoot!.querySelector<HTMLElement>(
              '#reverseScrollRow');
      assertTrue(!!reverseScrollSettings);
      reverseScrollSettings.click();
      expectReverseScrollValue(pointersPage, true);
      devicePage.set('prefs.settings.touchpad.natural_scroll.value', false);
      expectReverseScrollValue(pointersPage, false);
    });

    test('pointing stick acceleration toggle', () => {
      const toggle =
          pointersPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
              '#pointingStickAcceleration');
      assertTrue(!!toggle);
      assertTrue(toggle.pref!.value);
      toggle.click();
      assertFalse(
          devicePage.prefs!['settings'].pointing_stick.acceleration.value);

      pointersPage.set(
          'prefs.settings.pointing_stick.acceleration.value', true);
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
          3, devicePage.prefs!['settings'].pointing_stick.sensitivity.value);

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
});
