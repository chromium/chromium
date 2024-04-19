// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsKeyboardElement} from 'chrome://os-settings/lazy_load.js';
import {DevicePageBrowserProxyImpl, Route, Router, routes, settingMojom, SettingsSliderElement} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {clearBody} from '../utils.js';

import {getFakePrefs} from './device_page_test_util.js';
import {TestDevicePageBrowserProxy} from './test_device_page_browser_proxy.js';

suite('<settings-keyboard>', () => {
  let keyboardPage: SettingsKeyboardElement;
  let browserProxy: TestDevicePageBrowserProxy;

  setup(async () => {
    browserProxy = new TestDevicePageBrowserProxy();
    DevicePageBrowserProxyImpl.setInstanceForTesting(browserProxy);

    Router.getInstance().navigateTo(routes.KEYBOARD);

    clearBody();
    keyboardPage = document.createElement('settings-keyboard');
    keyboardPage.prefs = getFakePrefs();
    document.body.appendChild(keyboardPage);
    await flushTasks();
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
  });

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

  const name = (k: string) => `prefs.settings.language.${k}.value`;
  const get = (k: string) => keyboardPage.get(name(k));
  const set = (k: string, v: number|boolean) => keyboardPage.set(name(k), v);

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
    assertTrue(!!keyboardPage.shadowRoot!.querySelector('#externalCommandKey'));
    assertNull(keyboardPage.shadowRoot!.querySelector('#assistantKey'));

    // Add an internal keyboard.
    keyboardParams['hasLauncherKey'] = true;
    webUIListenerCallback('show-keys-changed', keyboardParams);
    flush();
    assertTrue(!!keyboardPage.shadowRoot!.querySelector('#launcherKey'));
    assertTrue(!!keyboardPage.shadowRoot!.querySelector('#capsLockKey'));
    assertTrue(!!keyboardPage.shadowRoot!.querySelector('#externalMetaKey'));
    assertTrue(!!keyboardPage.shadowRoot!.querySelector('#externalCommandKey'));
    assertNull(keyboardPage.shadowRoot!.querySelector('#assistantKey'));

    // Pretend an Assistant key is now available.
    keyboardParams['hasAssistantKey'] = true;
    webUIListenerCallback('show-keys-changed', keyboardParams);
    flush();
    assertTrue(!!keyboardPage.shadowRoot!.querySelector('#launcherKey'));
    assertTrue(!!keyboardPage.shadowRoot!.querySelector('#capsLockKey'));
    assertTrue(!!keyboardPage.shadowRoot!.querySelector('#externalMetaKey'));
    assertTrue(!!keyboardPage.shadowRoot!.querySelector('#externalCommandKey'));
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

    delaySlider = keyboardPage.shadowRoot!.querySelector<SettingsSliderElement>(
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

    delaySlider = keyboardPage.shadowRoot!.querySelector<SettingsSliderElement>(
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
        '#shortcutCustomizationApp');
    assertTrue(!!button);
    button.click();
    assertEquals(1, browserProxy.getCallCount('showShortcutCustomizationApp'));
  });

  test('Deep link to keyboard shortcuts', async () => {
    const element =
        keyboardPage.shadowRoot!.querySelector('#shortcutCustomizationApp');
    assertTrue(!!element);
    const button = element.shadowRoot!.querySelector('cr-icon-button');
    assertTrue(!!button);
    await checkDeepLink(
        routes.KEYBOARD, settingMojom.Setting.kKeyboardShortcuts.toString(),
        button, 'Keyboard shortcuts button');
  });
});
