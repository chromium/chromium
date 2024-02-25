// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import type {SiteFaviconElement} from 'chrome://settings/settings.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('SiteFavicon', function() {
  let siteFavicon: SiteFaviconElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    siteFavicon = document.createElement('site-favicon');
    document.body.appendChild(siteFavicon);
  });

  function assertIconEquals(expected: string) {
    const background = siteFavicon.$.favicon.style.backgroundImage;
    assertEquals(background, expected);
  }

  function formExpected(url: string): string {
    return 'image-set(' +
        'url("chrome://favicon2/?size=16&scaleFactor=1x&pageUrl=' +
        encodeURIComponent(url) + '&allowGoogleServerFallback=0") 1x, ' +
        'url("chrome://favicon2/?size=16&scaleFactor=2x&pageUrl=' +
        encodeURIComponent(url) + '&allowGoogleServerFallback=0") 2x)';
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
