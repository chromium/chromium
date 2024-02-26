// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';

import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
// clang-format on

suite('cr-toast', function() {
  let toast: CrToastElement;
  let mockTimer: MockTimer;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    toast = document.createElement('cr-toast');
    document.body.appendChild(toast);
    mockTimer = new MockTimer();
    mockTimer.install();
  });

  teardown(function() {
    mockTimer.uninstall();
  });

  test('simple show/hide', async function() {
    assertFalse(toast.open);

    await toast.show();
    assertTrue(toast.open);

    await toast.hide();
    assertFalse(toast.open);
  });

  test('auto hide with show()', async function() {
    const duration = 100;
    toast.duration = duration;

    await toast.show();
    assertTrue(toast.open);

    mockTimer.tick(duration);
    assertFalse(toast.open);
  });

  test('show() clears auto-hide', async function() {
    const duration = 70;
    toast.duration = duration;
    await toast.show();
    mockTimer.tick(duration - 1);
    await toast.show();

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


  test('clearing duration clears timeout', async function() {
    const nonZeroDuration = 30;
    toast.duration = nonZeroDuration;
    await toast.show();
    assertTrue(toast.open);

    const zeroDuration = 0;
    toast.duration = zeroDuration;
    await toast.updateComplete;
    mockTimer.tick(nonZeroDuration);
    assertTrue(toast.open);
  });

  test('setting a duration starts new auto-hide', async function() {
    toast.duration = 0;
    await toast.show();

    const nonZeroDuration = 50;
    toast.duration = nonZeroDuration;
    await toast.updateComplete;
    mockTimer.tick(nonZeroDuration - 1);
    assertTrue(toast.open);

    mockTimer.tick(1);
    assertFalse(toast.open);
  });

  test('setting duration clears auto-hide', async function() {
    const oldDuration = 30;
    toast.duration = oldDuration;
    await toast.show();

    mockTimer.tick(oldDuration - 1);
    assertTrue(toast.open);

    const newDuration = 50;
    toast.duration = newDuration;
    await toast.updateComplete;
    mockTimer.tick(newDuration - 1);
    assertTrue(toast.open);

    mockTimer.tick(1);
    assertFalse(toast.open);
  });
});
