// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {WindowProxy} from 'chrome://settings/settings.js';
import {ActiveTimer, WindowProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

class TestWindowProxy extends TestBrowserProxy implements WindowProxy {
  private now_: number = 0;
  private visibilityState_: DocumentVisibilityState = 'visible';
  private hasFocus_: boolean = true;

  constructor() {
    super(['now', 'getVisibilityState', 'hasFocus']);
  }

  now(): number {
    this.methodCalled('now');
    return this.now_;
  }

  setNow(now: number) {
    this.now_ = now;
  }

  getVisibilityState(): DocumentVisibilityState {
    this.methodCalled('getVisibilityState');
    return this.visibilityState_;
  }

  setVisibilityState(visibilityState: DocumentVisibilityState) {
    this.visibilityState_ = visibilityState;
  }

  hasFocus(): boolean {
    this.methodCalled('hasFocus');
    return this.hasFocus_;
  }

  setHasFocus(hasFocus: boolean) {
    this.hasFocus_ = hasFocus;
  }
}

suite('ActiveTimer', function() {
  let timer: ActiveTimer;
  let testProxy: TestWindowProxy;
  let recordLongTimeCalledWith: number[] = [];

  setup(function() {
    testProxy = new TestWindowProxy();
    WindowProxyImpl.setInstance(testProxy);
    recordLongTimeCalledWith = [];

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(function() {
    if (timer) {
      timer.stop();
    }
  });

  test('tracks active time', () => {
    timer = new ActiveTimer((duration) => {
      recordLongTimeCalledWith.push(duration);
    });
    timer.start();  // starts timer (active)

    testProxy.setNow(1_000);  // 1 second elapsed

    // Simulate blur
    testProxy.setHasFocus(false);
    window.dispatchEvent(new Event('blur'));  // Timer pauses. Total: 1_000ms

    testProxy.setNow(3_000);  // 2 more seconds elapsed (inactive)

    // Simulate focus
    testProxy.setHasFocus(true);
    window.dispatchEvent(new Event('focus'));  // Timer resumes.

    testProxy.setNow(5_000);  // 2 more seconds elapsed (active)

    // Simulate pagehide (unload)
    window.dispatchEvent(new Event('pagehide'));

    assertEquals(1, recordLongTimeCalledWith.length);
    // Active time: (1_000 - 0) + (5_000 - 3_000) = 3_000ms
    assertEquals(3_000, recordLongTimeCalledWith[0]!);
    assertEquals(4, testProxy.getCallCount('now'));
  });

  test('does not track time in background initially', () => {
    testProxy.setVisibilityState('hidden');
    timer = new ActiveTimer((duration) => {
      recordLongTimeCalledWith.push(duration);
    });
    timer.start();  // timer should not start

    // Simulate pagehide (unload)
    window.dispatchEvent(new Event('pagehide'));

    // Should not record metric if no active time
    assertEquals(0, recordLongTimeCalledWith.length);
    assertEquals(0, testProxy.getCallCount('now'));
  });

  test('handles BFCache restore', () => {
    timer = new ActiveTimer((duration) => {
      recordLongTimeCalledWith.push(duration);
    });
    timer.start();

    testProxy.setNow(1_000);

    // Simulate pagehide (BFCache)
    window.dispatchEvent(
        new PageTransitionEvent('pagehide', {persisted: true}));

    assertEquals(1, recordLongTimeCalledWith.length);
    assertEquals(1_000, recordLongTimeCalledWith[0]!);
    assertEquals(2, testProxy.getCallCount('now'));

    testProxy.setNow(3_000);

    // Simulate pageshow (Restore)
    window.dispatchEvent(
        new PageTransitionEvent('pageshow', {persisted: true}));

    testProxy.setNow(6_000);

    // Simulate pagehide (unload)
    window.dispatchEvent(
        new PageTransitionEvent('pagehide', {persisted: false}));

    assertEquals(2, recordLongTimeCalledWith.length);
    assertEquals(3_000, recordLongTimeCalledWith[1]!);
    assertEquals(4, testProxy.getCallCount('now'));
  });

  test('handles background freeze and resume', () => {
    timer = new ActiveTimer((duration) => {
      recordLongTimeCalledWith.push(duration);
    });
    timer.start();

    testProxy.setNow(1_000);

    // Simulate tab switch (hidden)
    testProxy.setVisibilityState('hidden');
    document.dispatchEvent(
        new Event('visibilitychange'));  // Timer pauses. Total: 1_000ms

    testProxy.setNow(2_000);

    // Simulate browser freezing the hidden tab
    document.dispatchEvent(new Event('freeze'));  // Timer already paused.

    testProxy.setNow(4_000);

    // Simulate browser resuming the tab (still hidden)
    document.dispatchEvent(new Event('resume'));  // Timer should remain paused.

    testProxy.setNow(5_000);

    // Simulate user switching back to the tab (visible)
    testProxy.setVisibilityState('visible');
    document.dispatchEvent(new Event('visibilitychange'));  // Timer resumes.

    testProxy.setNow(7_000);

    // Simulate pagehide (unload)
    window.dispatchEvent(new Event('pagehide'));

    assertEquals(1, recordLongTimeCalledWith.length);
    // Active time: 1000 (before hidden) + 2000 (after visible: 7000 - 5000) =
    // 3000ms
    assertEquals(3000, recordLongTimeCalledWith[0]!);
    assertEquals(4, testProxy.getCallCount('now'));
  });
});
