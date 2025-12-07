// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {SettingsPerformancePageIndexElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, PerformanceBrowserProxyImpl, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPerformanceBrowserProxy} from './test_performance_browser_proxy.js';

suite('PerformancePageIndex', function() {
  let index: SettingsPerformancePageIndexElement;
  let browserProxy: TestPerformanceBrowserProxy;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestPerformanceBrowserProxy();
    PerformanceBrowserProxyImpl.setInstance(browserProxy);

    const settingsPrefs = document.createElement('settings-prefs');
    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;
    index = document.createElement('settings-performance-page-index');
    index.prefs = settingsPrefs.prefs!;
    document.body.appendChild(index);
    return flushTasks();
  });

  test('Routing', async function() {
    function assertActiveViews(ids: string[]) {
      for (const id of ids) {
        assertTrue(
            !!index.$.viewManager.querySelector(`#${id}.active[slot=view]`));
      }
    }

    assertEquals(routes.BASIC, Router.getInstance().getCurrentRoute());
    assertActiveViews(['performance', 'memory', 'battery', 'speed']);

    Router.getInstance().navigateTo(routes.PERFORMANCE);
    await microtasksFinished();
    assertActiveViews(['performance', 'memory', 'battery', 'speed']);
  });

  test('BatteryVisibility_HasBattery', async function() {
    await browserProxy.whenCalled('getDeviceHasBattery');
    const batteryPage =
        index.$.viewManager.querySelector('settings-battery-page');
    assertTrue(!!batteryPage);
    // Battery section should be hidden by default.
    assertFalse(isVisible(batteryPage));

    // Simulate OnDeviceHasBatteryChanged from backend
    webUIListenerCallback('device-has-battery-changed', true);
    assertTrue(isVisible(batteryPage));
  });

  // Minimal (non-exhaustive) tests to ensure SearchableViewContainerMixin is
  // inherited correctly.
  test('Search', async function() {
    function assertVisibleViews(visible: string[], hidden: string[]) {
      for (const id of visible) {
        assertTrue(isVisible(index.$.viewManager.querySelector(`#${id}`)), id);
      }

      for (const id of hidden) {
        assertFalse(isVisible(index.$.viewManager.querySelector(`#${id}`)), id);
      }
    }

    // Case1: Results only in settings-performance-page
    let result = await index.searchContents('Performance issue alerts');
    assertFalse(result.canceled);
    assertEquals(2, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertVisibleViews(['performance'], ['memory', 'speed']);

    // Case2: Results only in settings-memory-page
    result = await index.searchContents('Memory Saver');
    assertFalse(result.canceled);
    assertEquals(2, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertVisibleViews(['memory'], ['performance', 'speed']);

    // Case3: Results only in settings-speed-page
    result = await index.searchContents('Speed');
    assertFalse(result.canceled);
    assertEquals(1, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertVisibleViews(['speed'], ['performance', 'memory']);

    // Case4: Results in all cards (ignoring 'battery' which is hidden by
    // default).
    result = await index.searchContents('Learn more');
    assertFalse(result.canceled);
    assertFalse(result.wasClearSearch);
    assertVisibleViews(['performance', 'memory', 'speed'], []);
  });
});
