// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {String16Converter} from 'chrome://resources/mojo/mojo/public/mojom/base/string16_converter.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('MojoTypeUtilTest', () => {
  test('Can convert strings to mojo String16s', () => {
    const converter = new String16Converter();
    assertDeepEquals(converter.data(''), []);
    assertDeepEquals(converter.data('hi'), [0x68, 0x69]);
    assertDeepEquals(converter.data('你好'), [0x4f60, 0x597d]);
  });

  test('mojoString16ToString_NoChunking', () => {
    const converter = new String16Converter();
    assertEquals(converter.convertImpl([]), '');
    assertEquals(converter.convertImpl([0x68, 0x69]), 'hi');
    assertEquals(converter.convertImpl([0x4f60, 0x597d]), '你好');
  });

  test('mojoString16ToString_WithChunking', () => {
    const converter = new String16Converter();
    assertEquals(
        'h'.repeat(9000), converter.convertImpl(Array(9000).fill(0x68)));
    assertEquals(
        'h'.repeat(18000),
        converter.convertImpl(Array(18000).fill(0x68)));
    assertEquals(
        'h'.repeat(1e6), converter.convertImpl(Array(1e6).fill(0x68)));
  });

  test('emojis', () => {
    const converter = new String16Converter();
    assertEquals('❤️', converter.convertImpl(converter.data('❤️')));
    assertEquals(
        '👨‍👨‍👦',
        converter.convertImpl(converter.data('👨‍👨‍👦')));
    assertEquals('🇯🇵', converter.convertImpl(converter.data('🇯🇵')));
    assertEquals('🇺🇳', converter.convertImpl(converter.data('🇺🇳')));
    assertEquals(
        '👨‍👨‍👦🇯🇵👨‍👨‍👦a你❤️👨‍👨‍👦',
        converter.convertImpl(converter.data(
            '👨‍👨‍👦🇯🇵👨‍👨‍👦a你❤️👨‍👨‍👦')));
  });

  test('mojoString16ToString_WithChunking_Boundaries', () => {
    const converter = new String16Converter();
    // Length of emoji flag = 4.
    // Adding characters at the beginning offsets it relative to the chunk size
    // which is 2^13.
    let s = '🇺🇳'.repeat(9000);
    assertEquals(s, (converter.convertImpl(converter.data(s))));
    s = 'h' +
        '🇺🇳'.repeat(9000);
    assertEquals(s, converter.convertImpl(converter.data(s)));
    s = 'hh' +
        '🇺🇳'.repeat(9000);
    assertEquals(s, converter.convertImpl(converter.data(s)));
    s = 'hhh' +
        '🇺🇳'.repeat(9000);
    assertEquals(s, converter.convertImpl(converter.data(s)));
  });
});
