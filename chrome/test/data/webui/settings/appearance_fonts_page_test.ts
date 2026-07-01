// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {PrefService, PrefsBrowserProxy} from 'chrome://settings/settings.js';
import type {FontsBrowserProxy, FontsData, SettingsAppearanceFontsPageElement} from 'chrome://settings/lazy_load.js';
import {FontsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

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
let prefsBrowserProxy: TestPrefsBrowserProxy;

suite('AppearanceFontHandler', function() {
  setup(async function() {
    const initialPrefs = [
      {
        key: 'webkit.webprefs.minimum_font_size',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: 0,
      },
      {
        key: 'webkit.webprefs.fonts.standard.Zyyy',
        type: chrome.settingsPrivate.PrefType.STRING,
        value: 'Roboto',
      },
      {
        key: 'webkit.webprefs.default_font_size',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: 16,
      },
      {
        key: 'webkit.webprefs.fonts.serif.Zyyy',
        type: chrome.settingsPrivate.PrefType.STRING,
        value: 'Roboto',
      },
      {
        key: 'webkit.webprefs.default_fixed_font_size',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: 16,
      },
      {
        key: 'webkit.webprefs.fonts.math.Zyyy',
        type: chrome.settingsPrivate.PrefType.STRING,
        value: 'Roboto',
      },
      {
        key: 'webkit.webprefs.fonts.sansserif.Zyyy',
        type: chrome.settingsPrivate.PrefType.STRING,
        value: 'Roboto',
      },
      {
        key: 'webkit.webprefs.fonts.fixed.Zyyy',
        type: chrome.settingsPrivate.PrefType.STRING,
        value: 'Roboto',
      },
    ];
    prefsBrowserProxy = new TestPrefsBrowserProxy(initialPrefs);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();

    fontsBrowserProxy = new TestFontsBrowserProxy();
    FontsBrowserProxyImpl.setInstance(fontsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    fontsPage = document.createElement('settings-appearance-fonts-page');
    document.body.appendChild(fontsPage);

    await PrefService.getInstance().whenInitialized();
    await microtasksFinished();
  });

  teardown(function() {
    fontsPage.remove();
    PrefService.resetInstanceForTesting();
  });

  test('fetchFontsData', function() {
    return fontsBrowserProxy.whenCalled('fetchFontsData');
  });

  test('minimum font size preview', async () => {
    assertTrue(fontsPage.$.minimumSizeFontPreview.hidden);

    prefsBrowserProxy.fakeApi.sendPrefChanges([
      {key: 'webkit.webprefs.minimum_font_size', value: 6},
    ]);
    await microtasksFinished();
    assertFalse(fontsPage.$.minimumSizeFontPreview.hidden);
    assertEquals('6px', fontsPage.$.minimumSizeFontPreview.style.fontSize);

    prefsBrowserProxy.fakeApi.sendPrefChanges([
      {key: 'webkit.webprefs.minimum_font_size', value: 0},
    ]);
    await microtasksFinished();
    assertTrue(fontsPage.$.minimumSizeFontPreview.hidden);
  });

  test('font preview size', async () => {
    function assertFontSize(element: HTMLElement, expectedFontSize: number) {
      // Check that the font size is applied correctly.
      const {value, unit} = element.computedStyleMap().get('font-size') as
          {value: number, unit: string};
      assertEquals('px', unit);
      assertEquals(expectedFontSize, value);
      // Check that the font size value is displayed correctly.
      assertTrue(
          element.textContent.trim().startsWith(expectedFontSize.toString()));
    }

    // Simulate backend change for default font size and default fixed font
    // size.
    prefsBrowserProxy.fakeApi.sendPrefChanges([
      {key: 'webkit.webprefs.default_font_size', value: 20},
      {key: 'webkit.webprefs.default_fixed_font_size', value: 10},
    ]);
    await microtasksFinished();

    assertFontSize(fontsPage.$.standardFontPreview, 20);
    assertFontSize(fontsPage.$.serifFontPreview, 20);
    assertFontSize(fontsPage.$.sansSerifFontPreview, 20);
    assertFontSize(fontsPage.$.fixedFontPreview, 10);

    assertFontSize(fontsPage.$.mathFontPreview, 20);
  });

  test('font preview family', async () => {
    function assertFontFamily(element: HTMLElement, genericFamily: string) {
      // Check that the font-family is applied correctly.
      const family =
          element.computedStyleMap().get('font-family') as CSSStyleValue;
      assertEquals(`custom_${genericFamily}`, family.toString());
    }

    // Simulate backend change for standard, serif, sansserif, fixed and math
    // fonts.
    prefsBrowserProxy.fakeApi.sendPrefChanges([
      {key: 'webkit.webprefs.fonts.standard.Zyyy', value: 'custom_standard'},
      {key: 'webkit.webprefs.fonts.serif.Zyyy', value: 'custom_serif'},
      {key: 'webkit.webprefs.fonts.sansserif.Zyyy', value: 'custom_sansserif'},
      {key: 'webkit.webprefs.fonts.fixed.Zyyy', value: 'custom_fixed'},
      {key: 'webkit.webprefs.fonts.math.Zyyy', value: 'custom_math'},
    ]);
    await microtasksFinished();

    assertFontFamily(fontsPage.$.standardFontPreview, 'standard');
    assertFontFamily(fontsPage.$.serifFontPreview, 'serif');
    assertFontFamily(fontsPage.$.sansSerifFontPreview, 'sansserif');
    assertFontFamily(fontsPage.$.fixedFontPreview, 'fixed');

    assertFontFamily(fontsPage.$.mathFontPreview, 'math');
  });

  test('font preview fixed Osaka', async () => {
    // Simulate backend change for fixed font.
    prefsBrowserProxy.fakeApi.sendPrefChanges([
      {key: 'webkit.webprefs.fonts.fixed.Zyyy', value: 'Osaka'},
    ]);
    await microtasksFinished();

    const cssFamilyName = fontsPage.$.fixedFontPreview.computedStyleMap().get(
                              'font-family') as CSSStyleValue;
    // <if expr="is_macosx">
    assertEquals(`Osaka-Mono`, cssFamilyName.toString());
    // </if>
    // <if expr="not is_macosx">
    assertEquals(`Osaka`, cssFamilyName.toString());
    // </if>
  });

  test('math font preview', async () => {
    const mathFontPreview = fontsPage.$.mathFontPreview;
    assertTrue(!!mathFontPreview);
    const maths = mathFontPreview.getElementsByTagNameNS(
        'http://www.w3.org/1998/Math/MathML', 'math');
    assertEquals(maths.length, 1);
    assertTrue(!!maths[0]);
    const math = maths[0];

    // Check that the font properties are inherited on the sample formula.
    const EXPECTED_FONT_SIZE = 20;
    const EXPECTED_FONT_FAMILY = 'inherited_math_font';
    // Simulate backend change for default font size and math font.
    prefsBrowserProxy.fakeApi.sendPrefChanges([
      {key: 'webkit.webprefs.default_font_size', value: EXPECTED_FONT_SIZE},
      {key: 'webkit.webprefs.fonts.math.Zyyy', value: EXPECTED_FONT_FAMILY},
    ]);
    await microtasksFinished();
    const family = math.computedStyleMap().get('font-family') as CSSStyleValue;
    assertEquals(EXPECTED_FONT_FAMILY, family.toString());
    const {value, unit} = math.computedStyleMap().get('font-size') as
        {value: number, unit: string};
    assertEquals('px', unit);
    assertEquals(EXPECTED_FONT_SIZE, value);

    // Check that the math preview demonstrates a few characters.
    const textContentArray: string[] = [];
    math.querySelectorAll('mn,mo,mi').forEach(element => {
      textContentArray.push(element.textContent);
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
