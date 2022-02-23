// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {string} html Text, possibly with HTML &entities; in it.
 * @return {string} The HTML decoded text.
 */
function decodeHtmlEntities(html) {
  const element = document.createElement('div');
  element.innerHTML = html;
  return element.textContent;
}

suite('TextDefaults', function() {
  test('text_defaults.css', function(done) {
    const link = document.createElement('link');
    link.rel = 'stylesheet';
    link.href = 'chrome://resources/css/text_defaults.css';
    link.onload = function() {
      const fontFamily = link.sheet.rules[1].style['font-family'];
      assertNotEquals('', fontFamily);
      assertEquals(decodeHtmlEntities(fontFamily), fontFamily);
      done();
    };
    document.body.appendChild(link);
  });

  test('text_defaults_md.css', function(done) {
    const link = document.createElement('link');
    link.rel = 'stylesheet';
    link.href = 'chrome://resources/css/text_defaults_md.css';
    link.onload = function() {
      const fontFamily = link.sheet.rules[2].style['font-family'];
      assertNotEquals('', fontFamily);
      assertEquals(decodeHtmlEntities(fontFamily), fontFamily);
      done();
    };
    document.body.appendChild(link);
  });
});
