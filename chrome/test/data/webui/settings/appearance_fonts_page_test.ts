// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import { FontsBrowserProxy, FontsBrowserProxyImpl, FontsData,SettingsAppearanceFontsPageElement} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

// clang-format on

class TestFontsBrowserProxy extends TestBrowserProxy implements
    FontsBrowserProxy {
  private fontsData_: FontsData;

  constructor() {
    super([
      'fetchFontsData',
    ]);

    this.fontsData_ = {
      'fontList': [['font name', 'alternate', 'ltr']],
    };
  }

  fetchFontsData() {
    this.methodCalled('fetchFontsData');
    return Promise.resolve(this.fontsData_);
  }
}

let fontsPage: SettingsAppearanceFontsPageElement;
let fontsBrowserProxy: TestFontsBrowserProxy;

suite('AppearanceFontHandler', function() {
  setup(function() {
    fontsBrowserProxy = new TestFontsBrowserProxy();
    FontsBrowserProxyImpl.setInstance(fontsBrowserProxy);

    document.body.innerHTML = '';

    fontsPage = document.createElement('settings-appearance-fonts-page');
    document.body.appendChild(fontsPage);
  });

  teardown(function() {
    fontsPage.remove();
  });

  test('fetchFontsData', function() {
    return fontsBrowserProxy.whenCalled('fetchFontsData');
  });

  test('minimum font size preview', () => {
    fontsPage.prefs = {
      webkit: {
        webprefs: {
          minimum_font_size:
              {value: 0, type: chrome.settingsPrivate.PrefType.NUMBER}
        }
      }
    };
    assertTrue(fontsPage.$.minimumSizeFontPreview.hidden);
    fontsPage.set('prefs.webkit.webprefs.minimum_font_size.value', 6);
    assertFalse(fontsPage.$.minimumSizeFontPreview.hidden);
    fontsPage.set('prefs.webkit.webprefs.minimum_font_size.value', 0);
    assertTrue(fontsPage.$.minimumSizeFontPreview.hidden);
  });

  test('font preview size', () => {
    function assertFontSize(element: HTMLElement, expectedFontSize: number) {
      // Check that the font size is applied correctly.
      const {value, unit} = element.computedStyleMap().get('font-size') as
          {value: number, unit: string};
      assertEquals('px', unit);
      assertEquals(expectedFontSize, value);
      // Check that the font size value is displayed correctly.
      assertTrue(
          element.textContent!.trim().startsWith(expectedFontSize.toString()));
    }

    fontsPage.prefs = {
      webkit: {
        webprefs: {
          default_font_size:
              {value: 20, type: chrome.settingsPrivate.PrefType.NUMBER},
          default_fixed_font_size:
              {value: 10, type: chrome.settingsPrivate.PrefType.NUMBER},
        }
      }
    };

    assertFontSize(fontsPage.$.standardFontPreview, 20);
    assertFontSize(fontsPage.$.serifFontPreview, 20);
    assertFontSize(fontsPage.$.sansSerifFontPreview, 20);
    assertFontSize(fontsPage.$.fixedFontPreview, 10);
  });

  test('font preview family', () => {
    function assertFontFamily(element: HTMLElement, genericFamily: string) {
      // Check that the font-family is applied correctly.
      const family = element.computedStyleMap().get('font-family') as string;
      assertEquals(`custom_${genericFamily}`, family.toString());
    }

    fontsPage.prefs = {
      webkit: {
        webprefs: {
          fonts: {
            standard: {
              Zyyy: {
                value: 'custom_standard',
                type: chrome.settingsPrivate.PrefType.STRING
              }
            },
            serif: {
              Zyyy: {
                value: 'custom_serif',
                type: chrome.settingsPrivate.PrefType.STRING
              }
            },
            sansserif: {
              Zyyy: {
                value: 'custom_sansserif',
                type: chrome.settingsPrivate.PrefType.STRING
              }
            },
            fixed: {
              Zyyy: {
                value: 'custom_fixed',
                type: chrome.settingsPrivate.PrefType.STRING
              }
            },
          }
        }
      }
    };

    assertFontFamily(fontsPage.$.standardFontPreview, 'standard');
    assertFontFamily(fontsPage.$.serifFontPreview, 'serif');
    assertFontFamily(fontsPage.$.sansSerifFontPreview, 'sansserif');
    assertFontFamily(fontsPage.$.fixedFontPreview, 'fixed');
  });
});
