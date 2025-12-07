// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFavicon, getFaviconForPageURL, getFaviconUrl, type GetFaviconUrlParams, getFileIconUrl} from 'chrome://resources/js/icon.js';
import {isAndroid, isChromeOS, isLinux, isMac, isWindows} from 'chrome://resources/js/platform.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('IconTest', function() {
  for (const isSyncedUrlForHistoryUi of [true, false]) {
    test(
        `GetFaviconUrl_isSyncedUrlForHistoryUi_${isSyncedUrlForHistoryUi}`,
        () => {
          const pageUrl = 'http://www.example.com/';

          const faviconUrl =
              new URL(getFaviconUrl(pageUrl, {isSyncedUrlForHistoryUi}));
          assertEquals(faviconUrl.searchParams.get('size'), '16');
          assertEquals(faviconUrl.searchParams.get('pageUrl'), pageUrl);
          assertEquals(
              faviconUrl.searchParams.get('allowGoogleServerFallback'),
              isSyncedUrlForHistoryUi ? '1' : '0');
          if (isSyncedUrlForHistoryUi) {
            assertEquals(faviconUrl.searchParams.get('iconUrl'), '');
          } else {
            assertFalse(faviconUrl.searchParams.has('iconUrl'));
          }
          assertFalse(faviconUrl.searchParams.has('forceLightMode'));
          assertFalse(faviconUrl.searchParams.has('fallbackToHost'));
          assertFalse(faviconUrl.searchParams.has('forceEmptyDefaultFavicon'));
          assertEquals(
              faviconUrl.searchParams.get('scaleFactor'), 'SCALEFACTORx');
        });
  }

  test('GetFaviconUrlWithOptionalParams', () => {
    function getFaviconUrlWithParams(params: GetFaviconUrlParams): URL {
      return new URL(getFaviconUrl(pageUrl, params));
    }

    const pageUrl = 'http://www.example.com/';

    let faviconUrl = getFaviconUrlWithParams({isSyncedUrlForHistoryUi: false});
    assertEquals(faviconUrl.searchParams.get('allowGoogleServerFallback'), '0');
    assertFalse(faviconUrl.searchParams.has('iconUrl'));
    faviconUrl = getFaviconUrlWithParams({isSyncedUrlForHistoryUi: true});
    assertEquals(faviconUrl.searchParams.get('allowGoogleServerFallback'), '1');
    assertEquals(faviconUrl.searchParams.get('iconUrl'), '');

    faviconUrl = getFaviconUrlWithParams({size: 20});
    assertEquals(faviconUrl.searchParams.get('size'), '20');

    faviconUrl = getFaviconUrlWithParams(
        {isSyncedUrlForHistoryUi: true, remoteIconUrlForUma: 'foo'});
    assertEquals(faviconUrl.searchParams.get('allowGoogleServerFallback'), '1');
    assertEquals(faviconUrl.searchParams.get('iconUrl'), 'foo');

    faviconUrl = getFaviconUrlWithParams({forceLightMode: false});
    assertFalse(faviconUrl.searchParams.has('forceLightMode'));
    faviconUrl = getFaviconUrlWithParams({forceLightMode: true});
    assertEquals(faviconUrl.searchParams.get('forceLightMode'), 'true');

    faviconUrl = getFaviconUrlWithParams({fallbackToHost: false});
    assertEquals(faviconUrl.searchParams.get('fallbackToHost'), '0');
    faviconUrl = getFaviconUrlWithParams({fallbackToHost: true});
    assertFalse(faviconUrl.searchParams.has('fallbackToHost'));

    faviconUrl = getFaviconUrlWithParams({forceEmptyDefaultFavicon: false});
    assertFalse(faviconUrl.searchParams.has('forceEmptyDefaultFavicon'));
    faviconUrl = getFaviconUrlWithParams({forceEmptyDefaultFavicon: true});
    assertEquals(faviconUrl.searchParams.get('forceEmptyDefaultFavicon'), '1');

    faviconUrl = getFaviconUrlWithParams({scaleFactor: '2x'});
    assertEquals(faviconUrl.searchParams.get('scaleFactor'), '2x');
  });

  test('GetFaviconForPageURL', function() {
    const url = 'http://foo.com';

    function getExpectedImageSet(size: number): string {
      const expectedDesktop = 'image-set(' +
          `url("chrome://favicon2/?size=${size}&scaleFactor=1x&pageUrl=` +
          encodeURIComponent(url) + '&allowGoogleServerFallback=0") 1x, ' +
          `url("chrome://favicon2/?size=${size}&scaleFactor=2x&pageUrl=` +
          encodeURIComponent(url) + '&allowGoogleServerFallback=0") 2x)';
      const expectedOther = 'image-set(' +
          `url("chrome://favicon2/?size=${size}&scaleFactor=1x&pageUrl=` +
          encodeURIComponent(url) + '&allowGoogleServerFallback=0") 1x, ' +
          `url("chrome://favicon2/?size=${size}&` +
          `scaleFactor=${window.devicePixelRatio}x&pageUrl=` +
          encodeURIComponent(url) + '&allowGoogleServerFallback=0") ' +
          `${window.devicePixelRatio}x)`;

      // Android simulator returns true for isLinux, so also check isAndroid.
      const isDesktop =
          (isMac || isChromeOS || isWindows || isLinux) && !isAndroid;
      return isDesktop ? expectedDesktop : expectedOther;
    }

    assertEquals(getExpectedImageSet(16), getFaviconForPageURL(url, false));
    assertEquals(
        getExpectedImageSet(24), getFaviconForPageURL(url, false, '', 24));
  });

  test('GetFaviconForPageURL_ForceLightMode', () => {
    const url = 'http://foo.com';
    assertFalse(
        getFaviconForPageURL(url, false, '', 16).includes('forceLightMode'));
    assertFalse(
        getFaviconForPageURL(url, false, '', 16, /* forceLightMode */ false)
            .includes('forceLightMode'));
    assertTrue(
        getFaviconForPageURL(url, false, '', 16, /* forceLightMode */ true)
            .includes('forceLightMode=true'));
  });

  test('GetFavicon', function() {
    const url = 'http://foo.com/foo.ico';
    const expectedDesktop = 'image-set(' +
        'url("chrome://favicon2/?size=16&scaleFactor=1x&iconUrl=' +
        encodeURIComponent('http://foo.com/foo.ico') + '") 1x, ' +
        'url("chrome://favicon2/?size=16&scaleFactor=2x&iconUrl=' +
        encodeURIComponent('http://foo.com/foo.ico') + '") 2x)';
    const expectedOther = 'image-set(' +
        'url("chrome://favicon2/?size=16&scaleFactor=1x&iconUrl=' +
        encodeURIComponent('http://foo.com/foo.ico') + '") 1x, ' +
        'url("chrome://favicon2/?size=16&' +
        `scaleFactor=${window.devicePixelRatio}x&iconUrl=` +
        encodeURIComponent('http://foo.com/foo.ico') + '") ' +
        `${window.devicePixelRatio}x)`;

    // Android simulator returns true for isLinux, so also check isAndroid.
    const isDesktop =
        (isMac || isChromeOS || isWindows || isLinux) && !isAndroid;
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
