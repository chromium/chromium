// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {deepEqual} from 'chrome://updater/tools.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('ToolsTest', () => {
  suite('deepEqual', () => {
    test('compares primitives', () => {
      assertTrue(deepEqual(1, 1));
      assertFalse(deepEqual(1, 2));
      assertTrue(deepEqual('a', 'a'));
      assertFalse(deepEqual('a', 'b'));
      assertTrue(deepEqual(true, true));
      assertFalse(deepEqual(true, false));
      assertTrue(deepEqual(null, null));
      assertTrue(deepEqual(undefined, undefined));
      assertFalse(deepEqual(null, undefined));
      assertFalse(deepEqual(1, '1'));
    });

    test('compares arrays', () => {
      assertTrue(deepEqual([], []));
      assertTrue(deepEqual([1, 2, 3], [1, 2, 3]));
      assertFalse(deepEqual([1, 2, 3], [1, 2]));
      assertFalse(deepEqual([1, 2, 3], [1, 2, 4]));
      assertTrue(deepEqual([1, [2, 3]], [1, [2, 3]]));
      assertFalse(deepEqual([1, [2, 3]], [1, [2, 4]]));
      assertFalse(deepEqual([1], {0: 1}));
    });

    test('compares objects', () => {
      assertTrue(deepEqual({}, {}));
      assertTrue(deepEqual({a: 1, b: 2}, {a: 1, b: 2}));
      assertTrue(deepEqual({a: 1, b: 2}, {b: 2, a: 1}));
      assertFalse(deepEqual({a: 1, b: 2}, {a: 1}));
      assertFalse(deepEqual({a: 1, b: 2}, {a: 1, b: 3}));
      assertTrue(deepEqual({a: {b: 1}}, {a: {b: 1}}));
      assertFalse(deepEqual({a: {b: 1}}, {a: {b: 2}}));
    });

    test('compares nested mixed types', () => {
      const val1 = {
        a: [1, 2, {c: 3}],
        b: 'test',
        d: {e: [4, 5]},
      };
      const val2 = {
        a: [1, 2, {c: 3}],
        b: 'test',
        d: {e: [4, 5]},
      };
      const val3 = {
        a: [1, 2, {c: 4}],
        b: 'test',
        d: {e: [4, 5]},
      };

      assertTrue(deepEqual(val1, val2));
      assertFalse(deepEqual(val1, val3));
    });
  });
});
