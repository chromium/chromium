// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function parseAndAssertThrows() {
  var args = arguments;
  assertThrows(function() {
    parseHtmlSubset.apply(null, args);
  });
}

function testText() {
  parseHtmlSubset('');
  parseHtmlSubset('abc');
  parseHtmlSubset('&nbsp;');
}

function testSupportedTags() {
  parseHtmlSubset('<b>bold</b>');
  parseHtmlSubset('Some <b>bold</b> text');
  parseHtmlSubset('Some <strong>strong</strong> text');
  parseHtmlSubset('<B>bold</B>');
  parseHtmlSubset('Some <B>bold</B> text');
  parseHtmlSubset('Some <STRONG>strong</STRONG> text');
}

function testInvalidTags() {
  parseAndAssertThrows('<unknown_tag>x</unknown_tag>');
  parseAndAssertThrows('<style>*{color:red;}</style>');
  parseAndAssertThrows(
      '<script>alert(1)<' +
      '/script>');
}

function testInvalidAttributes() {
  parseAndAssertThrows('<b onclick="alert(1)">x</b>');
  parseAndAssertThrows('<b style="color:red">x</b>');
  parseAndAssertThrows('<b foo>x</b>');
  parseAndAssertThrows('<b foo=bar></b>');
}

function testValidAnchors() {
  parseHtmlSubset('<a href="https://google.com">Google</a>');
  parseHtmlSubset('<a href="chrome://settings">Google</a>');
}

function testInvalidAnchorHrefs() {
  parseAndAssertThrows('<a href="http://google.com">Google</a>');
  parseAndAssertThrows('<a href="ftp://google.com">Google</a>');
  parseAndAssertThrows('<a href="http/google.com">Google</a>');
  parseAndAssertThrows('<a href="javascript:alert(1)">Google</a>');
  parseAndAssertThrows('<a href="chrome-extension://whurblegarble">Google</a>');
}

function testInvalidAnchorAttributes() {
  parseAndAssertThrows('<a name=foo>Google</a>');
  parseAndAssertThrows(
      '<a onclick="alert(1)" href="https://google.com">Google</a>');
  parseAndAssertThrows('<a foo="bar(1)" href="https://google.com">Google</a>');
}

function testAnchorTarget() {
  var df = parseHtmlSubset(
      '<a href="https://google.com" target="_blank">Google</a>');
  assertEquals('_blank', df.firstChild.target);
}

function testInvalidTarget() {
  parseAndAssertThrows('<a href="https://google.com" target="foo">Google</a>');
}

function testCustomTags() {
  parseHtmlSubset('<img>', ['IMG']);
}

function testInvalidCustomTags() {
  parseAndAssertThrows(
      'a pirate\'s<script>alert();<' +
          '/script>',
      ['script']);
}

function testCustomAttributes() {
  parseHtmlSubset('<a class="fancy">I\'m fancy!</a>', null, ['class']);
}

function testInvalidCustomAttributes() {
  parseAndAssertThrows('<a class="fancy">I\'m fancy!</a>');
}

function testOnErrorAsync(testDoneCallback) {
  window.called = false;

  parseAndAssertThrows('<img onerror="window.called = true" src="_.png">');
  parseAndAssertThrows('<img src="_.png" onerror="window.called = true">');

  window.setTimeout(function() {
    assertFalse(window.called);
    testDoneCallback();
  });
}
