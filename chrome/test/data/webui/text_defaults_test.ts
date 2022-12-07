// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotEquals, assertTrue} from './chai_assert.js';

suite('TextDefaults', function() {
  test('text_defaults.css', function(done) {
    const link = document.createElement('link');
    link.rel = 'stylesheet';
    link.href = 'chrome://resources/css/text_defaults.css';
    link.onload = function() {
      assertTrue(!!link.sheet);
      const fontFamily = (link.sheet.rules[1] as CSSStyleRule)
                             .style.getPropertyValue('font-family');
      assertNotEquals('', fontFamily);
      done();
    };
    document.body.appendChild(link);
  });

  test('text_defaults_md.css', function(done) {
    const link = document.createElement('link');
    link.rel = 'stylesheet';
    link.href = 'chrome://resources/css/text_defaults_md.css';
    link.onload = function() {
      assertTrue(!!link.sheet);
      const fontFamily = (link.sheet.rules[2] as CSSStyleRule)
                             .style.getPropertyValue('font-family');
      assertNotEquals('', fontFamily);
      done();
    };
    document.body.appendChild(link);
  });
});
