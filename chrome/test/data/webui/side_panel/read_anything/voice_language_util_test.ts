// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {convertLangOrLocaleForVoicePackManager, convertLangToAnAvailableLangIfPresent, createInitialListOfEnabledLanguages, mojoVoicePackStatusToVoicePackStatusEnum, VoicePackStatus} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertDeepEquals, assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

suite('voice and language utils', () => {
  test('mojoVoicePackStatusToVoicePackStatusEnum', () => {
    // Success codes
    assertEquals(
        (mojoVoicePackStatusToVoicePackStatusEnum('kNotInstalled')),
        VoicePackStatus.EXISTS);
    assertEquals(
        (mojoVoicePackStatusToVoicePackStatusEnum('kInstalled')),
        VoicePackStatus.DOWNLOADED);
    assertEquals(
        (mojoVoicePackStatusToVoicePackStatusEnum('kInstalling')),
        VoicePackStatus.INSTALLING);

    // Error codes
    assertEquals(
        (mojoVoicePackStatusToVoicePackStatusEnum('kUnknown')),
        VoicePackStatus.INSTALL_ERROR);
    assertEquals(
        (mojoVoicePackStatusToVoicePackStatusEnum('kOther')),
        VoicePackStatus.INSTALL_ERROR);
    assertEquals(
        (mojoVoicePackStatusToVoicePackStatusEnum('kWrongId')),
        VoicePackStatus.INSTALL_ERROR);
    assertEquals(
        (mojoVoicePackStatusToVoicePackStatusEnum('kNeedReboot')),
        VoicePackStatus.INSTALL_ERROR);
    assertEquals(
        (mojoVoicePackStatusToVoicePackStatusEnum('kAllocation')),
        VoicePackStatus.INSTALL_ERROR);
    assertEquals(
        (mojoVoicePackStatusToVoicePackStatusEnum('kUnsupportedPlatform')),
        VoicePackStatus.INSTALL_ERROR);
  });

  test('convertLangOrLocaleForVoicePackManager', () => {
    // Converts to locale when necessary
    assertEquals(convertLangOrLocaleForVoicePackManager('en'), 'en-us');
    assertEquals(convertLangOrLocaleForVoicePackManager('es'), 'es-es');
    assertEquals(convertLangOrLocaleForVoicePackManager('pt'), 'pt-br');

    // Converts to base language when necessary
    assertEquals(convertLangOrLocaleForVoicePackManager('fr-FR'), 'fr');
    assertEquals(convertLangOrLocaleForVoicePackManager('fr-Bf'), 'fr');


    // Converts from unsupported locale to supported locale when necessary
    assertEquals(convertLangOrLocaleForVoicePackManager('en-UK'), 'en-us');
    assertEquals(convertLangOrLocaleForVoicePackManager('es-MX'), 'es-es');

    // Keeps base language when necessary
    assertEquals(convertLangOrLocaleForVoicePackManager('nl'), 'nl');
    assertEquals(convertLangOrLocaleForVoicePackManager('yue'), 'yue');

    // Keeps locale when necesseary
    assertEquals(convertLangOrLocaleForVoicePackManager('en-GB'), 'en-gb');
    assertEquals(convertLangOrLocaleForVoicePackManager('es-es'), 'es-es');
    assertEquals(convertLangOrLocaleForVoicePackManager('pt-Br'), 'pt-br');

    // Unsupported languages are undefined
    assertEquals(convertLangOrLocaleForVoicePackManager('cn'), undefined);
  });

  test('convertLangToAnAvailableLangIfPresent', () => {
    // Returns direct matches
    assertEquals(
        convertLangToAnAvailableLangIfPresent('en-us', ['en', 'fr', 'en-us']),
        'en-us');
    assertEquals(
        convertLangToAnAvailableLangIfPresent('en', ['en', 'fr', 'en-us']),
        'en');

    // Finds matching locale for base lang input
    assertEquals(
        convertLangToAnAvailableLangIfPresent('en', ['en-US', 'fr']), 'en-us');

    // Finds locale with same base language as input locale, without a direct
    // locale match
    assertEquals(
        convertLangToAnAvailableLangIfPresent('en-nz', ['en-US', 'fr']),
        'en-us');

    // If there's no direct locale match, but our base lang matches both a
    // base lang and a locale with the same base, default to the base lang
    assertEquals(
        convertLangToAnAvailableLangIfPresent('en-nz', ['en-US', 'en', 'fr']),
        'en');

    // Uses browser language fallback.
    assertEquals(
        convertLangToAnAvailableLangIfPresent('es', ['en-US', 'en', 'fr']),
        chrome.readingMode.defaultLanguageForSpeech);

    // No match
    assertEquals(
        convertLangToAnAvailableLangIfPresent('es', ['zh', 'jp', 'fr']),
        undefined);
  });

  test('createInitialListOfEnabledLanguages', () => {
    assertDeepEquals(
        createInitialListOfEnabledLanguages(
            /* browserOrPageBaseLang= */ 'fr',
            /* storedLanguagesPref= */[], /* availableLangs= */['en'],
            /* langOfDefaultVoice= */ 'en'),
        ['en']);

    assertDeepEquals(
        createInitialListOfEnabledLanguages(
            /* browserOrPageBaseLang= */ 'fr',
            /* storedLanguagesPref= */[], /* availableLangs= */['en', 'fr-FR'],
            /* langOfDefaultVoice= */ 'en'),
        ['fr-fr']);

    assertDeepEquals(
        createInitialListOfEnabledLanguages(
            /* browserOrPageBaseLang= */ 'fr',
            /* storedLanguagesPref= */['en'],
            /* availableLangs= */['en-us', 'fr-FR'],
            /* langOfDefaultVoice= */ 'en')
            .sort(),
        ['en-us', 'fr-fr']);

    assertDeepEquals(
        createInitialListOfEnabledLanguages(
            /* browserOrPageBaseLang= */ 'fr',
            /* storedLanguagesPref= */['en-us'],
            /* availableLangs= */['en-uk', 'fr-FR'],
            /* langOfDefaultVoice= */ undefined)
            .sort(),
        ['en-uk', 'fr-fr']);

    assertDeepEquals(
        createInitialListOfEnabledLanguages(
            /* browserOrPageBaseLang= */ 'en',
            /* storedLanguagesPref= */['en-uk'],
            /* availableLangs= */['en-uk', 'en-us'],
            /* langOfDefaultVoice= */ undefined)
            .sort(),
        ['en-uk']);

    assertDeepEquals(
        createInitialListOfEnabledLanguages(
            /* browserOrPageBaseLang= */ 'en',
            /* storedLanguagesPref= */['en-us'],
            /* availableLangs= */['en-uk', 'en-us'],
            /* langOfDefaultVoice= */ undefined)
            .sort(),
        ['en-us']);

    assertDeepEquals(
        createInitialListOfEnabledLanguages(
            /* browserOrPageBaseLang= */ 'fr',
            /* storedLanguagesPref= */[],
            /* availableLangs= */[],
            /* langOfDefaultVoice= */ undefined),
        []);
  });
});
