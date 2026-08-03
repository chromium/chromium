// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sanitizeTextForPaste, stripJavascriptSchemas} from '//resources/cr_components/searchbox/utils.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

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
});
