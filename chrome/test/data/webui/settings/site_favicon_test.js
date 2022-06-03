// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

suite('SiteFavicon', function() {
  let siteFavicon;

  setup(function() {
    PolymerTest.clearBody();
    siteFavicon = document.createElement('site-favicon');
    document.body.appendChild(siteFavicon);
  });

  function assertIconEquals(expected) {
    const background = siteFavicon.$.favicon.style.backgroundImage;
    assertEquals(background, expected);
  }

  function formExpected(url) {
    return '-webkit-image-set(' +
        'url("chrome://favicon2/?size=16&scale_factor=1x&page_url=' +
        encodeURIComponent(url) + '&allow_google_server_fallback=0") 1x, ' +
        'url("chrome://favicon2/?size=16&scale_factor=2x&page_url=' +
        encodeURIComponent(url) + '&allow_google_server_fallback=0") 2x)';
  }

  test('normal URL', function() {
    const url = 'http://www.google.com';
    siteFavicon.url = url;
    assertIconEquals(formExpected(url));
  });

  test('URL without scheme', function() {
    const url = 'www.google.com';
    siteFavicon.url = url;
    assertIconEquals(formExpected(`http://${url}`));
  });

  test('URLs with wildcard', function() {
    const url1 = 'http://[*.]google.com';
    siteFavicon.url = url1;
    assertIconEquals(formExpected('http://google.com'));

    const url2 = 'https://[*.]google.com';
    siteFavicon.url = url2;
    assertIconEquals(formExpected('https://google.com'));

    const url3 = '[*.]google.com';
    siteFavicon.url = url3;
    assertIconEquals(formExpected('http://google.com'));
  });
});
