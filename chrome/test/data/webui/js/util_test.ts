// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {quoteString as quoteStringJs} from 'chrome://resources/js/util_ts.js';
import {$, getRequiredElement, quoteString} from 'chrome://resources/js/util_ts.js';
import {assertEquals, assertThrows, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('UtilModuleTest', function() {
  test('get elements', function() {
    const element = document.createElement('div');
    element.id = 'foo';
    document.body.appendChild(element);
    assertEquals(element, $('foo'));
    assertEquals(element, getRequiredElement('foo'));

    // $ should not throw if the element does not exist.
    assertEquals(null, $('bar'));

    // getRequiredElement should throw.
    assertThrows(() => getRequiredElement('bar'));
  });

  test('quote string', function() {
    // Basic cases.
    assertEquals('\"test\"', quoteString('"test"'));
    assertEquals('\\!\\?', quoteString('!?'));
    assertEquals(
        '\\(\\._\\.\\) \\( \\:l \\) \\(\\.-\\.\\)',
        quoteString('(._.) ( :l ) (.-.)'));

    // Using the output as a regex.
    let re = new RegExp(quoteString('"hello"'), 'gim');
    let match = re.exec('She said "Hello" loudly');
    assertTrue(!!match);
    assertEquals(9, match.index);

    re = new RegExp(quoteString('Hello, .*'), 'gim');
    match = re.exec('Hello, world');
    assertEquals(null, match);

    // JS version
    // Basic cases.
    assertEquals('\"test\"', quoteStringJs('"test"'));
    assertEquals('\\!\\?', quoteStringJs('!?'));
    assertEquals(
        '\\(\\._\\.\\) \\( \\:l \\) \\(\\.-\\.\\)',
        quoteStringJs('(._.) ( :l ) (.-.)'));

    // Using the output as a regex.
    re = new RegExp(quoteStringJs('"hello"'), 'gim');
    match = re.exec('She said "Hello" loudly');
    assertTrue(!!match);
    assertEquals(9, match.index);

    re = new RegExp(quoteStringJs('Hello, .*'), 'gim');
    match = re.exec('Hello, world');
    assertEquals(null, match);
  });
});
