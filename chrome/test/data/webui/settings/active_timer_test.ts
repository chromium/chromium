// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ActiveTimer} from 'chrome://settings/settings.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('ActiveTimer', function() {
  let timer: ActiveTimer;
  let recordLongTimeCalledWith: number[] = [];
  let nowValue = 0;
  let mockVisibilityState = 'visible';
  let mockHasFocus = true;

  let originalPerformanceNow: () => number;
  let originalVisibilityState: PropertyDescriptor|undefined;
  let originalHasFocus: () => boolean;

  setup(function() {
    nowValue = 0;
    mockVisibilityState = 'visible';
    mockHasFocus = true;
    recordLongTimeCalledWith = [];

    // Mock performance.now
    originalPerformanceNow = performance.now;
    performance.now = () => nowValue;

    // Mock document.visibilityState
    originalVisibilityState =
        Object.getOwnPropertyDescriptor(Document.prototype, 'visibilityState');
    Object.defineProperty(document, 'visibilityState', {
      get: () => mockVisibilityState,
      configurable: true,
    });

    // Mock document.hasFocus
    originalHasFocus = document.hasFocus;
    document.hasFocus = () => mockHasFocus;

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(function() {
    performance.now = originalPerformanceNow;
    if (originalVisibilityState) {
      Object.defineProperty(
          document, 'visibilityState', originalVisibilityState);
    } else {
      delete (document as {visibilityState?: string}).visibilityState;
    }
    document.hasFocus = originalHasFocus;
    if (timer) {
      timer.stop();
    }
  });

  test('tracks active time', () => {
    timer = new ActiveTimer((duration) => {
      recordLongTimeCalledWith.push(duration);
    });
    timer.start();  // starts timer (active)

    nowValue = 1_000;  // 1 second elapsed

    // Simulate blur
    mockHasFocus = false;
    window.dispatchEvent(new Event('blur'));  // Timer pauses. Total: 1_000ms

    nowValue = 3_000;  // 2 more seconds elapsed (inactive)

    // Simulate focus
    mockHasFocus = true;
    window.dispatchEvent(new Event('focus'));  // Timer resumes.

    nowValue = 5_000;  // 2 more seconds elapsed (active)

    // Simulate pagehide (unload)
    window.dispatchEvent(new Event('pagehide'));

    assertEquals(1, recordLongTimeCalledWith.length);
    // Active time: (1_000 - 0) + (5_000 - 3_000) = 3_000ms
    assertEquals(3_000, recordLongTimeCalledWith[0]!);
  });

  test('does not track time in background initially', () => {
    mockVisibilityState = 'hidden';
    timer = new ActiveTimer((duration) => {
      recordLongTimeCalledWith.push(duration);
    });
    timer.start();  // timer should not start

    nowValue = 5_000;  // 5 seconds elapsed (inactive)

    // Simulate pagehide (unload)
    window.dispatchEvent(new Event('pagehide'));

    // Should not record metric if no active time
    assertEquals(0, recordLongTimeCalledWith.length);
  });

  test('handles BFCache restore', () => {
    timer = new ActiveTimer((duration) => {
      recordLongTimeCalledWith.push(duration);
    });
    timer.start();

    nowValue = 1_000;

    // Simulate pagehide (BFCache)
    window.dispatchEvent(
        new PageTransitionEvent('pagehide', {persisted: true}));

    assertEquals(1, recordLongTimeCalledWith.length);
    assertEquals(1_000, recordLongTimeCalledWith[0]!);

    nowValue = 3_000;

    // Simulate pageshow (Restore)
    window.dispatchEvent(
        new PageTransitionEvent('pageshow', {persisted: true}));

    nowValue = 6_000;

    // Simulate pagehide (unload)
    window.dispatchEvent(
        new PageTransitionEvent('pagehide', {persisted: false}));

    assertEquals(2, recordLongTimeCalledWith.length);
    assertEquals(3_000, recordLongTimeCalledWith[1]!);
  });

  test('handles background freeze and resume', () => {
    timer = new ActiveTimer((duration) => {
      recordLongTimeCalledWith.push(duration);
    });
    timer.start();

    nowValue = 1_000;

    // Simulate tab switch (hidden)
    mockVisibilityState = 'hidden';
    document.dispatchEvent(
        new Event('visibilitychange'));  // Timer pauses. Total: 1_000ms

    nowValue = 2_000;

    // Simulate browser freezing the hidden tab
    document.dispatchEvent(new Event('freeze'));  // Timer already paused.

    nowValue = 4_000;

    // Simulate browser resuming the tab (still hidden)
    document.dispatchEvent(new Event('resume'));  // Timer should remain paused.

    nowValue = 5_000;

    // Simulate user switching back to the tab (visible)
    mockVisibilityState = 'visible';
    document.dispatchEvent(new Event('visibilitychange'));  // Timer resumes.

    nowValue = 7_000;

    // Simulate pagehide (unload)
    window.dispatchEvent(new Event('pagehide'));

    assertEquals(1, recordLongTimeCalledWith.length);
    // Active time: 1000 (before hidden) + 2000 (after visible: 7000 - 5000) =
    // 3000ms
    assertEquals(3000, recordLongTimeCalledWith[0]!);
  });
});
