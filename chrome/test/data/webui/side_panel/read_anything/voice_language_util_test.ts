// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {convertLangOrLocaleForVoicePackManager, convertLangOrLocaleToExactVoicePackLocale, convertLangToAnAvailableLangIfPresent, createInitialListOfEnabledLanguages, mojoVoicePackStatusToVoicePackStatusEnum, VoicePackServerStatusErrorCode, VoicePackServerStatusSuccessCode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertDeepEquals, assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';


suite('voice and language utils', () => {
  test('mojoVoicePackStatusToVoicePackStatusEnum', () => {
    // Success codes
    assertEquals(
        VoicePackServerStatusSuccessCode.NOT_INSTALLED,
        mojoVoicePackStatusToVoicePackStatusEnum('kNotInstalled').code);
    assertEquals(
        VoicePackServerStatusSuccessCode.INSTALLED,
        mojoVoicePackStatusToVoicePackStatusEnum('kInstalled').code);
    assertEquals(
        VoicePackServerStatusSuccessCode.INSTALLING,
        mojoVoicePackStatusToVoicePackStatusEnum('kInstalling').code);

    // Error codes
    assertEquals(
        VoicePackServerStatusErrorCode.OTHER,
        mojoVoicePackStatusToVoicePackStatusEnum('kUnknown').code);
    assertEquals(
        VoicePackServerStatusErrorCode.OTHER,
        mojoVoicePackStatusToVoicePackStatusEnum('kOther').code);
    assertEquals(
        VoicePackServerStatusErrorCode.WRONG_ID,
        mojoVoicePackStatusToVoicePackStatusEnum('kWrongId').code);
    assertEquals(
        VoicePackServerStatusErrorCode.NEED_REBOOT,
        mojoVoicePackStatusToVoicePackStatusEnum('kNeedReboot').code);
    assertEquals(
        VoicePackServerStatusErrorCode.ALLOCATION,
        mojoVoicePackStatusToVoicePackStatusEnum('kAllocation').code);
    assertEquals(
        VoicePackServerStatusErrorCode.UNSUPPORTED_PLATFORM,
        mojoVoicePackStatusToVoicePackStatusEnum('kUnsupportedPlatform').code);
  });

  test('convertLangOrLocaleForVoicePackManager', () => {
    // Converts to locale when necessary
    assertEquals('en-us', convertLangOrLocaleForVoicePackManager('en'));
    assertEquals('es-es', convertLangOrLocaleForVoicePackManager('es'));
    assertEquals('pt-br', convertLangOrLocaleForVoicePackManager('pt'));

    // Converts to base language when necessary
    assertEquals('fr', convertLangOrLocaleForVoicePackManager('fr-FR'));
    assertEquals('fr', convertLangOrLocaleForVoicePackManager('fr-Bf'));


    // Converts from unsupported locale to supported locale when necessary
    assertEquals('en-us', convertLangOrLocaleForVoicePackManager('en-UK'));
    assertEquals('es-es', convertLangOrLocaleForVoicePackManager('es-MX'));

    // Keeps base language when necessary
    assertEquals('nl', convertLangOrLocaleForVoicePackManager('nl'));
    assertEquals('yue', convertLangOrLocaleForVoicePackManager('yue'));

    // Keeps locale when necesseary
    assertEquals('en-gb', convertLangOrLocaleForVoicePackManager('en-GB'));
    assertEquals('es-es', convertLangOrLocaleForVoicePackManager('es-es'));
    assertEquals('pt-br', convertLangOrLocaleForVoicePackManager('pt-Br'));

    // Unsupported languages are undefined
    assertEquals(undefined, convertLangOrLocaleForVoicePackManager('cn'));
  });

  test('convertLangOrLocaleToExactVoicePackLocale', () => {
    // Converts to voice pack locale when multiple supported
    assertEquals('en-us', convertLangOrLocaleToExactVoicePackLocale('en'));
    assertEquals('es-es', convertLangOrLocaleToExactVoicePackLocale('es'));
    assertEquals('pt-br', convertLangOrLocaleToExactVoicePackLocale('pt'));

    // Converts to voice pack locale when only one locale supported
    assertEquals('fr-fr', convertLangOrLocaleToExactVoicePackLocale('fr'));
    assertEquals('it-it', convertLangOrLocaleToExactVoicePackLocale('it'));
    assertEquals('bn-bd', convertLangOrLocaleToExactVoicePackLocale('bn'));

    // Converts from unsupported locale to supported locale when necessary
    assertEquals('en-us', convertLangOrLocaleToExactVoicePackLocale('en-UK'));
    assertEquals('es-es', convertLangOrLocaleToExactVoicePackLocale('es-MX'));

    // Keeps locale when necesseary
    assertEquals('en-gb', convertLangOrLocaleToExactVoicePackLocale('en-GB'));
    assertEquals('es-es', convertLangOrLocaleToExactVoicePackLocale('es-es'));
    assertEquals('pt-br', convertLangOrLocaleToExactVoicePackLocale('pt-Br'));

    // Unsupported languages are undefined
    assertEquals(undefined, convertLangOrLocaleToExactVoicePackLocale('cn'));
    assertEquals(undefined, convertLangOrLocaleToExactVoicePackLocale('ar'));
  });

  test('convertLangToAnAvailableLangIfPresent', () => {
    // Returns direct matches
    assertEquals(
        'en-us',
        convertLangToAnAvailableLangIfPresent('en-us', ['en', 'fr', 'en-us']));
    assertEquals(
        'en',
        convertLangToAnAvailableLangIfPresent('en', ['en', 'fr', 'en-us']));

    // Finds matching locale for base lang input
    assertEquals(
        'en-us', convertLangToAnAvailableLangIfPresent('en', ['en-US', 'fr']));

    // Finds locale with same base language as input locale, without a direct
    // locale match
    assertEquals(
        'en-us',
        convertLangToAnAvailableLangIfPresent('en-nz', ['en-US', 'fr']));

    // If there's no direct locale match, but our base lang matches both a
    // base lang and a locale with the same base, default to the base lang
    assertEquals(
        'en',
        convertLangToAnAvailableLangIfPresent('en-nz', ['en-US', 'en', 'fr']));

    // Uses browser language fallback.
    assertEquals(
        chrome.readingMode.defaultLanguageForSpeech,
        convertLangToAnAvailableLangIfPresent('es', ['en-US', 'en', 'fr']));

    // No match
    assertEquals(
        undefined,
        convertLangToAnAvailableLangIfPresent('es', ['zh', 'jp', 'fr']));
  });

  test('createInitialListOfEnabledLanguages', () => {
    assertDeepEquals(
        ['en'],
        createInitialListOfEnabledLanguages(
            /* browserOrPageBaseLang= */ 'fr',
            /* storedLanguagesPref= */[], /* availableLangs= */['en'],
            /* langOfDefaultVoice= */ 'en'));

    assertDeepEquals(
        ['fr-fr'],
        createInitialListOfEnabledLanguages(
            /* browserOrPageBaseLang= */ 'fr',
            /* storedLanguagesPref= */[], /* availableLangs= */['en', 'fr-FR'],
            /* langOfDefaultVoice= */ 'en'));

    assertDeepEquals(
        ['en-us', 'fr-fr'],
        createInitialListOfEnabledLanguages(
            /* browserOrPageBaseLang= */ 'fr',
            /* storedLanguagesPref= */['en'],
            /* availableLangs= */['en-us', 'fr-FR'],
            /* langOfDefaultVoice= */ 'en')
            .sort());

    assertDeepEquals(
        ['en-uk', 'fr-fr'],
        createInitialListOfEnabledLanguages(
            /* browserOrPageBaseLang= */ 'fr',
            /* storedLanguagesPref= */['en-us'],
            /* availableLangs= */['en-uk', 'fr-FR'],
            /* langOfDefaultVoice= */ undefined)
            .sort());

    assertDeepEquals(
        ['en-uk'],
        createInitialListOfEnabledLanguages(
            /* browserOrPageBaseLang= */ 'en',
            /* storedLanguagesPref= */['en-uk'],
            /* availableLangs= */['en-uk', 'en-us'],
            /* langOfDefaultVoice= */ undefined)
            .sort());

    assertDeepEquals(
        ['en-us'],
        createInitialListOfEnabledLanguages(
            /* browserOrPageBaseLang= */ 'en',
            /* storedLanguagesPref= */['en-us'],
            /* availableLangs= */['en-uk', 'en-us'],
            /* langOfDefaultVoice= */ undefined)
            .sort());

    assertDeepEquals(
        [],
        createInitialListOfEnabledLanguages(
            /* browserOrPageBaseLang= */ 'fr',
            /* storedLanguagesPref= */[],
            /* availableLangs= */[],
            /* langOfDefaultVoice= */ undefined));
  });
});
