// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {isWindows} from 'chrome://resources/js/cr.m.js';
import {LanguageHelper, LanguagesBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeDataBind} from 'chrome://webui-test/test_util.js';

import {FakeLanguageSettingsPrivate, getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {FakeSettingsPrivate} from './fake_settings_private.js';
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
    document.body.innerHTML = '';
  });

  setup(function() {
    const settingsPrefs = document.createElement('settings-prefs');
    const settingsPrivate = new FakeSettingsPrivate(getFakeLanguagePrefs());
    settingsPrefs.initialize(
        settingsPrivate as unknown as typeof chrome.settingsPrivate);
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

    document.body.appendChild(settingsLanguages);
    return languageHelper.whenReady().then(function() {
      return isWindows ? browserProxy.whenCalled('getProspectiveUILanguage') :
                         Promise.resolve();
    });
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

    // The 'no' macrolanguage is returned for Norsk Nynorsk.
    lang = languageHelper.getLanguage('nn')!;
    assertEquals('no', lang.code);
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
