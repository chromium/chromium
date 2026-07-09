// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNull, assertThrows, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

suite('PrefService', function() {
  let proxy: TestPrefsBrowserProxy;
  let service: PrefService;

  const initialPrefs: chrome.settingsPrivate.PrefObject[] = [
    {
      key: 'browser.show_home_button',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'browser.homepage',
      type: chrome.settingsPrivate.PrefType.STRING,
      value: 'https://google.com',
    },
    {
      key: 'browser.dict_pref',
      type: chrome.settingsPrivate.PrefType.DICTIONARY,
      value: {
        key1: 1,
        key2: 2,
      },
    },
  ];

  setup(function() {
    proxy = new TestPrefsBrowserProxy(initialPrefs);
    PrefsBrowserProxy.setInstance(proxy);
    PrefService.resetInstanceForTesting();
    service = PrefService.getInstance();
    return service.whenInitialized();
  });

  test('Initialization', function() {
    const pref1 = service.getPref<boolean>('browser.show_home_button');
    assertFalse(pref1.value);
    assertEquals(chrome.settingsPrivate.PrefType.BOOLEAN, pref1.type);

    const pref2 = service.getPref<string>('browser.homepage');
    assertEquals('https://google.com', pref2.value);
    assertEquals(chrome.settingsPrivate.PrefType.STRING, pref2.type);
  });

  test('GetPrefTthrows', function() {
    assertThrows(() => {
      service.getPref('unknown.pref');
    });
  });

  test('AddObserverThrowsForUnknownPref', async function() {
    const rejectionPromise = eventToPromise('unhandledrejection', window);

    service.addObserver('unknown.pref', () => {});

    const rejectionEvent = await rejectionPromise;
    rejectionEvent.preventDefault();
    const error = rejectionEvent.reason as Error;
    assertEquals(
        'Assertion failed: pref \'unknown.pref\' not found in cache',
        error.message);
  });

  test('SetPrefValueSuccess', async function() {
    let calledCount = 0;
    let observedVal: string|null = null;
    const key = 'browser.homepage';

    service.addObserver<string>(key, pref => {
      calledCount++;
      observedVal = pref.value;
    });

    // Wait for initial call (dispatched via microtask after addObserver).
    await Promise.resolve();
    assertEquals(1, calledCount);
    assertEquals('https://google.com', observedVal);

    // Reset tracking.
    calledCount = 0;
    observedVal = null;

    // Verify synchronous update in cache.
    const promise = service.setPrefValue(key, 'https://chromium.org');
    assertEquals('https://chromium.org', service.getPref<string>(key).value);

    // Verify synchronous observer notification.
    assertEquals(1, calledCount);
    assertEquals('https://chromium.org', observedVal);

    // Verify promise resolves to true.
    const success = await promise;
    assertTrue(success);

    // Verify backend is called with correct params.
    const callArgs = await proxy.fakeApi.whenCalled('setPref');
    assertEquals(key, callArgs.key);
    assertEquals('https://chromium.org', callArgs.value);
  });

  test('SetPrefValueFailure', async function() {
    let calledCount = 0;
    let observedVal: string|null = null;
    const key = 'browser.homepage';

    service.addObserver<string>(key, pref => {
      calledCount++;
      observedVal = pref.value;
    });

    // Wait for initial call (dispatched via microtask after addObserver).
    await Promise.resolve();
    assertEquals(1, calledCount);
    assertEquals('https://google.com', observedVal);

    // Configure proxy to fail next setPref call.
    proxy.fakeApi.failNextSetPref();

    // Reset tracking.
    calledCount = 0;
    observedVal = null;

    // Verify synchronous update (speculative).
    const promise = service.setPrefValue(key, 'https://chromium.org');

    assertEquals('https://chromium.org', service.getPref<string>(key).value);
    assertEquals(1, calledCount);
    assertEquals('https://chromium.org', observedVal);

    // Reset tracking for revert notification.
    calledCount = 0;
    observedVal = null;

    // Verify promise resolves to false.
    const success = await promise;
    assertFalse(success);

    // Verify speculative cache change was reverted.
    assertEquals('https://google.com', service.getPref<string>(key).value);

    // Verify observers were notified of the revert.
    assertEquals(1, calledCount);
    assertEquals('https://google.com', observedVal);
  });

  test('SetPrefDictEntry', async function() {
    const key = 'browser.dict_pref';

    // Verify initial values.
    const initialPref = service.getPref<Record<string, number>>(key);
    assertEquals(1, initialPref.value['key1']);
    assertEquals(2, initialPref.value['key2']);

    // Set a new entry.
    let promise = service.setPrefDictEntry<number>(key, 'key3', 3);

    // Verify synchronous update in cache.
    let updatedPref = service.getPref<Record<string, number>>(key);
    assertEquals(1, updatedPref.value['key1']);
    assertEquals(2, updatedPref.value['key2']);
    assertEquals(3, updatedPref.value['key3']);

    // Verify promise resolves to true.
    let success = await promise;
    assertTrue(success);

    // Verify backend is called with correct params (the whole dict).
    let callArgs = await proxy.fakeApi.whenCalled('setPref');
    assertEquals(key, callArgs.key);
    assertDeepEquals({key1: 1, key2: 2, key3: 3}, callArgs.value);

    // Update an existing entry.
    proxy.fakeApi.resetResolver('setPref');
    promise = service.setPrefDictEntry<number>(key, 'key1', 10);

    // Verify synchronous update in cache.
    updatedPref = service.getPref<Record<string, number>>(key);
    assertEquals(10, updatedPref.value['key1']);
    assertEquals(2, updatedPref.value['key2']);
    assertEquals(3, updatedPref.value['key3']);

    // Verify promise resolves to true.
    success = await promise;
    assertTrue(success);

    // Verify backend is called with correct params (the whole dict).
    callArgs = await proxy.fakeApi.whenCalled('setPref');
    assertEquals(key, callArgs.key);
    assertDeepEquals({key1: 10, key2: 2, key3: 3}, callArgs.value);
  });

  test('addObserverSingleExternalChange', async function() {
    const key1 = 'browser.show_home_button';
    const key2 = 'browser.homepage';

    let calledCount1 = 0;
    let lastObservedPref1: chrome.settingsPrivate.PrefObject<boolean>|null =
        null;
    let calledCount2 = 0;
    let lastObservedPref2: chrome.settingsPrivate.PrefObject<string>|null =
        null;

    function reset() {
      calledCount1 = 0;
      lastObservedPref1 = null;
      calledCount2 = 0;
      lastObservedPref2 = null;
    }

    // Add observers.
    const observerId1 = service.addObserver<boolean>(key1, pref => {
      calledCount1++;
      lastObservedPref1 = pref;
    });

    const observerId2 = service.addObserver<string>(key2, pref => {
      calledCount2++;
      lastObservedPref2 = pref;
    });

    // Wait for initial calls (dispatched via microtask after addObserver).
    await Promise.resolve();
    assertEquals(1, calledCount1);
    assertFalse(lastObservedPref1!.value);
    assertEquals(1, calledCount2);
    assertEquals('https://google.com', lastObservedPref2!.value);

    reset();

    // Simulate backend change for pref1
    proxy.fakeApi.sendPrefChanges([{key: key1, value: true}]);

    assertEquals(1, calledCount1);
    assertTrue(lastObservedPref1!.value);
    assertTrue(service.getPref<boolean>(key1).value);
    assertEquals(0, calledCount2);
    assertNull(lastObservedPref2);
    reset();

    // Simulate backend change for pref2
    proxy.fakeApi.sendPrefChanges([{key: key2, value: 'https://chromium.org'}]);

    assertEquals(0, calledCount1);
    assertNull(lastObservedPref1);
    assertEquals(1, calledCount2);
    assertEquals('https://chromium.org', lastObservedPref2!.value);
    assertEquals('https://chromium.org', service.getPref<string>(key2).value);

    // Remove observers and verify they are not called anymore.
    assertTrue(service.removeObserver(observerId1));
    assertTrue(service.removeObserver(observerId2));
    reset();

    proxy.fakeApi.sendPrefChanges([
      {key: key1, value: false},
      {key: key2, value: 'https://google.com'},
    ]);
    assertEquals(0, calledCount1);
    assertEquals(0, calledCount2);
  });

  test('addObserverMultipleExternalChanges', async function() {
    const key1 = 'browser.show_home_button';
    const key2 = 'browser.homepage';

    assertFalse(service.getPref<boolean>(key1).value);
    assertEquals('https://google.com', service.getPref<string>(key2).value);

    let observer1CallCount = 0;
    let observer2CallCount = 0;

    service.addObserver<boolean>(key1, pref => {
      observer1CallCount++;
      // Skip the initial call.
      if (observer1CallCount === 1) {
        return;
      }
      // Verify that key2 is already updated in the cache.
      assertEquals('https://chromium.org', service.getPref<string>(key2).value);
      assertTrue(pref.value);
    });

    service.addObserver<string>(key2, pref => {
      observer2CallCount++;
      // Skip the initial call.
      if (observer2CallCount === 1) {
        return;
      }
      // Verify that key1 is already updated in the cache.
      assertTrue(service.getPref<boolean>(key1).value);
      assertEquals('https://chromium.org', pref.value);
    });

    // Wait for initial calls to settle.
    await Promise.resolve();
    assertEquals(1, observer1CallCount);
    assertEquals(1, observer2CallCount);

    // Simulate bulk change from backend.
    proxy.fakeApi.sendPrefChanges([
      {key: key1, value: true},
      {key: key2, value: 'https://chromium.org'},
    ]);

    assertEquals(2, observer1CallCount);
    assertEquals(2, observer2CallCount);
  });

  test('nestedPrefUpdatesOrder', async function() {
    const key1 = 'browser.show_home_button';
    const key2 = 'browser.homepage';

    assertFalse(service.getPref<boolean>(key1).value);
    assertEquals('https://google.com', service.getPref<string>(key2).value);

    let observer1CallCount = 0;
    let observer2CallCount = 0;
    const key2Notifications: string[] = [];

    service.addObserver<boolean>(key1, () => {
      observer1CallCount++;
      if (observer1CallCount === 1) {
        return;
      }
      // In prefA observer, change prefB to a different value.
      service.setPrefValue(key2, 'https://example.com');
    });

    service.addObserver<string>(key2, pref => {
      observer2CallCount++;
      if (observer2CallCount === 1) {
        return;
      }
      key2Notifications.push(pref.value);
    });

    // Wait for initial calls to settle.
    await Promise.resolve();
    assertEquals(1, observer1CallCount);
    assertEquals(1, observer2CallCount);

    // Simulate bulk change from backend.
    proxy.fakeApi.sendPrefChanges([
      {key: key1, value: true},
      {key: key2, value: 'https://chromium.org'},
    ]);

    // We expect 2 notifications for key2 (homepage).
    // The correct order should be:
    // 1. 'https://chromium.org' (from bulk change)
    // 2. 'https://example.com' (from programmatic change in key1 observer)
    assertEquals(3, observer2CallCount);  // 1 (initial) + 2 (updates)
    assertEquals(2, key2Notifications.length);
    assertEquals('https://chromium.org', key2Notifications[0]);
    assertEquals('https://example.com', key2Notifications[1]);
  });

  test('RemoveObserverFailure', function() {
    assertFalse(service.removeObserver(999));
  });
});
