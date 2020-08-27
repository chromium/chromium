// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allowedByDefault = '<a href="https://google.com">Google!</a>';
var text = 'I\'m just text, nobody should have a problem with me!';
var nonBreakingSpace = 'A\u00a0B\u00a0C';  // \u00a0 is a unicode nbsp.

function setUpPage() {
  loadTimeData.data = {
    'allowedByDefault': allowedByDefault,
    'customAttr': '<a is="action-link">Take action!</a>',
    'optionalTag': '<img>',
    'javascriptHref': '<a href="javascript:alert(1)">teh hax</a>',
    'script': '<script>alert(/xss/)</scr' +
        'ipt>',
    'text': text,
    'nonBreakingSpace': nonBreakingSpace,
  };
}

function testI18n() {
  assertEquals(text, I18nBehavior.i18n('text'));
  assertEquals(nonBreakingSpace, I18nBehavior.i18n('nonBreakingSpace'));

  assertThrows(function() {
    I18nBehavior.i18n('customAttr');
  });
  assertThrows(function() {
    I18nBehavior.i18n('optionalTag');
  });
  assertThrows(function() {
    I18nBehavior.i18n('javascriptHref');
  });
  assertThrows(function() {
    I18nBehavior.i18n('script');
  });
}

function testI18nAdvanced() {
  assertEquals(allowedByDefault, I18nBehavior.i18nAdvanced('allowedByDefault'));
  I18nBehavior.i18nAdvanced('customAttr', {attrs: ['is']});
  I18nBehavior.i18nAdvanced('optionalTag', {tags: ['img']});
}

function testI18nDynamic() {
  var locale = 'en';
  assertEquals(text, I18nBehavior.i18nDynamic(locale, 'text'));
}

function testI18nExists() {
  assertTrue(I18nBehavior.i18nExists('text'));
  assertFalse(I18nBehavior.i18nExists('missingText'));
}
