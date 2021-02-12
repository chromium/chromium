// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FontsBrowserProxy, FontsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
// clang-format on

/** @implements {FontsBrowserProxy} */
class TestFontsBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'fetchFontsData',
    ]);

    /** @private {!FontsData} */
    this.fontsData_ = {
      'fontList': [['font name', 'alternate', 'ltr']],
      'encodingList': [['encoding name', 'alternate', 'ltr']],
    };
  }

  /** @override */
  fetchFontsData() {
    this.methodCalled('fetchFontsData');
    return Promise.resolve(this.fontsData_);
  }
}

let fontsPage = null;

/** @type {?TestFontsBrowserProxy} */
let fontsBrowserProxy = null;

suite('AppearanceFontHandler', function() {
  setup(function() {
    fontsBrowserProxy = new TestFontsBrowserProxy();
    FontsBrowserProxyImpl.instance_ = fontsBrowserProxy;

    PolymerTest.clearBody();

    fontsPage = document.createElement('settings-appearance-fonts-page');
    document.body.appendChild(fontsPage);
  });

  teardown(function() {
    fontsPage.remove();
  });

  test('fetchFontsData', function() {
    return fontsBrowserProxy.whenCalled('fetchFontsData');
  });

  test('minimum font size preview', async () => {
    fontsPage.prefs = {webkit: {webprefs: {minimum_font_size: {value: 0}}}};
    assertTrue(fontsPage.$.minimumSizeFontPreview.hidden);
    fontsPage.set('prefs.webkit.webprefs.minimum_font_size.value', 6);
    assertFalse(fontsPage.$.minimumSizeFontPreview.hidden);
    fontsPage.set('prefs.webkit.webprefs.minimum_font_size.value', 0);
    assertTrue(fontsPage.$.minimumSizeFontPreview.hidden);
  });

  test('font preview size', async () => {
    /**
     * @param {!HTMLElement} element
     * @param {number} expectedFontSize
     */
    function assertFontSize(element, expectedFontSize) {
      // Check that the font size is applied correctly.
      const {value, unit} = element.computedStyleMap().get('font-size');
      assertEquals('px', unit);
      assertEquals(expectedFontSize, value);
      // Check that the font size value is displayed correctly.
      assertTrue(element.textContent.trim().startsWith(expectedFontSize));
    }

    fontsPage.prefs = {
      webkit: {
        webprefs: {
          default_font_size: {value: 20},
          default_fixed_font_size: {value: 10},
        }
      }
    };

    assertFontSize(fontsPage.$.standardFontPreview, 20);
    assertFontSize(fontsPage.$.serifFontPreview, 20);
    assertFontSize(fontsPage.$.sansSerifFontPreview, 20);
    assertFontSize(fontsPage.$.fixedFontPreview, 10);
  });
});
