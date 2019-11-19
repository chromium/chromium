// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';

suite('I18nBehaviorModuleTest', function() {
  const allowedByDefault = '<a href="https://google.com">Google!</a>';
  const text = 'I\'m just text, nobody should have a problem with me!';
  const nonBreakingSpace = 'A\u00a0B\u00a0C';  // \u00a0 is a unicode nbsp.

  suiteSetup(function() {
    window.loadTimeData.data = {
      'allowedByDefault': allowedByDefault,
      'customAttr': '<a is="action-link">Take action!</a>',
      'customTag': '<x-foo>I\'m an X, foo!</x-foo>',
      'javascriptHref': '<a href="javascript:alert(1)">teh hax</a>',
      'script': '<script>alert(/xss/)</scr' +
          'ipt>',
      'text': text,
      'nonBreakingSpace': nonBreakingSpace,
    };
  });

  test('i18n', function() {
    assertEquals(text, I18nBehavior.i18n('text'));
    assertEquals(nonBreakingSpace, I18nBehavior.i18n('nonBreakingSpace'));

    assertThrows(function() {
      I18nBehavior.i18n('customAttr');
    });
    assertThrows(function() {
      I18nBehavior.i18n('customTag');
    });
    assertThrows(function() {
      I18nBehavior.i18n('javascriptHref');
    });
    assertThrows(function() {
      I18nBehavior.i18n('script');
    });
  });

  test('i18n advanced', function() {
    assertEquals(
        allowedByDefault, I18nBehavior.i18nAdvanced('allowedByDefault'));
    I18nBehavior.i18nAdvanced('customAttr', {
      attrs: {
        is: function(el, val) {
          return el.tagName == 'A' && val == 'action-link';
        },
      },
    });
    I18nBehavior.i18nAdvanced('customTag', {tags: ['X-FOO']});
  });

  test('i18n dynamic', function() {
    var locale = 'en';
    assertEquals(text, I18nBehavior.i18nDynamic(locale, 'text'));
  });

  test('i18n exists', function() {
    assertTrue(I18nBehavior.i18nExists('text'));
    assertFalse(I18nBehavior.i18nExists('missingText'));
  });
});
