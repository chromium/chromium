// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {LanguageHelper} from 'chrome://settings/lazy_load.js';
import {LanguagesBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';
import {fakeDataBind, flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import type {FakeLanguageSettingsPrivate} from './fake_language_settings_private.js';
import {getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {TestLanguagesBrowserProxy} from './test_languages_browser_proxy.js';

// clang-format on

suite('settings-languages', function() {
  function assertLanguageOrder(expected: string[]) {
    assertEquals(expected.length, languageHelper.languages!.enabled.length);
    for (let i = 0; i < expected.length; i++) {
      assertEquals(
          expected[i], languageHelper.languages!.enabled[i]!.language.code);
    }
  }

  let browserProxy: TestLanguagesBrowserProxy;
  let languageHelper: LanguageHelper;
  let languageSettingsPrivate: FakeLanguageSettingsPrivate;

  suiteSetup(function() {
    CrSettingsPrefs.deferInitialization = true;
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  setup(async function() {
    const settingsPrefs = document.createElement('settings-prefs');
    const settingsPrivate = new FakeSettingsPrivate(getFakeLanguagePrefs());
    settingsPrefs.initialize(settingsPrivate);
    document.body.appendChild(settingsPrefs);

    // Setup test browser proxy.
    browserProxy = new TestLanguagesBrowserProxy();
    LanguagesBrowserProxyImpl.setInstance(browserProxy);

    // Setup fake languageSettingsPrivate API.
    languageSettingsPrivate = browserProxy.getLanguageSettingsPrivate() as
        unknown as FakeLanguageSettingsPrivate;
    languageSettingsPrivate.setSettingsPrefs(settingsPrefs);

    const settingsLanguages = document.createElement('settings-languages');
    languageHelper = settingsLanguages.languageHelper;

    // Prefs would normally be data-bound to settings-languages.
    fakeDataBind(settingsPrefs, settingsLanguages, 'prefs');
    await flushTasks();

    document.body.appendChild(settingsLanguages);
    await languageHelper.whenReady();
    // <if expr="is_win">
    await browserProxy.whenCalled('getProspectiveUiLanguage');
    // </if>
  });

  test('languages model', function() {
    for (let i = 0; i < languageSettingsPrivate.languages.length; i++) {
      assertEquals(
          languageSettingsPrivate.languages[i]!.code,
          languageHelper.languages!.supported[i]!.code);
    }
    assertLanguageOrder(['en-US', 'sw']);
    assertEquals('en', languageHelper.languages!.translateTarget);

    // TODO(michaelpg): Test other aspects of the model.
  });

  test('get language', function() {
    // If a language code is not found, try language without location.
    let lang = languageHelper.getLanguage('en-CN')!;
    assertEquals('en', lang.code);

    // The old language code for Hebriew is supported.
    lang = languageHelper.getLanguage('iw')!;
    assertEquals('he', lang.code);

    // tl is converted to fil
    lang = languageHelper.getLanguage('tl')!;
    assertEquals('fil', lang.code);
  });

  test('is translate base language', function() {
    assertFalse(languageHelper.isTranslateBaseLanguage(
        languageHelper.getLanguage('nb')!));
    assertFalse(languageHelper.isTranslateBaseLanguage(
        languageHelper.getLanguage('en-US')!));
    assertTrue(languageHelper.isTranslateBaseLanguage(
        languageHelper.getLanguage('en')!));
    assertTrue(languageHelper.isTranslateBaseLanguage(
        languageHelper.getLanguage('sw')!));
  });

  test('get language code without region', function() {
    const cases: Array<[string, string]> = [
      ['en', 'en'],
      ['en-us', 'en'],
      ['fil', 'fil'],
      ['iw', 'iw'],
      ['a', 'a'],
      ['a-b', 'a'],
      ['a-b-c', 'a'],
      ['', ''],
    ];

    for (const [code, base] of cases) {
      assertEquals(languageHelper.getBaseLanguage(code), base);
    }
  });

  test('to translate format', function() {
    const cases: Array<[string, string]> = [
      ['en', 'en'],
      ['en-AU', 'en'],
      ['zh-HK', 'zh-TW'],
      ['zh-TW', 'zh-TW'],
      ['fil', 'tl'],
      ['nb', 'no'],
      ['nn', 'nn'],
      ['he', 'iw'],
    ];

    for (const [code, converted] of cases) {
      assertEquals(
          languageHelper.convertLanguageCodeForTranslate(code), converted);
    }
  });

  test('to chrome format', function() {
    const cases: Array<[string, string]> = [
      ['en-US', 'en-US'],
      ['iw', 'he'],
      ['tl', 'fil'],
      ['jw', 'jv'],
    ];

    for (const [code, converted] of cases) {
      assertEquals(
          languageHelper.convertLanguageCodeForChrome(code), converted);
    }
  });

  test('modifying languages', function() {
    assertTrue(languageHelper.isLanguageEnabled('en-US'));
    assertTrue(languageHelper.isLanguageEnabled('sw'));
    assertFalse(languageHelper.isLanguageEnabled('en-CA'));

    languageHelper.enableLanguage('en-CA');
    assertTrue(languageHelper.isLanguageEnabled('en-CA'));
    languageHelper.disableLanguage('sw');
    assertFalse(languageHelper.isLanguageEnabled('sw'));

    // TODO(michaelpg): Test other modifications.
  });

  test('reorder languages', function() {
    // New language is added at the end.
    languageHelper.enableLanguage('en-CA');
    assertLanguageOrder(['en-US', 'sw', 'en-CA']);

    // Can move a language up.
    languageHelper.moveLanguage('en-CA', true /* upDirection */);
    assertLanguageOrder(['en-US', 'en-CA', 'sw']);

    // Can move a language down.
    languageHelper.moveLanguage('en-US', false /* upDirection */);
    assertLanguageOrder(['en-CA', 'en-US', 'sw']);

    // Can move a language to the front.
    languageHelper.moveLanguageToFront('sw');
    const expectedOrder = ['sw', 'en-CA', 'en-US'];
    assertLanguageOrder(expectedOrder);

    // Moving the first language up has no effect.
    languageHelper.moveLanguage('sw', true /* upDirection */);
    assertLanguageOrder(expectedOrder);

    // Moving the first language to top has no effect.
    languageHelper.moveLanguageToFront('sw');
    assertLanguageOrder(expectedOrder);

    // Moving the last language down has no effect.
    languageHelper.moveLanguage('en-US', false /* upDirection */);
    assertLanguageOrder(expectedOrder);
  });
});
