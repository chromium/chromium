// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {FontsBrowserProxy, FontsData,SettingsAppearanceFontsPageElement} from 'chrome://settings/lazy_load.js';
import {FontsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
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

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    fontsPage = document.createElement('settings-appearance-fonts-page');
    document.body.appendChild(fontsPage);
    flush();  // #mathFontPreview is inserted dynamically via dom-if.
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
              {value: 0, type: chrome.settingsPrivate.PrefType.NUMBER},
        },
      },
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
        },
      },
    };

    assertFontSize(fontsPage.$.standardFontPreview, 20);
    assertFontSize(fontsPage.$.serifFontPreview, 20);
    assertFontSize(fontsPage.$.sansSerifFontPreview, 20);
    assertFontSize(fontsPage.$.fixedFontPreview, 10);

    const mathFontPreview =
        fontsPage.shadowRoot!.querySelector<HTMLElement>('#mathFontPreview');
    assertTrue(!!mathFontPreview);
    assertFontSize(mathFontPreview, 20);
  });

  test('font preview family', () => {
    function assertFontFamily(element: HTMLElement, genericFamily: string) {
      // Check that the font-family is applied correctly.
      const family =
          element.computedStyleMap().get('font-family') as CSSStyleValue;
      assertEquals(`custom_${genericFamily}`, family.toString());
    }

    fontsPage.prefs = {
      webkit: {
        webprefs: {
          fonts: {
            standard: {
              Zyyy: {
                value: 'custom_standard',
                type: chrome.settingsPrivate.PrefType.STRING,
              },
            },
            serif: {
              Zyyy: {
                value: 'custom_serif',
                type: chrome.settingsPrivate.PrefType.STRING,
              },
            },
            sansserif: {
              Zyyy: {
                value: 'custom_sansserif',
                type: chrome.settingsPrivate.PrefType.STRING,
              },
            },
            fixed: {
              Zyyy: {
                value: 'custom_fixed',
                type: chrome.settingsPrivate.PrefType.STRING,
              },
            },
            math: {
              Zyyy: {
                value: 'custom_math',
                type: chrome.settingsPrivate.PrefType.STRING,
              },
            },
          },
        },
      },
    };

    assertFontFamily(fontsPage.$.standardFontPreview, 'standard');
    assertFontFamily(fontsPage.$.serifFontPreview, 'serif');
    assertFontFamily(fontsPage.$.sansSerifFontPreview, 'sansserif');
    assertFontFamily(fontsPage.$.fixedFontPreview, 'fixed');

    const mathFontPreview =
        fontsPage.shadowRoot!.querySelector<HTMLElement>('#mathFontPreview');
    assertTrue(!!mathFontPreview);
    assertFontFamily(mathFontPreview, 'math');
  });

  test('font preview fixed Osaka', () => {
    fontsPage.prefs = {
      webkit: {
        webprefs: {
          fonts: {
            fixed: {
              Zyyy: {
                value: 'Osaka',
                type: chrome.settingsPrivate.PrefType.STRING,
              },
            },
          },
        },
      },
    };

    const cssFamilyName = fontsPage.$.fixedFontPreview.computedStyleMap().get(
                              'font-family') as CSSStyleValue;
    // <if expr="is_macosx">
    assertEquals(`Osaka-Mono`, cssFamilyName.toString());
    // </if>
    // <if expr="not is_macosx">
    assertEquals(`Osaka`, cssFamilyName.toString());
    // </if>
  });

  test('math font preview', () => {
    const mathFontPreview =
        fontsPage.shadowRoot!.querySelector<HTMLElement>('#mathFontPreview');
    assertTrue(!!mathFontPreview);
    const maths = mathFontPreview.getElementsByTagNameNS(
        'http://www.w3.org/1998/Math/MathML', 'math');
    assertEquals(maths.length, 1);
    assertTrue(!!maths[0]);
    const math = maths[0];

    // Check that the font properties are inherited on the sample formula.
    const EXPECTED_FONT_SIZE = 20;
    const EXPECTED_FONT_FAMILY = 'inherited_math_font';
    fontsPage.prefs = {
      webkit: {
        webprefs: {
          fonts: {
            math: {
              Zyyy: {
                value: EXPECTED_FONT_FAMILY,
                type: chrome.settingsPrivate.PrefType.STRING,
              },
            },
          },
          default_font_size: {
            value: EXPECTED_FONT_SIZE,
            type: chrome.settingsPrivate.PrefType.NUMBER,
          },
        },
      },
    };
    const family = math.computedStyleMap().get('font-family') as CSSStyleValue;
    assertEquals(EXPECTED_FONT_FAMILY, family.toString());
    const {value, unit} = math.computedStyleMap().get('font-size') as
        {value: number, unit: string};
    assertEquals('px', unit);
    assertEquals(EXPECTED_FONT_SIZE, value);

    // Check that the math preview demonstrates a few characters.
    const textContentArray: string[] = [];
    math.querySelectorAll('mn,mo,mi').forEach(element => {
      textContentArray.push(element.textContent!);
    });
    ['0', '∞', 'n', '−', 'π', '∊', 'ℝ', '∑', '∫'].forEach(textContent => {
      assertTrue(
          textContentArray.includes(textContent),
          `math preview contains text "${textContent}"`);
    });

    // Check that the math preview demonstrates a few special math layout such
    // as square roots, scripts or fractions.
    ['msqrt', 'munderover', 'mfrac', 'msup', 'msubsup'].forEach(tagName => {
      assertTrue(
          !!math.querySelector(tagName),
          `math preview contains element <${tagName}>`);
    });
  });
});
