// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import type {CrCollapseElement, SettingsRadioGroupElement} from 'chrome://settings/lazy_load.js';
import type {SettingsMemoryPageElement} from 'chrome://settings/settings.js';
import {MEMORY_SAVER_MODE_AGGRESSIVENESS_PREF, MEMORY_SAVER_MODE_PREF, MemorySaverModeAggressiveness, MemorySaverModeState, PerformanceMetricsProxyImpl, PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPerformanceMetricsProxy} from './test_performance_metrics_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

const INITIAL_PREFS: chrome.settingsPrivate.PrefObject[] = [
  {
    key: MEMORY_SAVER_MODE_PREF,
    type: chrome.settingsPrivate.PrefType.NUMBER,
    value: MemorySaverModeState.DISABLED,
  },
  {
    key: MEMORY_SAVER_MODE_AGGRESSIVENESS_PREF,
    type: chrome.settingsPrivate.PrefType.NUMBER,
    value: MemorySaverModeAggressiveness.MEDIUM,
  },
];

suite('MemorySaver', function() {
  let memoryPage: SettingsMemoryPageElement;
  let performanceMetricsProxy: TestPerformanceMetricsProxy;
  let prefsBrowserProxy: TestPrefsBrowserProxy;
  let prefService: PrefService;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    performanceMetricsProxy = new TestPerformanceMetricsProxy();
    PerformanceMetricsProxyImpl.setInstance(performanceMetricsProxy);

    prefsBrowserProxy = new TestPrefsBrowserProxy(INITIAL_PREFS);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    memoryPage = document.createElement('settings-memory-page');
    document.body.appendChild(memoryPage);
    await microtasksFinished();
  });

  test('MemorySaverModeEnabled', async function() {
    await prefService.setPrefValue(
        MEMORY_SAVER_MODE_PREF, MemorySaverModeState.ENABLED);
    assertTrue(memoryPage.$.toggleButton.checked);
  });

  test('MemorySaverModeDisabled', async function() {
    await prefService.setPrefValue(
        MEMORY_SAVER_MODE_PREF, MemorySaverModeState.DISABLED);
    assertFalse(memoryPage.$.toggleButton.checked);
  });

  test('MemorySaverModeChangeState', async function() {
    await prefService.setPrefValue(
        MEMORY_SAVER_MODE_PREF, MemorySaverModeState.DISABLED);

    memoryPage.$.toggleButton.click();
    let state = await performanceMetricsProxy.whenCalled(
        'recordMemorySaverModeChanged');
    assertEquals(state, MemorySaverModeState.ENABLED);
    assertEquals(
        prefService.getPref(MEMORY_SAVER_MODE_PREF).value,
        MemorySaverModeState.ENABLED);

    performanceMetricsProxy.reset();
    memoryPage.$.toggleButton.click();
    state = await performanceMetricsProxy.whenCalled(
        'recordMemorySaverModeChanged');
    assertEquals(state, MemorySaverModeState.DISABLED);
    assertEquals(
        prefService.getPref(MEMORY_SAVER_MODE_PREF).value,
        MemorySaverModeState.DISABLED);
  });
});

