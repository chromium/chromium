// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTrustedHTML, getTrustedScript, getTrustedScriptURL} from 'chrome://resources/js/static_types.js';

import {assertEquals, assertNotReached, assertThrows} from 'chrome://webui-test/chai_assert.js';

suite('StaticTypesTest', function() {
  test('compatible with Trusted Types', () => {
    const meta = document.createElement('meta');
    meta.httpEquiv = 'content-security-policy';
    meta.content = 'require-trusted-types-for \'script\';';
    document.head.appendChild(meta);
    try {
      getTrustedHTML`test`;
      getTrustedScript`test`;
      getTrustedScriptURL`test`;
    } catch (e) {
      assertNotReached('Trusted Types violation detected');
    }
  });

  test('returns Trusted Types', () => {
    assertEquals(getTrustedHTML`test` instanceof TrustedHTML, true);
    assertEquals(getTrustedScript`test` instanceof TrustedScript, true);
    assertEquals(getTrustedScriptURL`test` instanceof TrustedScriptURL, true);
  });

  test('accepts single and mutiple lines', () => {
    assertEquals(getTrustedHTML`test`.toString(), 'test');
    assertEquals(getTrustedScript`test`.toString(), 'test');
    assertEquals(getTrustedScriptURL`test`.toString(), 'test');

    assertEquals(
        getTrustedHTML`test1
        test2`.toString(),
        'test1\n        test2');
    assertEquals(
        getTrustedScript`test1
        test2`.toString(),
        'test1\n        test2');
  });

  test('throws when invalid', () => {
    function ensureThrows(arg: any) {
      assertThrows(() => {
        getTrustedHTML(arg);
      });
      assertThrows(() => {
        getTrustedScript(arg);
      });
      assertThrows(() => {
        getTrustedScriptURL(arg);
      });
    }

    const a = 'test';
    ensureThrows(a);

    const b = [a];
    ensureThrows(b);

    // c holds stringified value of `test`, which isn't a template literal.
    const c = `test`;
    ensureThrows(c);
  });
});
