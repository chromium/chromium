// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isChromeOS, isLacros, isLinux, isMac, isWindows} from 'chrome://resources/js/cr.m.js';
import {getFavicon, getFaviconForPageURL, getFileIconUrl} from 'chrome://resources/js/icon.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('IconModuleTest', function() {
  test('GetFaviconForPageURL', function() {
    const url = 'http://foo.com';

    function getExpectedImageSet(size: number): string {
      const expectedDesktop = '-webkit-image-set(' +
          `url("chrome://favicon2/?size=${size}&scaleFactor=1x&pageUrl=` +
          encodeURIComponent(url) + '&allowGoogleServerFallback=0") 1x, ' +
          `url("chrome://favicon2/?size=${size}&scaleFactor=2x&pageUrl=` +
          encodeURIComponent(url) + '&allowGoogleServerFallback=0") 2x)';
      const expectedOther = '-webkit-image-set(' +
          `url("chrome://favicon2/?size=${size}&scaleFactor=1x&pageUrl=` +
          encodeURIComponent(url) + '&allowGoogleServerFallback=0") ' +
          window.devicePixelRatio + 'x)';

      const isDesktop = isMac || isChromeOS || isWindows || isLinux || isLacros;
      return isDesktop ? expectedDesktop : expectedOther;
    }

    assertEquals(getExpectedImageSet(16), getFaviconForPageURL(url, false));
    assertEquals(
        getExpectedImageSet(24), getFaviconForPageURL(url, false, '', 24));
  });

  test('GetFavicon', function() {
    const url = 'http://foo.com/foo.ico';
    const expectedDesktop = '-webkit-image-set(' +
        'url("chrome://favicon2/?size=16&scaleFactor=1x&iconUrl=' +
        encodeURIComponent('http://foo.com/foo.ico') + '") 1x, ' +
        'url("chrome://favicon2/?size=16&scaleFactor=2x&iconUrl=' +
        encodeURIComponent('http://foo.com/foo.ico') + '") 2x)';
    const expectedOther = '-webkit-image-set(' +
        'url("chrome://favicon2/?size=16&scaleFactor=1x&iconUrl=' +
        encodeURIComponent('http://foo.com/foo.ico') + '") ' +
        window.devicePixelRatio + 'x)';

    const isDesktop = isMac || isChromeOS || isWindows || isLinux || isLacros;
    const expected = isDesktop ? expectedDesktop : expectedOther;
    assertEquals(expected, getFavicon(url));
  });

  test('GetFileIconUrl', function() {
    assertEquals(
        getFileIconUrl('file path'),
        'chrome://fileicon/?path=file+path&scale=' + window.devicePixelRatio +
            'x');
  });
});
