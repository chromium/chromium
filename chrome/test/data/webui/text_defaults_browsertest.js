// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "content/public/test/browser_test.h"');

/**
 * Test fixture for testing async methods of cr.js.
 * @constructor
 * @extends testing.Test
 */
function TextDefaultsTest() {}

TextDefaultsTest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Must be on same domain as text_defaults.css (chrome://resources).
   * @override
   */
  browsePreload: 'chrome://resources/html/assert.html',

  /** @override */
  isAsync: true,
};

/**
 * @param {string} html Text, possibly with HTML &entities; in it.
 * @return {string} The HTML decoded text.
 */
function decodeHtmlEntities(html) {
  var element = document.createElement('div');
  element.innerHTML = html;
  return element.textContent;
}

TEST_F('TextDefaultsTest', 'ScrapeStyles', function() {
  var link = document.createElement('link');
  link.rel = 'stylesheet';
  link.href = 'chrome://resources/css/text_defaults.css';
  link.onload = function() {
    var fontFamily = link.sheet.rules[1].style['font-family'];
    assertNotEquals('', fontFamily);
    assertEquals(decodeHtmlEntities(fontFamily), fontFamily);
    testDone();
  };
  document.body.appendChild(link);
});

TEST_F('TextDefaultsTest', 'ScrapeMDStyles', function() {
  var link = document.createElement('link');
  link.rel = 'stylesheet';
  link.href = 'chrome://resources/css/text_defaults_md.css';
  link.onload = function() {
    var fontFamily = link.sheet.rules[2].style['font-family'];
    assertNotEquals('', fontFamily);
    assertEquals(decodeHtmlEntities(fontFamily), fontFamily);
    testDone();
  };
  document.body.appendChild(link);
});
