// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

// clang-format on

suite('EventTargetModuleTest', () => {
  test('FucntionListener', () => {
    let fi = 0;
    function f(_e: Event) {
      fi++;
    }

    let gi = 0;
    function g(_e: Event) {
      gi++;
    }

    const et = new EventTarget();
    et.addEventListener('f', f);
    et.addEventListener('g', g);

    // Adding again should not cause it to be called twice
    et.addEventListener('f', f);
    et.dispatchEvent(new Event('f'));
    assertEquals(1, fi, 'Should have been called once');
    assertEquals(0, gi);

    et.removeEventListener('f', f);
    et.dispatchEvent(new Event('f'));
    assertEquals(1, fi, 'Should not have been called again');

    et.dispatchEvent(new Event('g'));
    assertEquals(1, gi, 'Should have been called once');
  });

  test('HandleEvent', () => {
    let fi = 0;
    const f = /** @type {!EventListener} */ ({
      handleEvent: function(_e: Event) {
        fi++;
      }
    });

    let gi = 0;
    const g = /** @type {!EventListener} */ ({
      handleEvent: function(_e: Event) {
        gi++;
      }
    });

    const et = new EventTarget();
    et.addEventListener('f', f);
    et.addEventListener('g', g);

    // Adding again should not cause it to be called twice
    et.addEventListener('f', f);
    et.dispatchEvent(new Event('f'));
    assertEquals(1, fi, 'Should have been called once');
    assertEquals(0, gi);

    et.removeEventListener('f', f);
    et.dispatchEvent(new Event('f'));
    assertEquals(1, fi, 'Should not have been called again');

    et.dispatchEvent(new Event('g'));
    assertEquals(1, gi, 'Should have been called once');
  });

  test('PreventDefault', () => {
    let i = 0;
    function prevent(e: Event) {
      i++;
      e.preventDefault();
    }

    let j = 0;
    function pass(_e: Event) {
      j++;
    }

    const et = new EventTarget();
    et.addEventListener('test', pass);

    assertTrue(et.dispatchEvent(new Event('test', {cancelable: true})));
    assertEquals(1, j);

    et.addEventListener('test', prevent);

    assertFalse(et.dispatchEvent(new Event('test', {cancelable: true})));
    assertEquals(2, j);
    assertEquals(1, i);
  });
});
