// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {mojoString16ToString, stringToMojoString16, stringToMojoUrl} from 'chrome://resources/js/mojo_type_util.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('MojoTypeUtilTest', () => {
  test('Can convert strings to mojo String16s', () => {
    assertDeepEquals(stringToMojoString16(''), {data: []});
    assertDeepEquals(stringToMojoString16('hi'), {data: [0x68, 0x69]});
    assertDeepEquals(stringToMojoString16('你好'), {data: [0x4f60, 0x597d]});
  });

  test('Can convert mojo String16s to strings', () => {
    assertEquals(mojoString16ToString({data: []}), '');
    assertEquals(mojoString16ToString({data: [0x68, 0x69]}), 'hi');
    assertEquals(mojoString16ToString({data: [0x4f60, 0x597d]}), '你好');
  });

  test('Can convert strings to mojo Urls', () => {
    assertDeepEquals(stringToMojoUrl(''), {url: ''});
    assertDeepEquals(
        stringToMojoUrl('https://chromium.org'), {url: 'https://chromium.org'});
    assertDeepEquals(
        stringToMojoUrl('https://user:pass@example.test'),
        {url: 'https://user:pass@example.test'});
    assertDeepEquals(
        stringToMojoUrl('http://insecure.test'), {url: 'http://insecure.test'});
    assertDeepEquals(
        stringToMojoUrl('file:///home/test/test.pdf'),
        {url: 'file:///home/test/test.pdf'});
  });
});
