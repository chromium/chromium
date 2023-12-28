// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsKeyboardElement} from 'chrome://os-settings/lazy_load.js';
import {DevicePageBrowserProxyImpl, Route, Router, routes, setDisplayApiForTesting, settingMojom, SettingsDevicePageElement, SettingsSliderElement} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {disableAnimationsAndTransitions} from 'chrome://webui-test/test_api.js';

import {FakeSystemDisplay} from '../fake_system_display.js';

import {getFakePrefs} from './device_page_test_util.js';
import {TestDevicePageBrowserProxy} from './test_device_page_browser_proxy.js';

suite('<settings-keyboard> for device page', () => {
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

  suite('keyboard', () => {
    const name = (k: string) => `prefs.settings.language.${k}.value`;
    const get = (k: string) => devicePage.get(name(k));
    const set = (k: string, v: number|boolean) => devicePage.set(name(k), v);

    let keyboardPage: SettingsKeyboardElement;

    setup(async () => {
      setDeviceSplitEnabled(false);
      await init();
      const element = showAndGetDeviceSubpage('keyboard', routes.KEYBOARD) as
          SettingsKeyboardElement;
      assertTrue(!!element);
      keyboardPage = element;
    });

    teardown(() => {
      keyboardPage.remove();
    });

    test('keyboard', async () => {
      // Initially, the optional keys are hidden.
      assertNull(keyboardPage.shadowRoot!.querySelector('#capsLockKey'));

      // Pretend no internal keyboard is available.
      const keyboardParams = {
        'showCapsLock': false,
        'showExternalMetaKey': false,
        'showAppleCommandKey': false,
        'hasLauncherKey': false,
        'hasAssistantKey': false,
      };
      webUIListenerCallback('show-keys-changed', keyboardParams);
      flush();
      assertNull(keyboardPage.shadowRoot!.querySelector('#launcherKey'));
      assertNull(keyboardPage.shadowRoot!.querySelector('#capsLockKey'));
      assertNull(keyboardPage.shadowRoot!.querySelector('#externalMetaKey'));
      assertNull(keyboardPage.shadowRoot!.querySelector('#externalCommandKey'));
      assertNull(keyboardPage.shadowRoot!.querySelector('#assistantKey'));

      // Pretend a Caps Lock key is now available.
      keyboardParams['showCapsLock'] = true;
      webUIListenerCallback('show-keys-changed', keyboardParams);
      flush();
      assertNull(keyboardPage.shadowRoot!.querySelector('#launcherKey'));
      assertTrue(!!keyboardPage.shadowRoot!.querySelector('#capsLockKey'));
      assertNull(keyboardPage.shadowRoot!.querySelector('#externalMetaKey'));
      assertNull(keyboardPage.shadowRoot!.querySelector('#externalCommandKey'));
      assertNull(keyboardPage.shadowRoot!.querySelector('#assistantKey'));

      // Add a non-Apple external keyboard.
      keyboardParams['showExternalMetaKey'] = true;
      webUIListenerCallback('show-keys-changed', keyboardParams);
      flush();
      assertNull(keyboardPage.shadowRoot!.querySelector('#launcherKey'));
      assertTrue(!!keyboardPage.shadowRoot!.querySelector('#capsLockKey'));
      assertTrue(!!keyboardPage.shadowRoot!.querySelector('#externalMetaKey'));
      assertNull(keyboardPage.shadowRoot!.querySelector('#externalCommandKey'));
      assertNull(keyboardPage.shadowRoot!.querySelector('#assistantKey'));

      // Add an Apple keyboard.
      keyboardParams['showAppleCommandKey'] = true;
      webUIListenerCallback('show-keys-changed', keyboardParams);
      flush();
      assertNull(keyboardPage.shadowRoot!.querySelector('#launcherKey'));
      assertTrue(!!keyboardPage.shadowRoot!.querySelector('#capsLockKey'));
      assertTrue(!!keyboardPage.shadowRoot!.querySelector('#externalMetaKey'));
      assertTrue(
          !!keyboardPage.shadowRoot!.querySelector('#externalCommandKey'));
      assertNull(keyboardPage.shadowRoot!.querySelector('#assistantKey'));

      // Add an internal keyboard.
      keyboardParams['hasLauncherKey'] = true;
      webUIListenerCallback('show-keys-changed', keyboardParams);
      flush();
      assertTrue(!!keyboardPage.shadowRoot!.querySelector('#launcherKey'));
      assertTrue(!!keyboardPage.shadowRoot!.querySelector('#capsLockKey'));
      assertTrue(!!keyboardPage.shadowRoot!.querySelector('#externalMetaKey'));
      assertTrue(
          !!keyboardPage.shadowRoot!.querySelector('#externalCommandKey'));
      assertNull(keyboardPage.shadowRoot!.querySelector('#assistantKey'));

      // Pretend an Assistant key is now available.
      keyboardParams['hasAssistantKey'] = true;
      webUIListenerCallback('show-keys-changed', keyboardParams);
      flush();
      assertTrue(!!keyboardPage.shadowRoot!.querySelector('#launcherKey'));
      assertTrue(!!keyboardPage.shadowRoot!.querySelector('#capsLockKey'));
      assertTrue(!!keyboardPage.shadowRoot!.querySelector('#externalMetaKey'));
      assertTrue(
          !!keyboardPage.shadowRoot!.querySelector('#externalCommandKey'));
      assertTrue(!!keyboardPage.shadowRoot!.querySelector('#assistantKey'));

      const collapse = keyboardPage.shadowRoot!.querySelector('iron-collapse');
      assertTrue(!!collapse);
      assertTrue(collapse.opened);

      let delaySlider =
          keyboardPage.shadowRoot!.querySelector<SettingsSliderElement>(
              '#delaySlider');
      assertTrue(!!delaySlider);
      assertEquals(500, delaySlider.pref.value);

      let repeatRateSlider =
          keyboardPage.shadowRoot!.querySelector<SettingsSliderElement>(
              '#repeatRateSlider');
      assertTrue(!!repeatRateSlider);
      assertEquals(500, repeatRateSlider.pref.value);

      const delayCrSlider = delaySlider.shadowRoot!.querySelector('cr-slider');
      assertTrue(!!delayCrSlider);

      const repeatCrSlider =
          repeatRateSlider.shadowRoot!.querySelector('cr-slider');
      assertTrue(!!repeatCrSlider);

      // Test interaction with the settings-slider's underlying cr-slider.
      pressAndReleaseKeyOn(delayCrSlider, 37 /* left */, [], 'ArrowLeft');
      pressAndReleaseKeyOn(repeatCrSlider, 39, [], 'ArrowRight');
      await flushTasks();
      assertEquals(1000, get('xkb_auto_repeat_delay_r2'));
      assertEquals(300, get('xkb_auto_repeat_interval_r2'));

      // Test sliders change when prefs change.
      set('xkb_auto_repeat_delay_r2', 1500);
      await flushTasks();

      delaySlider =
          keyboardPage.shadowRoot!.querySelector<SettingsSliderElement>(
              '#delaySlider');
      assertTrue(!!delaySlider);
      assertEquals(1500, delaySlider.pref.value);
      set('xkb_auto_repeat_interval_r2', 2000);
      await flushTasks();

      repeatRateSlider =
          keyboardPage.shadowRoot!.querySelector<SettingsSliderElement>(
              '#repeatRateSlider');
      assertTrue(!!repeatRateSlider);
      assertEquals(2000, repeatRateSlider.pref.value);

      // Test sliders round to nearest value when prefs change.
      set('xkb_auto_repeat_delay_r2', 600);
      await flushTasks();

      delaySlider =
          keyboardPage.shadowRoot!.querySelector<SettingsSliderElement>(
              '#delaySlider');
      assertTrue(!!delaySlider);
      assertEquals(500, delaySlider.pref.value);
      set('xkb_auto_repeat_interval_r2', 45);
      await flushTasks();

      repeatRateSlider =
          keyboardPage.shadowRoot!.querySelector<SettingsSliderElement>(
              '#repeatRateSlider');
      assertTrue(!!repeatRateSlider);
      assertEquals(50, repeatRateSlider.pref.value);

      set('xkb_auto_repeat_enabled_r2', false);
      assertFalse(collapse.opened);

      // Test keyboard shortcut viewer button.
      const button = keyboardPage.shadowRoot!.querySelector<HTMLButtonElement>(
          '#keyboardShortcutViewer');
      assertTrue(!!button);
      button.click();
      assertEquals(1, browserProxy.getCallCount('showKeyboardShortcutViewer'));
    });

    test('Deep link to keyboard shortcuts', async () => {
      const element =
          keyboardPage.shadowRoot!.querySelector('#keyboardShortcutViewer');
      assertTrue(!!element);
      const button = element.shadowRoot!.querySelector('cr-icon-button');
      assertTrue(!!button);
      await checkDeepLink(
          routes.KEYBOARD, settingMojom.Setting.kKeyboardShortcuts.toString(),
          button, 'Keyboard shortcuts button');
    });
  });
});
