// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {announce, markOnce, sanitizeTextForPaste, stripJavascriptSchemas} from '//resources/cr_components/searchbox/utils.js';
import type {AriaNotificationOptions} from '//resources/cr_components/searchbox/utils.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('SearchboxUtilsTest', () => {
  suite('stripJavascriptSchemas', () => {
    test('no javascript schema', () => {
      assertEquals('', stripJavascriptSchemas(''));
      assertEquals('hello world', stripJavascriptSchemas('hello world'));
      assertEquals(
          'https://google.com', stripJavascriptSchemas('https://google.com'));
      assertEquals('javax:alert(1)', stripJavascriptSchemas('javax:alert(1)'));
    });

    test('simple javascript schema', () => {
      assertEquals('alert(1)', stripJavascriptSchemas('javascript:alert(1)'));
      assertEquals('alert(1)', stripJavascriptSchemas('JavaScript:alert(1)'));
      assertEquals('alert(1)', stripJavascriptSchemas('JAVAscript:alert(1)'));
    });

    test('whitespace and control characters before schema', () => {
      assertEquals('alert(1)', stripJavascriptSchemas('  javascript:alert(1)'));
      assertEquals(
          'alert(1)', stripJavascriptSchemas('\t\n javascript:alert(1)'));
      assertEquals(
          'alert(1)', stripJavascriptSchemas('\x01\x1fjavascript:alert(1)'));
    });

    test('nested javascript schemas', () => {
      assertEquals(
          'alert(1)', stripJavascriptSchemas('javascript:javascript:alert(1)'));
      assertEquals(
          'alert(1)',
          stripJavascriptSchemas('javascript:  javascript:alert(1)'));
      assertEquals(
          'alert(1)',
          stripJavascriptSchemas('javascript:\x01javascript:alert(1)'));
    });
  });

  suite('sanitizeTextForPaste', () => {
    test('empty and whitespace only', () => {
      assertEquals('', sanitizeTextForPaste(''));
      assertEquals(' ', sanitizeTextForPaste(' '));
      assertEquals(' ', sanitizeTextForPaste('   '));
      assertEquals(' ', sanitizeTextForPaste('\t\n '));
    });

    test('basic string sanitization', () => {
      assertEquals('hello world', sanitizeTextForPaste('hello world'));
      assertEquals('hello world', sanitizeTextForPaste('  hello world  '));
      assertEquals('hello   world', sanitizeTextForPaste('hello   world'));
    });

    test('newline and carriage return handling', () => {
      // Without non-LF whitespace: newlines removed
      assertEquals('helloworld', sanitizeTextForPaste('hello\nworld'));
      assertEquals('helloworld', sanitizeTextForPaste('hello\r\nworld'));

      // With non-LF whitespace: newlines converted to single space
      assertEquals(
          'hello world foo bar', sanitizeTextForPaste('hello world\nfoo bar'));
      assertEquals(
          'hello world foo bar',
          sanitizeTextForPaste('hello world\r\nfoo bar'));
    });

    test('javascript schema stripping on paste', () => {
      assertEquals('alert(1)', sanitizeTextForPaste('javascript:alert(1)'));
      assertEquals(
          'alert(1)',
          sanitizeTextForPaste('  javascript:javascript:alert(1)\n'));
    });
  });

  suite('markOnce', () => {
    teardown(() => {
      performance.clearMarks();
    });

    test('logs multiple independent marks once each', () => {
      const markA = 'test-mark-a';
      const markB = 'test-mark-b';

      assertTrue(markOnce(markA));
      assertTrue(markOnce(markB));
      assertFalse(markOnce(markA));
      assertFalse(markOnce(markB));

      assertEquals(1, performance.getEntriesByName(markA).length);
      assertEquals(1, performance.getEntriesByName(markB).length);
    });
  });

  suite('announce', () => {
    let testEl: HTMLElement;

    setup(() => {
      testEl = document.createElement('div');
      document.body.appendChild(testEl);
    });

    teardown(() => {
      testEl.remove();
    });

    test('calls ariaNotify with high priority when available', () => {
      const calls: Array<{message: string, options?: AriaNotificationOptions}> =
          [];
      testEl.ariaNotify =
          (message: string, options: AriaNotificationOptions) => {
            calls.push({message, options});
          };

      announce(testEl, 'Hello world');
      assertEquals(1, calls.length);
      assertEquals('Hello world', calls[0]!.message);
      assertEquals('high', calls[0]!.options?.priority);
    });

    test('does not announce empty messages', () => {
      let called = false;
      testEl.ariaNotify = () => {
        called = true;
      };

      announce(testEl, '');
      assertFalse(called);
    });

    test(
        'falls back to cr-a11y-announcer when ariaNotify is unavailable',
        async () => {
          Object.defineProperty(
              testEl, 'ariaNotify', {value: undefined, configurable: true});
          const announcementPromise =
              eventToPromise('cr-a11y-announcer-messages-sent', document.body);

          announce(testEl, 'Fallback announcement');
          const event =
              await announcementPromise as CustomEvent<{messages: string[]}>;
          assertTrue(event.detail.messages.includes('Fallback announcement'));
        });
  });
});
