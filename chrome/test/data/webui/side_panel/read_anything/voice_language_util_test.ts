// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {convertLangOrLocaleForVoicePackManager, convertLangOrLocaleToExactVoicePackLocale, convertLangToAnAvailableLangIfPresent, createInitialListOfEnabledLanguages, getFilteredVoiceList, getNotification, mojoVoicePackStatusToVoicePackStatusEnum, NotificationType, VoiceClientSideStatusCode, VoicePackServerStatusErrorCode, VoicePackServerStatusSuccessCode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertDeepEquals, assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createSpeechSynthesisVoice} from './common.js';


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

    // Uses enabled langs when available
    const enabledLangs = ['en-au', 'es-us', 'pt-pt'];
    assertEquals(
        'en-au', convertLangOrLocaleForVoicePackManager('en', enabledLangs));
    assertEquals(
        'es-us', convertLangOrLocaleForVoicePackManager('es', enabledLangs));
    assertEquals(
        'pt-pt', convertLangOrLocaleForVoicePackManager('pt', enabledLangs));

    // Uses available langs when available
    const availableLangs = ['en-gb', 'es-us', 'pt-pt'];
    assertEquals(
        'en-gb',
        convertLangOrLocaleForVoicePackManager('en', [], availableLangs));
    assertEquals(
        'es-us',
        convertLangOrLocaleForVoicePackManager('es', [], availableLangs));
    assertEquals(
        'pt-pt',
        convertLangOrLocaleForVoicePackManager('pt', [], availableLangs));
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

  test('getFilteredVoiceList filters remote voices', () => {
    const voice1 = {
      default: true,
      name: 'Eitan',
      lang: 'en-us',
      localService: false,
      voiceURI: '',
    };
    const voice2 = {
      default: false,
      name: 'Lauren',
      lang: 'en-us',
      localService: true,
      voiceURI: '',
    };
    let possibleVoices: SpeechSynthesisVoice[] = [voice1];

    // Remote voices not filtered out with just one voice.
    assertDeepEquals([voice1], getFilteredVoiceList(possibleVoices));

    possibleVoices = [voice1, voice2];
    // Remote voices filtered out when a local voice exists
    assertDeepEquals([voice2], getFilteredVoiceList(possibleVoices));
  });

  test('getFilteredVoiceList filters Android voices', () => {
    const voice1 = {
      default: false,
      name: 'Xiang',
      lang: 'en-us',
      localService: true,
      voiceURI: '',
    };
    const voice2 = {
      default: true,
      name: 'Kristi (Android)',
      lang: 'en-us',
      localService: true,
      voiceURI: '',
    };

    // Android voices should be filtered out only on ChromeOS Ash.
    const possibleVoices: SpeechSynthesisVoice[] = [voice1, voice2];

    if (chrome.readingMode.isChromeOsAsh) {
      assertDeepEquals([voice1], getFilteredVoiceList(possibleVoices));
    } else {
      assertDeepEquals([voice2], getFilteredVoiceList(possibleVoices));
    }
  });

  test('getFilteredVoiceList filters eSpeak voices', () => {
    const voice1 = createSpeechSynthesisVoice(
        {default: true, name: 'eSpeak Yu', localService: true, lang: 'en-us'});
    const voice2 = createSpeechSynthesisVoice(
        {default: true, name: 'eSpeak Kristi', localService: true, lang: 'cy'});
    const voice3 = createSpeechSynthesisVoice({
      default: true,
      name: 'eSpeak Lauren',
      localService: true,
      lang: 'en-cb',
    });

    const possibleVoices: SpeechSynthesisVoice[] = [voice1, voice2, voice3];

    if (chrome.readingMode.isChromeOsAsh) {
      // Welsh isn't a Google TTS locale, so the Welsh eSpeak voice should be
      // kept, but the English eSpeak voices should be filtered out because
      // Google TTS voices in English (even if in a different locale) exist.
      assertDeepEquals([voice2], getFilteredVoiceList(possibleVoices));
    } else {
      // eSpeak voices should be filtered out only on ChromeOS Ash.
      assertDeepEquals(
          [voice1, voice2, voice3], getFilteredVoiceList(possibleVoices));
    }
  });

  test(
      'getFilteredVoiceList returns only Google voices and one system voice',
      () => {
        const voice1 = createSpeechSynthesisVoice({
          default: true,
          name: 'Google Eitan',
          localService: true,
          lang: 'en-us',
        });
        const voice2 = createSpeechSynthesisVoice({
          default: true,
          name: 'Google Shari',
          localService: true,
          lang: 'cy',
        });
        const voice3 = createSpeechSynthesisVoice({
          default: false,
          name: 'Lauren',
          localService: true,
          lang: 'en-cb',
        });
        const voice4 = createSpeechSynthesisVoice({
          default: true,
          name: 'Kristi',
          localService: true,
          lang: 'en-cb',
        });

        const possibleVoices: SpeechSynthesisVoice[] =
            [voice1, voice2, voice3, voice4];

        if (chrome.readingMode.isChromeOsAsh) {
          // Don't filter out any system voices on ChromeOS.
          assertDeepEquals(
              possibleVoices, getFilteredVoiceList(possibleVoices));
        } else {
          // Keep only the default system voice.
          assertDeepEquals(
              [voice1, voice2, voice4], getFilteredVoiceList(possibleVoices));
        }
      });

  test('getNotification', () => {
    const availableVoices: SpeechSynthesisVoice[] = [];
    // Unsupported language.
    assertEquals(
        NotificationType.NONE,
        getNotification(
            'unsupported lang', VoiceClientSideStatusCode.SENT_INSTALL_REQUEST,
            availableVoices, true));

    // Downloading notifications.
    const voicePackLang = 'cs-cz';
    assertEquals(
        NotificationType.DOWNLOADING,
        getNotification(
            voicePackLang, VoiceClientSideStatusCode.SENT_INSTALL_REQUEST,
            availableVoices, true));
    assertEquals(
        NotificationType.DOWNLOADING,
        getNotification(
            voicePackLang,
            VoiceClientSideStatusCode.SENT_INSTALL_REQUEST_ERROR_RETRY,
            availableVoices, true));
    assertEquals(
        NotificationType.DOWNLOADING,
        getNotification(
            voicePackLang, VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE,
            availableVoices, true));

    // Offline.
    assertEquals(
        NotificationType.NO_INTERNET,
        getNotification(
            voicePackLang, VoiceClientSideStatusCode.ERROR_INSTALLING,
            availableVoices, false));
    availableVoices.push(createSpeechSynthesisVoice({
      name: 'Ed',
      lang: voicePackLang,
    }));
    assertEquals(
        NotificationType.NONE,
        getNotification(
            voicePackLang, VoiceClientSideStatusCode.ERROR_INSTALLING,
            availableVoices, false));

    // Generic error.
    assertEquals(
        NotificationType.NONE,
        getNotification(
            voicePackLang, VoiceClientSideStatusCode.ERROR_INSTALLING,
            availableVoices, true));
    availableVoices.pop();
    assertEquals(
        NotificationType.GENERIC_ERROR,
        getNotification(
            voicePackLang, VoiceClientSideStatusCode.ERROR_INSTALLING,
            availableVoices, true));

    // Allocation error.
    assertEquals(
        NotificationType.NO_SPACE,
        getNotification(
            voicePackLang, VoiceClientSideStatusCode.INSTALL_ERROR_ALLOCATION,
            availableVoices, true));
    availableVoices.push(createSpeechSynthesisVoice({
      name: 'Taylor',
      lang: voicePackLang,
    }));
    assertEquals(
        NotificationType.NO_SPACE_HQ,
        getNotification(
            voicePackLang, VoiceClientSideStatusCode.INSTALL_ERROR_ALLOCATION,
            availableVoices, true));
  });
});
