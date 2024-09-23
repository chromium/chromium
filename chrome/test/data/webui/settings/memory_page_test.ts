// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrCollapseElement, SettingsRadioGroupElement} from 'chrome://settings/lazy_load.js';
import type {SettingsMemoryPageElement} from 'chrome://settings/settings.js';
import {MEMORY_SAVER_MODE_AGGRESSIVENESS_PREF, MEMORY_SAVER_MODE_PREF, MemorySaverModeAggressiveness, MemorySaverModeState, PerformanceMetricsProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestPerformanceMetricsProxy} from './test_performance_metrics_proxy.js';

const memorySaverModeMockPrefs = {
  high_efficiency_mode: {
    state: {
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: MemorySaverModeState.DISABLED,
    },
    aggressiveness: {
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: MemorySaverModeAggressiveness.MEDIUM,
    },
  },
};

suite('MemorySaver', function() {
  let memoryPage: SettingsMemoryPageElement;
  let performanceMetricsProxy: TestPerformanceMetricsProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    performanceMetricsProxy = new TestPerformanceMetricsProxy();
    PerformanceMetricsProxyImpl.setInstance(performanceMetricsProxy);

    memoryPage = document.createElement('settings-memory-page');
    memoryPage.set('prefs', {
      performance_tuning: {
        ...memorySaverModeMockPrefs,
      },
    });
    document.body.appendChild(memoryPage);
    flush();
  });

  test('testMemorySaverModeEnabled', function() {
    memoryPage.setPrefValue(
        MEMORY_SAVER_MODE_PREF, MemorySaverModeState.ENABLED);
    assertTrue(memoryPage.$.toggleButton.checked);
  });

  test('testMemorySaverModeDisabled', function() {
    memoryPage.setPrefValue(
        MEMORY_SAVER_MODE_PREF, MemorySaverModeState.DISABLED);
    assertFalse(memoryPage.$.toggleButton.checked);
  });

  test('testMemorySaverModeChangeState', async function() {
    memoryPage.setPrefValue(
        MEMORY_SAVER_MODE_PREF, MemorySaverModeState.DISABLED);

    memoryPage.$.toggleButton.click();
    let state = await performanceMetricsProxy.whenCalled(
        'recordMemorySaverModeChanged');
    assertEquals(state, MemorySaverModeState.ENABLED);
    assertEquals(
        memoryPage.getPref(MEMORY_SAVER_MODE_PREF).value,
        MemorySaverModeState.ENABLED);

    performanceMetricsProxy.reset();
    memoryPage.$.toggleButton.click();
    state = await performanceMetricsProxy.whenCalled(
        'recordMemorySaverModeChanged');
    assertEquals(state, MemorySaverModeState.DISABLED);
    assertEquals(
        memoryPage.getPref(MEMORY_SAVER_MODE_PREF).value,
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

  /**
   * Used to get elements from the performance page that may or may not exist,
   * such as those inside a dom-if.
   * TODO(charlesmeng): remove once MemorySaverModeAggressiveness flag is
   * cleaned up, since elements can then be selected with $ interface
   */
  function getMemoryPageElement<T extends HTMLElement = HTMLElement>(
      id: string): T {
    const el = memoryPage.shadowRoot!.querySelector<T>(`#${id}`);
    assertTrue(!!el);
    assertTrue(el instanceof HTMLElement);
    return el;
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    performanceMetricsProxy = new TestPerformanceMetricsProxy();
    PerformanceMetricsProxyImpl.setInstance(performanceMetricsProxy);

    memoryPage = document.createElement('settings-memory-page');
    memoryPage.set('prefs', {
      performance_tuning: {
        ...memorySaverModeMockPrefs,
      },
    });
    document.body.appendChild(memoryPage);
    flush();

    conservativeButton = getMemoryPageElement('conservativeButton');
    mediumButton = getMemoryPageElement('mediumButton');
    aggressiveButton = getMemoryPageElement('aggressiveButton');
    radioGroup = getMemoryPageElement('radioGroup');
    radioGroupCollapse = getMemoryPageElement('radioGroupCollapse');
  });

  test('testMemorySaverModeDisabled', function() {
    memoryPage.setPrefValue(
        MEMORY_SAVER_MODE_PREF, MemorySaverModeState.DISABLED);
    assertFalse(memoryPage.$.toggleButton.checked);
    assertFalse(radioGroupCollapse.opened);
  });

  test('testMemorySaverModeEnabled', function() {
    memoryPage.setPrefValue(
        MEMORY_SAVER_MODE_PREF, MemorySaverModeState.ENABLED);
    assertTrue(memoryPage.$.toggleButton.checked);
    assertTrue(radioGroupCollapse.opened);
    assertEquals(
        String(MemorySaverModeAggressiveness.MEDIUM), radioGroup.selected);
  });

  test('testMemorySaverModeChangeState', async function() {
    async function testMemorySaverModeChangeState(
        expectedState: MemorySaverModeState) {
      performanceMetricsProxy.reset();
      memoryPage.$.toggleButton.click();

      const state = await performanceMetricsProxy.whenCalled(
          'recordMemorySaverModeChanged');
      assertEquals(state, expectedState);
      assertEquals(
          memoryPage.getPref(MEMORY_SAVER_MODE_PREF).value, expectedState);
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
          memoryPage.getPref(MEMORY_SAVER_MODE_AGGRESSIVENESS_PREF).value,
          expectedAggressiveness);
    }

    memoryPage.setPrefValue(
        MEMORY_SAVER_MODE_PREF, MemorySaverModeState.DISABLED);
    memoryPage.setPrefValue(
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

  test('testMemorySaverModeAggressiveness', function() {
    function assertMemorySaverModeAggressivenessPolicyIndicatorExists(
        mode: MemorySaverModeAggressiveness, el: HTMLElement) {
      memoryPage.setPrefValue(MEMORY_SAVER_MODE_AGGRESSIVENESS_PREF, mode);
      flush();
      assertTrue(!!el.shadowRoot!.querySelector('cr-policy-pref-indicator'));
    }

    memoryPage.setPrefValue(
        MEMORY_SAVER_MODE_PREF, MemorySaverModeState.ENABLED);
    memoryPage.set(`prefs.${MEMORY_SAVER_MODE_AGGRESSIVENESS_PREF}`, {
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: MemorySaverModeAggressiveness.MEDIUM,
    });

    assertMemorySaverModeAggressivenessPolicyIndicatorExists(
        MemorySaverModeAggressiveness.CONSERVATIVE, conservativeButton);

    assertMemorySaverModeAggressivenessPolicyIndicatorExists(
        MemorySaverModeAggressiveness.MEDIUM, mediumButton);

    assertMemorySaverModeAggressivenessPolicyIndicatorExists(
        MemorySaverModeAggressiveness.AGGRESSIVE, aggressiveButton);
  });
});
