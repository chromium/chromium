// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTrustedHTML, getTrustedScript, getTrustedScriptURL} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertNotReached, assertThrows, assertTrue} from 'chrome://webui-test/chai_assert.js';

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
    assertTrue(getTrustedHTML`test` instanceof window.TrustedHTML);
    assertTrue(getTrustedScript`test` instanceof window.TrustedScript);
    assertTrue(getTrustedScriptURL`test` instanceof window.TrustedScriptURL);
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
    function ensureThrows(arg: TemplateStringsArray) {
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

    // Casting since purposefully passing incorrect value.
    const a = 'test' as unknown as TemplateStringsArray;
    ensureThrows(a);

    // Casting since purposefully passing incorrect value.
    const b = [a] as unknown as TemplateStringsArray;
    ensureThrows(b);

    // c holds stringified value of `test`, which isn't a template literal.
    // Casting since purposefully passing incorrect value.
    const c = `test` as unknown as TemplateStringsArray;
    ensureThrows(c);
  });
});
