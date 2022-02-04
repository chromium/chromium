// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://extensions/extensions.js';

import {getFaviconUrl} from 'chrome://extensions/extensions.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('UrlUtilTest', function() {
  function getExpectedImageSet(url: string): string {
    return '-webkit-image-set(' +
        'url("chrome://favicon2/?size=20&scale_factor=1x&page_url=' +
        encodeURIComponent(url) + '&allow_google_server_fallback=0") 1x, ' +
        'url("chrome://favicon2/?size=20&scale_factor=2x&page_url=' +
        encodeURIComponent(url) + '&allow_google_server_fallback=0") 2x)';
  }

  test('favicon for normal URL', function() {
    const expectedUrl = getExpectedImageSet('http://www.google.com');
    assertEquals(expectedUrl, getFaviconUrl('http://www.google.com'));
  });

  test('favicon for URLs with wildcards', function() {
    const expectedUrl1 = getExpectedImageSet('http://*.google.com');
    const url1 = 'http://*.google.com/*';
    assertEquals(expectedUrl1, getFaviconUrl(url1));

    const expectedUrl2 = getExpectedImageSet('http://google.com');
    const url2 = '*://google.com';
    assertEquals(expectedUrl2, getFaviconUrl(url2));
  });
});
