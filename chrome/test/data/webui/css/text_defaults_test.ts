// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('TextDefaults', function() {
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  function assertFontFamilyExists(link: HTMLLinkElement) {
    assertTrue(!!link.sheet);
    const styleRules =
        Array.from(link.sheet.cssRules)
            .filter(r => r instanceof CSSStyleRule) as CSSStyleRule[];
    assertTrue(styleRules.length > 0);
    const fontFamily = styleRules[0]!.style.getPropertyValue('font-family');
    assertNotEquals('', fontFamily);
  }

  test('text_defaults.css', function(done) {
    const link = document.createElement('link');
    link.rel = 'stylesheet';
    link.href = 'chrome://resources/css/text_defaults.css';
    link.onload = function() {
      assertFontFamilyExists(link);
      done();
    };
    document.body.appendChild(link);
  });

  test('text_defaults_md.css', function(done) {
    const link = document.createElement('link');
    link.rel = 'stylesheet';
    link.href = 'chrome://resources/css/text_defaults_md.css';
    link.onload = function() {
      assertFontFamilyExists(link);
      done();
    };
    document.body.appendChild(link);
  });
});
