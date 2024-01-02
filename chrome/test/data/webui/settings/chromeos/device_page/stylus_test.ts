// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsStylusElement} from 'chrome://os-settings/lazy_load.js';
import {CrLinkRowElement, CrPolicyIndicatorElement, CrToggleElement, DevicePageBrowserProxyImpl, NoteAppInfo, NoteAppLockScreenSupport, Route, Router, routes, settingMojom, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {flush, microTask} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

import {getFakePrefs} from './device_page_test_util.js';
import {TestDevicePageBrowserProxy} from './test_device_page_browser_proxy.js';

suite('<settings-stylus>', () => {
  let stylusPage: SettingsStylusElement;
  let browserProxy: TestDevicePageBrowserProxy;

  let appSelector: HTMLSelectElement;
  let noAppsDiv: HTMLElement;
  let waitingDiv: HTMLElement;
  // Shorthand for NoteAppLockScreenSupport.
  let LockScreenSupport: typeof NoteAppLockScreenSupport;

  setup(async () => {
    // Always show stylus settings.
    loadTimeData.overrideValues({
      hasInternalStylus: true,
    });

    browserProxy = new TestDevicePageBrowserProxy();
    DevicePageBrowserProxyImpl.setInstanceForTesting(browserProxy);

    Router.getInstance().navigateTo(routes.STYLUS);

    clearBody();
    stylusPage = document.createElement('settings-stylus');
    stylusPage.prefs = getFakePrefs();
    document.body.appendChild(stylusPage);
    await flushTasks();

    const selectElement =
        stylusPage.shadowRoot!.querySelector<HTMLSelectElement>('#selectApp');
    assertTrue(!!selectElement);
    appSelector = selectElement;
    const div1 = stylusPage.shadowRoot!.querySelector<HTMLElement>('#no-apps');
    assertTrue(!!div1);
    noAppsDiv = div1;
    const div2 = stylusPage.shadowRoot!.querySelector<HTMLElement>('#waiting');
    assertTrue(!!div2);
    waitingDiv = div2;
    LockScreenSupport = NoteAppLockScreenSupport;
    assertEquals(1, browserProxy.getCallCount('requestNoteTakingApps'));
    assertTrue(!!browserProxy['onNoteTakingAppsUpdated_']);
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

  // Helper function to allocate a note app entry.
  function entry(
      name: string, value: string, preferred: boolean,
      lockScreenSupport: NoteAppLockScreenSupport): NoteAppInfo {
    return {
      name,
      value,
      preferred,
      lockScreenSupport,
    };
  }

  function noteTakingAppLockScreenSettings(): HTMLElement|null {
    return stylusPage.shadowRoot!.querySelector(
        '#note-taking-app-lock-screen-settings');
  }

  function enableAppOnLockScreenToggle(): CrToggleElement|null {
    return stylusPage.shadowRoot!.querySelector(
        '#enable-app-on-lock-screen-toggle');
  }

  function enableAppOnLockScreenPolicyIndicator(): CrPolicyIndicatorElement|
      null {
    return stylusPage.shadowRoot!.querySelector(
        '#enable-app-on-lock-screen-policy-indicator');
  }

  function enableAppOnLockScreenToggleLabel(): HTMLElement {
    const label = stylusPage.shadowRoot!.querySelector<HTMLElement>(
        '#lock-screen-toggle-label');
    assertTrue(!!label);
    return label;
  }

  function keepLastNoteOnLockScreenToggle(): SettingsToggleButtonElement|null {
    return stylusPage.shadowRoot!.querySelector(
        '#keep-last-note-on-lock-screen-toggle');
  }

  test('stylus tools prefs', () => {
    // Both stylus tools prefs are initially false.
    assertFalse(stylusPage.get('prefs.settings.enable_stylus_tools.value'));
    assertFalse(
        stylusPage.get('prefs.settings.launch_palette_on_eject_event.value'));

    // Since both prefs are initially false, the launch palette on eject pref
    // toggle is disabled.
    const toolsToggle = stylusPage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#enableStylusToolsToggle');
    assertTrue(!!toolsToggle);

    let eventsToggle = stylusPage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#launchPaletteOnEjectEventToggle');
    assertTrue(!!eventsToggle);
    assertTrue(eventsToggle.disabled);

    assertFalse(stylusPage.get('prefs.settings.enable_stylus_tools.value'));
    assertFalse(
        stylusPage.get('prefs.settings.launch_palette_on_eject_event.value'));

    // Tapping the enable stylus tools pref causes the launch palette on
    // eject pref toggle to not be disabled anymore.
    toolsToggle.click();
    assertTrue(stylusPage.get('prefs.settings.enable_stylus_tools.value'));

    eventsToggle = stylusPage.shadowRoot!.querySelector<HTMLButtonElement>(
        '#launchPaletteOnEjectEventToggle');
    assertTrue(!!eventsToggle);
    assertFalse(eventsToggle.disabled);
    eventsToggle.click();
    assertTrue(
        stylusPage.get('prefs.settings.launch_palette_on_eject_event.value'));
  });

  test('choose first app if no preferred ones', () => {
    // Selector chooses the first value in list if there is no preferred
    // value set.
    browserProxy.setNoteTakingApps([
      entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
      entry('n2', 'v2', false, LockScreenSupport.NOT_SUPPORTED),
    ]);
    flush();
    assertEquals('v1', appSelector.value);
  });

  test('choose prefered app if exists', () => {
    // Selector chooses the preferred value if set.
    browserProxy.setNoteTakingApps([
      entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
      entry('n2', 'v2', true, LockScreenSupport.NOT_SUPPORTED),
    ]);
    flush();
    assertEquals('v2', appSelector.value);
  });

  test('change preferred app', () => {
    // Load app list.
    browserProxy.setNoteTakingApps([
      entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
      entry('n2', 'v2', true, LockScreenSupport.NOT_SUPPORTED),
    ]);
    flush();
    assertEquals(0, browserProxy.getCallCount('setPreferredNoteTakingApp'));
    assertEquals('v2', browserProxy.getPreferredNoteTakingAppId());

    // Update select element to new value, verify browser proxy is called.
    appSelector.value = 'v1';
    stylusPage['onSelectedAppChanged_']();
    assertEquals(1, browserProxy.getCallCount('setPreferredNoteTakingApp'));
    assertEquals('v1', browserProxy.getPreferredNoteTakingAppId());
  });

  test('preferred app does not change without interaction', () => {
    // Pass various types of data to page, verify the preferred note-app
    // does not change.
    browserProxy.setNoteTakingApps([]);
    flush();
    assertEquals('', browserProxy.getPreferredNoteTakingAppId());

    browserProxy['onNoteTakingAppsUpdated_']([], true);
    flush();
    assertEquals('', browserProxy.getPreferredNoteTakingAppId());

    browserProxy.addNoteTakingApp(
        entry('n', 'v', false, LockScreenSupport.NOT_SUPPORTED));
    flush();
    assertEquals('', browserProxy.getPreferredNoteTakingAppId());

    browserProxy.setNoteTakingApps([
      entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
      entry('n2', 'v2', true, LockScreenSupport.NOT_SUPPORTED),
    ]);
    flush();
    assertEquals(0, browserProxy.getCallCount('setPreferredNoteTakingApp'));
    assertEquals('v2', browserProxy.getPreferredNoteTakingAppId());
  });

  test('Deep link to preferred app', async () => {
    browserProxy.setNoteTakingApps([
      entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
      entry('n2', 'v2', false, LockScreenSupport.NOT_SUPPORTED),
    ]);
    browserProxy.setAndroidAppsReceived(true);

    const select =
        stylusPage.shadowRoot!.querySelector<HTMLElement>('#selectApp');
    assertTrue(!!select);
    await checkDeepLink(
        routes.STYLUS, settingMojom.Setting.kStylusNoteTakingApp.toString(),
        select, 'Note-taking apps dropdown');
  });

  test('app-visibility', () => {
    // No apps available.
    browserProxy.setNoteTakingApps([]);
    assert(noAppsDiv.hidden);
    assert(!waitingDiv.hidden);
    assert(appSelector.hidden);

    // Waiting for apps to finish loading.
    browserProxy.setAndroidAppsReceived(true);
    assert(!noAppsDiv.hidden);
    assert(waitingDiv.hidden);
    assert(appSelector.hidden);

    // Apps loaded, show selector.
    browserProxy.addNoteTakingApp(
        entry('n', 'v', false, LockScreenSupport.NOT_SUPPORTED));
    assert(noAppsDiv.hidden);
    assert(waitingDiv.hidden);
    assert(!appSelector.hidden);

    // Waiting for Android apps again.
    browserProxy.setAndroidAppsReceived(false);
    assert(noAppsDiv.hidden);
    assert(!waitingDiv.hidden);
    assert(appSelector.hidden);

    browserProxy.setAndroidAppsReceived(true);
    assert(noAppsDiv.hidden);
    assert(waitingDiv.hidden);
    assert(!appSelector.hidden);
  });

  test('enabled-on-lock-screen', async () => {
    assertFalse(isVisible(noteTakingAppLockScreenSettings()));
    assertFalse(isVisible(enableAppOnLockScreenToggle()));
    assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

    await new Promise((resolve) => {
      // No apps available.
      browserProxy.setNoteTakingApps([]);
      microTask.run(resolve);
    });

    flush();
    assertFalse(isVisible(noteTakingAppLockScreenSettings()));
    assertFalse(isVisible(enableAppOnLockScreenToggle()));
    assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
    // Single app which does not support lock screen note taking.
    browserProxy.addNoteTakingApp(
        entry('n1', 'v1', true, LockScreenSupport.NOT_SUPPORTED));
    await new Promise((resolve) => microTask.run(resolve));

    flush();
    assertFalse(isVisible(noteTakingAppLockScreenSettings()));
    assertFalse(isVisible(enableAppOnLockScreenToggle()));
    assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
    // Add an app with lock screen support, but do not select it yet.
    browserProxy.addNoteTakingApp(
        entry('n2', 'v2', false, LockScreenSupport.SUPPORTED));
    await new Promise((resolve) => microTask.run(resolve));

    flush();
    assertFalse(isVisible(noteTakingAppLockScreenSettings()));
    assertFalse(isVisible(enableAppOnLockScreenToggle()));
    assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
    // Select the app with lock screen app support.
    appSelector.value = 'v2';
    stylusPage['onSelectedAppChanged_']();
    assertEquals(1, browserProxy.getCallCount('setPreferredNoteTakingApp'));
    assertEquals('v2', browserProxy.getPreferredNoteTakingAppId());
    await new Promise((resolve) => microTask.run(resolve));

    flush();
    assertTrue(isVisible(noteTakingAppLockScreenSettings()));
    assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
    assert(isVisible(enableAppOnLockScreenToggle()));
    assertFalse(enableAppOnLockScreenToggle()!.checked);
    // Preferred app updated to be enabled on lock screen.
    browserProxy.setNoteTakingApps([
      entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
      entry('n2', 'v2', true, LockScreenSupport.ENABLED),
    ]);
    await new Promise((resolve) => microTask.run(resolve));

    flush();
    assertTrue(isVisible(noteTakingAppLockScreenSettings()));
    assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
    assert(isVisible(enableAppOnLockScreenToggle()));
    assertTrue(enableAppOnLockScreenToggle()!.checked);
    // Select the app that does not support lock screen again.
    appSelector.value = 'v1';
    stylusPage['onSelectedAppChanged_']();
    assertEquals(2, browserProxy.getCallCount('setPreferredNoteTakingApp'));
    assertEquals('v1', browserProxy.getPreferredNoteTakingAppId());
    await new Promise((resolve) => microTask.run(resolve));

    flush();
    assertFalse(isVisible(noteTakingAppLockScreenSettings()));
    assertFalse(isVisible(enableAppOnLockScreenToggle()));
    assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
  });

  test('initial-app-lock-screen-enabled', async () => {
    await new Promise((resolve) => {
      browserProxy.setNoteTakingApps(
          [entry('n1', 'v1', true, LockScreenSupport.SUPPORTED)]);
      microTask.run(resolve);
    });

    flush();
    assertTrue(isVisible(noteTakingAppLockScreenSettings()));
    assert(isVisible(enableAppOnLockScreenToggle()));
    assertFalse(enableAppOnLockScreenToggle()!.checked);
    assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
    browserProxy.setNoteTakingApps(
        [entry('n1', 'v1', true, LockScreenSupport.ENABLED)]);
    await new Promise((resolve) => microTask.run(resolve));

    flush();
    assertTrue(isVisible(noteTakingAppLockScreenSettings()));
    assert(isVisible(enableAppOnLockScreenToggle()));
    assertTrue(enableAppOnLockScreenToggle()!.checked);
    assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
    browserProxy.setNoteTakingApps(
        [entry('n1', 'v1', true, LockScreenSupport.SUPPORTED)]);
    await new Promise((resolve) => microTask.run(resolve));

    flush();
    assertTrue(isVisible(noteTakingAppLockScreenSettings()));
    assert(isVisible(enableAppOnLockScreenToggle()));
    assertFalse(enableAppOnLockScreenToggle()!.checked);
    assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
    browserProxy.setNoteTakingApps([]);
    await new Promise((resolve) => microTask.run(resolve));

    flush();
    assertFalse(isVisible(enableAppOnLockScreenToggle()));
  });

  test('tap-on-enable-note-taking-on-lock-screen', async () => {
    await new Promise((resolve) => {
      browserProxy.setNoteTakingApps(
          [entry('n1', 'v1', true, LockScreenSupport.SUPPORTED)]);
      microTask.run(resolve);
    });

    flush();
    assert(isVisible(enableAppOnLockScreenToggle()));
    assertFalse(enableAppOnLockScreenToggle()!.checked);
    assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
    enableAppOnLockScreenToggle()!.click();
    assertEquals(
        1,
        browserProxy.getCallCount(
            'setPreferredNoteTakingAppEnabledOnLockScreen'));
    await new Promise((resolve) => microTask.run(resolve));

    flush();
    assertTrue(enableAppOnLockScreenToggle()!.checked);
    assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
    assertEquals(
        LockScreenSupport.ENABLED,
        browserProxy.getPreferredAppLockScreenState());
    enableAppOnLockScreenToggle()!.click();
    assertEquals(
        2,
        browserProxy.getCallCount(
            'setPreferredNoteTakingAppEnabledOnLockScreen'));
    await new Promise((resolve) => microTask.run(resolve));

    flush();
    assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
    assertFalse(enableAppOnLockScreenToggle()!.checked);
    assertEquals(
        LockScreenSupport.SUPPORTED,
        browserProxy.getPreferredAppLockScreenState());
  });

  test('tap-on-enable-note-taking-on-lock-screen-label', async () => {
    await new Promise((resolve) => {
      browserProxy.setNoteTakingApps(
          [entry('n1', 'v1', true, LockScreenSupport.SUPPORTED)]);
      microTask.run(resolve);
    });

    flush();
    assert(isVisible(enableAppOnLockScreenToggle()));
    assertFalse(enableAppOnLockScreenToggle()!.checked);
    enableAppOnLockScreenToggleLabel().click();
    assertEquals(
        1,
        browserProxy.getCallCount(
            'setPreferredNoteTakingAppEnabledOnLockScreen'));
    await new Promise((resolve) => microTask.run(resolve));

    flush();
    assertTrue(enableAppOnLockScreenToggle()!.checked);
    assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));
    assertEquals(
        LockScreenSupport.ENABLED,
        browserProxy.getPreferredAppLockScreenState());
    enableAppOnLockScreenToggleLabel().click();
    assertEquals(
        2,
        browserProxy.getCallCount(
            'setPreferredNoteTakingAppEnabledOnLockScreen'));
    await new Promise((resolve) => microTask.run(resolve));

    flush();
    assertFalse(enableAppOnLockScreenToggle()!.checked);
    assertEquals(
        LockScreenSupport.SUPPORTED,
        browserProxy.getPreferredAppLockScreenState());
  });

  test('lock-screen-apps-disabled-by-policy', async () => {
    assertFalse(isVisible(enableAppOnLockScreenToggle()));
    assertFalse(isVisible(enableAppOnLockScreenPolicyIndicator()));

    await new Promise((resolve) => {
      // Add an app with lock screen support.
      browserProxy.addNoteTakingApp(
          entry('n2', 'v2', true, LockScreenSupport.NOT_ALLOWED_BY_POLICY));
      microTask.run(resolve);
    });

    flush();
    assertTrue(isVisible(noteTakingAppLockScreenSettings()));
    assert(isVisible(enableAppOnLockScreenToggle()));
    assertFalse(enableAppOnLockScreenToggle()!.checked);
    assertTrue(isVisible(enableAppOnLockScreenPolicyIndicator()));
    // The toggle should be disabled, so enabling app on lock screen
    // should not be attempted.
    enableAppOnLockScreenToggle()!.click();
    assertEquals(
        0,
        browserProxy.getCallCount(
            'setPreferredNoteTakingAppEnabledOnLockScreen'));
    await new Promise((resolve) => microTask.run(resolve));

    flush();
    // Tap on label should not work either.
    enableAppOnLockScreenToggleLabel().click();
    assertEquals(
        0,
        browserProxy.getCallCount(
            'setPreferredNoteTakingAppEnabledOnLockScreen'));
    await new Promise((resolve) => microTask.run(resolve));

    flush();
    assertTrue(isVisible(noteTakingAppLockScreenSettings()));
    assert(isVisible(enableAppOnLockScreenToggle()));
    assertFalse(enableAppOnLockScreenToggle()!.checked);
    assertTrue(isVisible(enableAppOnLockScreenPolicyIndicator()));
    assertEquals(
        LockScreenSupport.NOT_ALLOWED_BY_POLICY,
        browserProxy.getPreferredAppLockScreenState());
  });

  test('keep-last-note-on-lock-screen', async () => {
    await new Promise((resolve) => {
      browserProxy.setNoteTakingApps([
        entry('n1', 'v1', true, LockScreenSupport.NOT_SUPPORTED),
        entry('n2', 'v2', false, LockScreenSupport.SUPPORTED),
      ]);
      microTask.run(resolve);
    });

    flush();
    assertFalse(isVisible(noteTakingAppLockScreenSettings()));
    assertFalse(isVisible(keepLastNoteOnLockScreenToggle()));
    browserProxy.setNoteTakingApps([
      entry('n1', 'v1', false, LockScreenSupport.NOT_SUPPORTED),
      entry('n2', 'v2', true, LockScreenSupport.SUPPORTED),
    ]);
    await new Promise((resolve) => microTask.run(resolve));

    flush();
    assertTrue(isVisible(noteTakingAppLockScreenSettings()));
    assertFalse(isVisible(keepLastNoteOnLockScreenToggle()));
    browserProxy.setNoteTakingApps([
      entry('n2', 'v2', true, LockScreenSupport.ENABLED),
    ]);
    await new Promise((resolve) => microTask.run(resolve));

    flush();
    assertTrue(isVisible(noteTakingAppLockScreenSettings()));
    assert(isVisible(keepLastNoteOnLockScreenToggle()));
    assertTrue(keepLastNoteOnLockScreenToggle()!.checked);
    // Clicking the toggle updates the pref value.
    const button = keepLastNoteOnLockScreenToggle()!.shadowRoot!
                       .querySelector<HTMLButtonElement>('#control');
    assertTrue(!!button);
    button.click();

    assertFalse(keepLastNoteOnLockScreenToggle()!.checked);
    assertFalse(
        stylusPage.get('prefs.settings.restore_last_lock_screen_note.value'));
    // Changing the pref value updates the toggle.
    stylusPage.set('prefs.settings.restore_last_lock_screen_note.value', true);
    assertTrue(keepLastNoteOnLockScreenToggle()!.checked);
  });

  test(
      'Clicking "Find more stylus apps" button should open Google Play',
      async () => {
        const findMoreAppsLink =
            stylusPage.shadowRoot!.querySelector<CrLinkRowElement>(
                '#findMoreAppsLink');
        assertTrue(
            !!findMoreAppsLink, 'Find more apps link element does not exist');
        assertFalse(findMoreAppsLink.hidden, 'Find more apps link is hidden');

        findMoreAppsLink.click();
        const url = await browserProxy.whenCalled('showPlayStore');
        const expectedUrl =
            'https://play.google.com/store/apps/collection/promotion_30023cb_stylus_apps';
        assertEquals(expectedUrl, url);
      });
});
