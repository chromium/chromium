// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {parseHtmlSubset} from 'chrome://resources/js/parse_html_subset.m.js';

suite('ParseHtmlSubsetModuleTest', function() {
  function parseAndAssertThrows() {
    var args = arguments;
    assertThrows(function() {
      parseHtmlSubset.apply(null, args);
    });
  }

  test('text', function() {
    parseHtmlSubset('');
    parseHtmlSubset('abc');
    parseHtmlSubset('&nbsp;');
  });

  test('supported tags', function() {
    parseHtmlSubset('<b>bold</b>');
    parseHtmlSubset('Some <b>bold</b> text');
    parseHtmlSubset('Some <strong>strong</strong> text');
    parseHtmlSubset('<B>bold</B>');
    parseHtmlSubset('Some <B>bold</B> text');
    parseHtmlSubset('Some <STRONG>strong</STRONG> text');
  });

  test('invalid tags', function() {
    parseAndAssertThrows('<unknown_tag>x</unknown_tag>');
    parseAndAssertThrows('<img>');
    parseAndAssertThrows(
        '<script>alert(1)<' +
        '/script>');
  });

  test('invalid attributes', function() {
    parseAndAssertThrows('<b onclick="alert(1)">x</b>');
    parseAndAssertThrows('<b style="color:red">x</b>');
    parseAndAssertThrows('<b foo>x</b>');
    parseAndAssertThrows('<b foo=bar></b>');
  });

  test('valid anchors', function() {
    parseHtmlSubset('<a href="https://google.com">Google</a>');
    parseHtmlSubset('<a href="chrome://settings">Google</a>');
  });

  test('invalid anchor hrefs', function() {
    parseAndAssertThrows('<a href="http://google.com">Google</a>');
    parseAndAssertThrows('<a href="ftp://google.com">Google</a>');
    parseAndAssertThrows('<a href="http/google.com">Google</a>');
    parseAndAssertThrows('<a href="javascript:alert(1)">Google</a>');
    parseAndAssertThrows(
        '<a href="chrome-extension://whurblegarble">Google</a>');
  });

  test('invalid anchor attributes', function() {
    parseAndAssertThrows('<a name=foo>Google</a>');
    parseAndAssertThrows(
        '<a onclick="alert(1)" href="https://google.com">Google</a>');
    parseAndAssertThrows(
        '<a foo="bar(1)" href="https://google.com">Google</a>');
  });

  test('anchor target', function() {
    var df = parseHtmlSubset(
        '<a href="https://google.com" target="_blank">Google</a>');
    assertEquals('_blank', df.firstChild.target);
  });

  test('invalid target', function() {
    parseAndAssertThrows('<form target="_evil">', ['form']);
    parseAndAssertThrows('<iframe target="_evil">', ['iframe']);
    parseAndAssertThrows(
        '<a href="https://google.com" target="foo">Google</a>');
  });

  test('custom tags', function() {
    parseHtmlSubset('yo <I>ho</i><bR>yo <EM>ho</em>', ['i', 'EM', 'Br']);
  });

  test('invalid custom tags', function() {
    parseAndAssertThrows(
        'a pirate\'s<script>lifeForMe();<' +
            '/script>',
        ['br']);
  });

  test('custom attributes', function() {
    const returnsTruthy = function(node, value) {
      assertEquals('A', node.tagName);
      assertEquals('fancy', value);
      return true;
    };
    parseHtmlSubset(
        '<a class="fancy">I\'m fancy!</a>', null, {class: returnsTruthy});
  });

  test('invalid custom attributes', function() {
    const returnsFalsey = function() {
      return false;
    };
    parseAndAssertThrows(
        '<a class="fancy">I\'m fancy!</a>', null, {class: returnsFalsey});
    parseAndAssertThrows('<a class="fancy">I\'m fancy!</a>');
  });

  test('on error async', function(done) {
    window.called = false;

    parseAndAssertThrows('<img onerror="window.called = true" src="_.png">');
    parseAndAssertThrows('<img src="_.png" onerror="window.called = true">');

    window.setTimeout(function() {
      assertFalse(window.called);
      done();
    });
  });
});
