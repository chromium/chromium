// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testGetFaviconForPageURL() {
  var url = 'http://foo.com';
  function getExpectedImageSet(size) {
    var expectedDesktop = '-webkit-image-set(' +
        `url("chrome://favicon2/?size=${size}&scale_factor=1x&page_url=` +
        encodeURIComponent(url) + '&allow_google_server_fallback=0") 1x, ' +
        `url("chrome://favicon2/?size=${size}&scale_factor=2x&page_url=` +
        encodeURIComponent(url) + '&allow_google_server_fallback=0") 2x)';
    var expectedOther = '-webkit-image-set(' +
        `url("chrome://favicon2/?size=${size}&scale_factor=1x&page_url=` +
        encodeURIComponent(url) + '&allow_google_server_fallback=0") ' +
        window.devicePixelRatio + 'x)';

    var isDesktop =
        cr.isMac || cr.isChromeOS || cr.isWindows || cr.isLinux || cr.isLacros;
    return isDesktop ? expectedDesktop : expectedOther;
  }

  assertEquals(getExpectedImageSet(16), cr.icon.getFaviconForPageURL(url));
  assertEquals(
      getExpectedImageSet(24),
      cr.icon.getFaviconForPageURL(url, false, '', 24));
}

function testGetFavicon() {
  var url = 'http://foo.com/foo.ico';
  var expectedDesktop = '-webkit-image-set(' +
      'url("chrome://favicon2/?size=16&scale_factor=1x&icon_url=' +
      encodeURIComponent('http://foo.com/foo.ico') + '") 1x, ' +
      'url("chrome://favicon2/?size=16&scale_factor=2x&icon_url=' +
      encodeURIComponent('http://foo.com/foo.ico') + '") 2x)';
  var expectedOther = '-webkit-image-set(' +
      'url("chrome://favicon2/?size=16&scale_factor=1x&icon_url=' +
      encodeURIComponent('http://foo.com/foo.ico') + '") ' +
      window.devicePixelRatio + 'x)';

  var isDesktop =
      cr.isMac || cr.isChromeOS || cr.isWindows || cr.isLinux || cr.isLacros;
  var expected = isDesktop ? expectedDesktop : expectedOther;
  assertEquals(expected, cr.icon.getFavicon(url));
}

function testGetFileIconUrl() {
  assertEquals(
      cr.icon.getFileIconUrl('file path'),
      'chrome://fileicon/?path=file+path&scale=' + window.devicePixelRatio +
          'x');
}
