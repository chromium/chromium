// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$, listenForPrivilegedLinkClicks, quoteString} from 'chrome://resources/js/util.m.js';

suite('UtilModuleTest', function() {
  test('quote string', function() {
    // Basic cases.
    assertEquals('\"test\"', quoteString('"test"'));
    assertEquals('\\!\\?', quoteString('!?'));
    assertEquals(
        '\\(\\._\\.\\) \\( \\:l \\) \\(\\.-\\.\\)',
        quoteString('(._.) ( :l ) (.-.)'));

    // Using the output as a regex.
    var re = new RegExp(quoteString('"hello"'), 'gim');
    var match = re.exec('She said "Hello" loudly');
    assertEquals(9, match.index);

    re = new RegExp(quoteString('Hello, .*'), 'gim');
    match = re.exec('Hello, world');
    assertEquals(null, match);
  });

  test('click handler', function() {
    listenForPrivilegedLinkClicks();
    document.body.innerHTML = `
      <a id="file" href="file:///path/to/file">File</a>
      <a id="chrome" href="about:chrome">Chrome</a>
      <a href="about:blank"><b id="blank">Click me</b></a>
    `;

    var clickArgs = null;
    var oldSend = chrome.send;
    chrome.send = function(message, args) {
      assertEquals('navigateToUrl', message);
      clickArgs = args;
    };
    $('file').click();
    assertEquals('file:///path/to/file', clickArgs[0]);
    $('chrome').click();
    assertEquals('about:chrome', clickArgs[0]);
    $('blank').click();
    assertEquals('about:blank', clickArgs[0]);
    chrome.send = oldSend;
  });
});