suite('MemorySaverAggressiveness', function() {
  let memoryPage: SettingsMemoryPageElement;
  let performanceMetricsProxy: TestPerformanceMetricsProxy;
  let conservativeButton: HTMLElement;
  let mediumButton: HTMLElement;
  let aggressiveButton: HTMLElement;
  let radioGroup: SettingsRadioGroupElement;
  let radioGroupCollapse: CrCollapseElement;
  let prefsBrowserProxy: TestPrefsBrowserProxy;
  let prefService: PrefService;

  /**
   * Used to get elements from the performance page that may or may not exist,
   * such as those inside a dom-if.
   * TODO(charlesmeng): remove once MemorySaverModeAggressiveness flag is
   * cleaned up, since elements can then be selected with $ interface
   */
  function getMemoryPageElement<T extends HTMLElement = HTMLElement>(
      id: string): T {
    const el = memoryPage.shadowRoot.querySelector<T>(`#${id}`);
    assertTrue(el !== null);
    assertTrue(el instanceof HTMLElement);
    return el;
  }

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    performanceMetricsProxy = new TestPerformanceMetricsProxy();
    PerformanceMetricsProxyImpl.setInstance(performanceMetricsProxy);

    prefsBrowserProxy = new TestPrefsBrowserProxy(INITIAL_PREFS);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    memoryPage = document.createElement('settings-memory-page');
    document.body.appendChild(memoryPage);
    await microtasksFinished();

    conservativeButton = getMemoryPageElement('conservativeButton');
    mediumButton = getMemoryPageElement('mediumButton');
    aggressiveButton = getMemoryPageElement('aggressiveButton');
    radioGroup = getMemoryPageElement('radioGroup');
    radioGroupCollapse = getMemoryPageElement('radioGroupCollapse');
  });

  test('MemorySaverModeDisabled', async function() {
    await prefService.setPrefValue(
        MEMORY_SAVER_MODE_PREF, MemorySaverModeState.DISABLED);
    await microtasksFinished();
    assertFalse(memoryPage.$.toggleButton.checked);
    assertFalse(radioGroupCollapse.opened);
  });

  test('MemorySaverModeEnabled', async function() {
    await prefService.setPrefValue(
        MEMORY_SAVER_MODE_PREF, MemorySaverModeState.ENABLED);
    await microtasksFinished();
    assertTrue(memoryPage.$.toggleButton.checked);
    assertTrue(radioGroupCollapse.opened);
    assertEquals(
        String(MemorySaverModeAggressiveness.MEDIUM), radioGroup.selected);
  });

  test('MemorySaverModeChangeState', async function() {
    async function testMemorySaverModeChangeState(
        expectedState: MemorySaverModeState) {
      performanceMetricsProxy.reset();
      memoryPage.$.toggleButton.click();

      const state = await performanceMetricsProxy.whenCalled(
          'recordMemorySaverModeChanged');
      assertEquals(state, expectedState);
      assertEquals(
          prefService.getPref(MEMORY_SAVER_MODE_PREF).value, expectedState);
    }

    async function testMemorySaverModeChangeAggressiveness(
        button: HTMLElement,
        expectedAggressiveness: MemorySaverModeAggressiveness) {
      performanceMetricsProxy.reset();
      button.click();

      const aggressiveness = await performanceMetricsProxy.whenCalled(
          'recordMemorySaverModeAggressivenessChanged');
      assertEquals(aggressiveness, expectedAggressiveness);
      assertEquals(
          prefService.getPref(MEMORY_SAVER_MODE_AGGRESSIVENESS_PREF).value,
          expectedAggressiveness);
    }

    await prefService.setPrefValue(
        MEMORY_SAVER_MODE_PREF, MemorySaverModeState.DISABLED);
    await prefService.setPrefValue(
        MEMORY_SAVER_MODE_AGGRESSIVENESS_PREF,
        MemorySaverModeAggressiveness.MEDIUM);

    await testMemorySaverModeChangeState(MemorySaverModeState.ENABLED);

    await testMemorySaverModeChangeAggressiveness(
        aggressiveButton, MemorySaverModeAggressiveness.AGGRESSIVE);

    await testMemorySaverModeChangeAggressiveness(
        conservativeButton, MemorySaverModeAggressiveness.CONSERVATIVE);

    await testMemorySaverModeChangeAggressiveness(
        mediumButton, MemorySaverModeAggressiveness.MEDIUM);

    await testMemorySaverModeChangeState(MemorySaverModeState.DISABLED);
  });

  test('MemorySaverModeAggressiveness', async function() {
    async function assertMemorySaverModeAggressivenessPolicyIndicatorExists(
        mode: MemorySaverModeAggressiveness, el: HTMLElement) {
      await prefService.setPrefValue(
          MEMORY_SAVER_MODE_AGGRESSIVENESS_PREF, mode);
      await microtasksFinished();
      assertTrue(!!el.shadowRoot!.querySelector('cr-policy-pref-indicator'));
    }

    await prefService.setPrefValue(
        MEMORY_SAVER_MODE_PREF, MemorySaverModeState.ENABLED);
    prefsBrowserProxy.fakeApi.sendPrefChanges([{
      key: MEMORY_SAVER_MODE_AGGRESSIVENESS_PREF,
      value: MemorySaverModeAggressiveness.MEDIUM,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
    }]);

    await assertMemorySaverModeAggressivenessPolicyIndicatorExists(
        MemorySaverModeAggressiveness.CONSERVATIVE, conservativeButton);

    await assertMemorySaverModeAggressivenessPolicyIndicatorExists(
        MemorySaverModeAggressiveness.MEDIUM, mediumButton);

    await assertMemorySaverModeAggressivenessPolicyIndicatorExists(
        MemorySaverModeAggressiveness.AGGRESSIVE, aggressiveButton);
  });
});
