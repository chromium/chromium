// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {findAncestor, quoteString} from 'chrome://resources/js/util.m.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('UtilModuleTest', function() {
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
  });

  test('findAncestor', function() {
    const option = document.createElement('option');
    option.value = 'success';

    const failure = document.createTextNode('is not an option');
    option.appendChild(failure);

    const select = document.createElement('select');
    select.appendChild(option);

    const div = document.createElement('div');
    const root = div.attachShadow({mode: 'open'});
    root.appendChild(select);

    assertEquals(findAncestor(failure, n => n.nodeName === 'SELECT'), select);

    // findAncestor() only traverses shadow roots (which |div| is outside of) if
    // |includeShadowHosts| is true. If omitted, |div| shouldn't be found.
    assertEquals(findAncestor(failure, n => n.nodeName === 'DIV'), null);
    assertEquals(
        findAncestor(
            failure, n => n.nodeName === 'DIV',
            /*includeShadowHosts=*/ true),
        div);
  });
});
