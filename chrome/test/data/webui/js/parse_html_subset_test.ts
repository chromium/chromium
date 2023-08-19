// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {parseHtmlSubset, sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {assertEquals, assertFalse, assertThrows} from 'chrome://webui-test/chai_assert.js';

declare global {
  interface Window {
    called: boolean;
  }
}

suite('ParseHtmlSubsetTest', function() {
  function parseAndAssertThrows(
      s: string, extraTags?: string[], extraAttrs?: string[]) {
    assertThrows(function() {
      parseHtmlSubset(s, extraTags, extraAttrs);
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
    parseHtmlSubset('<PRE>pre</PRE><BR>');
    parseHtmlSubset('Some <PRE>pre</PRE><BR> text', ['BR']);
  });

  test('invalid tags', function() {
    parseAndAssertThrows('<unknown_tag>x</unknown_tag>');
    parseAndAssertThrows('<style>*{color:red;}</style>');
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
    const df = parseHtmlSubset(
        '<a href="https://google.com" target="_blank">Google</a>');
    assertEquals('_blank', (df.firstChild as HTMLAnchorElement).target);
  });

  test('invalid target', function() {
    parseAndAssertThrows(
        '<a href="https://google.com" target="foo">Google</a>');
  });

  test('supported optional tags', function() {
    parseHtmlSubset('<img>Some <b>bold</b> text', ['img']);
    parseHtmlSubset('A list:<ul><li>An item</li></ul>', ['li', 'ul']);
  });

  test('supported optional tags without the argument', function() {
    parseAndAssertThrows('<img>');
  });

  test('invalid optional tags', function() {
    parseAndAssertThrows('a pirate\'s<script>alert();</script>', ['script']);
  });

  test('supported optional attributes', function() {
    let result =
        parseHtmlSubset('<a role="link">link</a>', undefined, ['role']);
    assertEquals(
        'link', (result.firstChild as HTMLAnchorElement).getAttribute('role'));
    result =
        parseHtmlSubset('<img src="chrome://favicon2/">', ['img'], ['src']);
    assertEquals(
        'chrome://favicon2/',
        (result.firstChild as HTMLAnchorElement).getAttribute('src'));
  });

  test('supported optional attributes without the argument', function() {
    parseAndAssertThrows('<img src="chrome://favicon2/">', ['img']);
    parseAndAssertThrows('<a id="test">link</a>');
  });

  test('invalid optional attributes', function() {
    parseAndAssertThrows(
        '<a test="fancy">I\'m fancy!</a>', undefined, ['test']);
    parseAndAssertThrows('<a name="fancy">I\'m fancy!</a>');
  });

  test('invalid optional attribute\'s value', function() {
    parseAndAssertThrows('<a is="xss-link">link</a>', undefined, ['is']);
  });

  test('sanitizeInnerHtml', function() {
    assertEquals(
        '<a href="chrome://foo"></a>',
        sanitizeInnerHtml('<a href="chrome://foo"></a>').toString());
    assertThrows(() => {
      sanitizeInnerHtml('<iframe></iframe>');
    }, 'IFRAME is not supported');
    assertEquals('<div></div>', sanitizeInnerHtml('<div></div>').toString());
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
