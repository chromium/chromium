// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/cr_elements/cr_toast/cr_toast.m.js';
// #import {MockTimer} from '../mock_timer.m.js';
// clang-format on

suite('cr-toast', function() {
  let toast;
  let mockTimer;

  setup(function() {
    PolymerTest.clearBody();
    toast = document.createElement('cr-toast');
    document.body.appendChild(toast);
    mockTimer = new MockTimer();
    mockTimer.install();
  });

  teardown(function() {
    mockTimer.uninstall();
  });

  test('simple show/hide', function() {
    assertFalse(toast.open);

    toast.show();
    assertTrue(toast.open);

    toast.hide();
    assertFalse(toast.open);
  });

  test('auto hide with show()', function() {
    const duration = 100;
    toast.duration = duration;

    toast.show();
    assertTrue(toast.open);

    mockTimer.tick(duration);
    assertFalse(toast.open);
  });

  test('auto hide with (open = true)', function() {
    const duration = 100;
    toast.duration = duration;

    toast.open = true;

    mockTimer.tick(duration);
    assertFalse(toast.open);
  });

  test('show() clears auto-hide', function() {
    const duration = 70;
    toast.duration = duration;
    toast.open = true;
    mockTimer.tick(duration - 1);
    toast.show();

    // Auto-hide is cleared and toast should remain open.
    mockTimer.tick(1);
    assertTrue(toast.open);

    // When duration passes, new auto-hide should close toast.
    mockTimer.tick(duration - 2);
    assertTrue(toast.open);
    mockTimer.tick(1);
    mockTimer.tick(duration);
    assertFalse(toast.open);
  });

  test('(open = true) does not clear auto-hide', function() {
    const duration = 70;
    toast.duration = duration;
    toast.open = true;
    mockTimer.tick(duration - 1);
    toast.open = true;
    mockTimer.tick(1);
    assertFalse(toast.open);
  });

  test('clearing duration clears timeout', function() {
    const nonZeroDuration = 30;
    toast.duration = nonZeroDuration;
    toast.open = true;
    assertTrue(toast.open);

    const zeroDuration = 0;
    toast.duration = zeroDuration;
    mockTimer.tick(nonZeroDuration);
    assertTrue(toast.open);
  });

  test('setting a duration starts new auto-hide', function() {
    toast.duration = 0;
    toast.show();

    const nonZeroDuration = 50;
    toast.duration = nonZeroDuration;
    mockTimer.tick(nonZeroDuration - 1);
    assertTrue(toast.open);

    mockTimer.tick(1);
    assertFalse(toast.open);
  });

  test('setting duration clears auto-hide', function() {
    const oldDuration = 30;
    toast.duration = oldDuration;
    toast.open = true;

    mockTimer.tick(oldDuration - 1);
    assertTrue(toast.open);

    const newDuration = 50;
    toast.duration = newDuration;
    mockTimer.tick(newDuration - 1);
    assertTrue(toast.open);

    mockTimer.tick(1);
    assertFalse(toast.open);
  });

  test('setting duration using show(duration)', function() {
    const duration = 100;
    toast.show(duration);
    assertTrue(toast.open);

    mockTimer.tick(duration);
    assertFalse(toast.open);
  });
});
