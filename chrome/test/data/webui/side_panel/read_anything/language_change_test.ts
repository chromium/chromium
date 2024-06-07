// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {AVAILABLE_GOOGLE_TTS_LOCALES, convertLangOrLocaleForVoicePackManager, PACK_MANAGER_SUPPORTED_LANGS_AND_LOCALES, VoicePackServerStatusSuccessCode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {VoicePackStatus} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('LanguageChanged', () => {
  const langForDefaultVoice = 'en';
  const lang1 = 'zh';
  const lang2 = 'tr';
  const lang3 = 'pt-br';
  const langWithNoVoices = 'elvish';

  const defaultVoice = {
    lang: langForDefaultVoice,
    name: 'Kristi',
    default: true,
  } as SpeechSynthesisVoice;
  const firstVoiceWithLang1 = {lang: lang1, name: 'Lauren'} as
      SpeechSynthesisVoice;
  const defaultVoiceWithLang1 = {lang: lang1, name: 'Eitan', default: true} as
      SpeechSynthesisVoice;
  const firstVoiceWithLang2 = {lang: lang2, name: 'Yu'} as SpeechSynthesisVoice;
  const secondVoiceWithLang2 = {lang: lang2, name: 'Xiang'} as
      SpeechSynthesisVoice;
  const firstVoiceWithLang3 = {lang: lang3, name: 'Kristi'} as
      SpeechSynthesisVoice;
  const naturalVoiceWithLang3 = {lang: lang3, name: 'Kristi (Natural)'} as
      SpeechSynthesisVoice;
  const otherVoice = {lang: 'it', name: 'Shari'} as SpeechSynthesisVoice;
  const voices = [
    defaultVoice,
    firstVoiceWithLang1,
    defaultVoiceWithLang1,
    otherVoice,
    firstVoiceWithLang2,
    secondVoiceWithLang2,
    firstVoiceWithLang3,
    naturalVoiceWithLang3,
  ];

  let testBrowserProxy: TestColorUpdaterBrowserProxy;
  let app: ReadAnythingElement;

  function selectedVoice(): SpeechSynthesisVoice {
    // Bypass Typescript compiler to allow us to set a private property
    // @ts-ignore
    return app.selectedVoice;
  }

  function enableLangs(...langs: string[]) {
    for (const l of langs) {
      // @ts-ignore
      if (!app.enabledLanguagesInPref.includes(l)) {
        // @ts-ignore
        app.enabledLanguagesInPref.push(l);
      }
    }
  }

  function setInstalled(lang: string) {
    // @ts-ignore
    app.voicePackInstallStatusServerResponses[lang] = {
      id: 'Successful response',
      code: VoicePackServerStatusSuccessCode.INSTALLED,
    };
  }

  setup(() => {
    suppressInnocuousErrors();
    testBrowserProxy = new TestColorUpdaterBrowserProxy();
    BrowserProxy.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;

    app = document.createElement('read-anything-app');
    document.body.appendChild(app);

    // @ts-ignore
    app.availableVoices = voices;
    // @ts-ignore
    app.availableLangs = voices.map(v => v.lang);
    flush();
  });

  test('updates toolbar fonts', () => {
    let updatedFontsOnToolbar = false;
    app.$.toolbar.updateFonts = () => {
      updatedFontsOnToolbar = true;
    };

    app.languageChanged();

    assertTrue(updatedFontsOnToolbar);
  });

  test('without flag does not update selected voice', () => {
    const startingVoice = app.getSpeechSynthesisVoice();

    chrome.readingMode.getStoredVoice = () => otherVoice.name;
    app.languageChanged();
    assertEquals(selectedVoice(), startingVoice);
  });

  suite('with flag updates selected voice', () => {
    setup(() => {
      chrome.readingMode.isAutoVoiceSwitchingEnabled = true;
      chrome.readingMode.isLanguagePackDownloadingEnabled = true;

      for (const v of voices) {
        setInstalled(v.lang);
        enableLangs(v.lang);
      }
    });

    test('to the stored voice for this language if there is one', () => {
      chrome.readingMode.getStoredVoice = () => otherVoice.name;
      chrome.readingMode.baseLanguageForSpeech = otherVoice.lang;

      app.languageChanged();

      assertEquals(selectedVoice(), otherVoice);
    });

    suite('when there is no stored voice for this language', () => {
      setup(() => {
        chrome.readingMode.getStoredVoice = () => '';
      });

      suite('and no voices at all for this language', () => {
        setup(() => {
          chrome.readingMode.baseLanguageForSpeech = langWithNoVoices;
        });

        test('to the current voice if there is one', () => {
          // @ts-ignore
          app.selectedVoice = otherVoice;
          app.languageChanged();
          assertEquals(selectedVoice(), otherVoice);
        });

        test('to a natural voice if there\'s no current voice', () => {
          app.languageChanged();
          assertEquals(selectedVoice(), naturalVoiceWithLang3);
        });

        test('to the device default if there\'s no natural', () => {
          // @ts-ignore
          app.availableVoices = voices.filter(v => v !== naturalVoiceWithLang3);
          flush();

          app.languageChanged();
          assertEquals(selectedVoice(), defaultVoice);
        });
      });

      suite('and this locale is enabled', () => {
        test('to a natural voice for this language', () => {
          chrome.readingMode.baseLanguageForSpeech = lang3;
          app.languageChanged();
          assertEquals(selectedVoice(), naturalVoiceWithLang3);
        });

        test(
            'to the default voice for this language if there\'s no natural voice',
            () => {
              chrome.readingMode.baseLanguageForSpeech = lang1;
              app.languageChanged();
              assertEquals(selectedVoice(), defaultVoiceWithLang1);
            });

        test(
            'to the first listed voice for this language if there\'s no default',
            () => {
              chrome.readingMode.baseLanguageForSpeech = lang2;
              app.languageChanged();
              assertEquals(selectedVoice(), firstVoiceWithLang2);
            });
      });

      suite('and this locale is disabled', () => {
        test('and it enables pack manager locale', () => {
          // @ts-ignore
          app.enabledLanguagesInPref = [];
          flush();
          chrome.readingMode.baseLanguageForSpeech = lang3;

          app.languageChanged();

          // @ts-ignore
          assertTrue(app.enabledLanguagesInPref.includes(lang3));
          assertEquals(selectedVoice(), naturalVoiceWithLang3);
        });

        test(
            'and it enables other locale if not supported by pack manager',
            () => {
              // @ts-ignore
              app.enabledLanguagesInPref = [];
              flush();
              chrome.readingMode.baseLanguageForSpeech = lang1;

              app.languageChanged();

              // @ts-ignore
              assertTrue(app.enabledLanguagesInPref.includes(lang1));
              assertEquals(selectedVoice(), defaultVoiceWithLang1);
            });


        test('to voice in different locale and same language', () => {
          const voice = {lang: 'en-GB', name: 'British', default: true} as
              SpeechSynthesisVoice;
          // @ts-ignore
          app.enabledLanguagesInPref = ['en-gb'];
          // @ts-ignore
          app.availableVoices = [voice];
          setInstalled('en-gb');
          setInstalled('en-us');
          flush();
          chrome.readingMode.baseLanguageForSpeech = 'en-US';

          app.languageChanged();

          assertEquals(selectedVoice(), voice);
        });

        test('to natural enabled voice if no same locale', () => {
          // @ts-ignore
          app.enabledLanguagesInPref = [lang3];
          // @ts-ignore
          app.availableVoices = [naturalVoiceWithLang3];
          flush();
          chrome.readingMode.baseLanguageForSpeech = lang2;

          app.languageChanged();

          assertEquals(selectedVoice(), naturalVoiceWithLang3);
        });

        test('to default enabled voice if no natural voice', () => {
          // @ts-ignore
          app.enabledLanguagesInPref = [lang1];
          // @ts-ignore
          app.availableVoices = [defaultVoiceWithLang1];
          flush();
          chrome.readingMode.baseLanguageForSpeech = lang2;

          app.languageChanged();

          assertEquals(selectedVoice(), defaultVoiceWithLang1);
        });

        test('to undefined if no enabled languages', () => {
          // @ts-ignore
          app.enabledLanguagesInPref = [];
          flush();
          chrome.readingMode.baseLanguageForSpeech = lang2;

          app.languageChanged();

          assertEquals(selectedVoice(), undefined);
        });
      });
    });
  });

  suite('with flag tries to install voice pack', () => {
    let sentRequest: boolean;

    function setVoicePackServerStatus(lang: string, status: VoicePackStatus) {
      // @ts-ignore
      app.setVoicePackServerStatus_(lang, status);
    }

    setup(() => {
      chrome.readingMode.isLanguagePackDownloadingEnabled = true;
      sentRequest = false;
      chrome.readingMode.sendGetVoicePackInfoRequest = () => {
        sentRequest = true;
      };
    });

    test('but doesn\'t if the language is unsupported', () => {
      chrome.readingMode.baseLanguageForSpeech = 'zh';

      app.languageChanged();

      // Use this check to ensure this stays updated if the supported languages
      // changes.
      assertFalse(PACK_MANAGER_SUPPORTED_LANGS_AND_LOCALES.has(
          chrome.readingMode.baseLanguageForSpeech));
      assertFalse(sentRequest);
    });

    test('if the language is unsupported but has valid voice pack code', () => {
      chrome.readingMode.baseLanguageForSpeech = 'bn';

      app.languageChanged();

      // Use this check to ensure this stays updated if the supported
      // languages changes.
      assertTrue(PACK_MANAGER_SUPPORTED_LANGS_AND_LOCALES.has(
          chrome.readingMode.baseLanguageForSpeech));
      assertFalse(AVAILABLE_GOOGLE_TTS_LOCALES.has(
          chrome.readingMode.baseLanguageForSpeech));
      assertTrue(sentRequest);
    });

    test('but doesn\'t if the language is already installing', () => {
      const lang = 'bn-bd';
      const voicePackLang = convertLangOrLocaleForVoicePackManager(lang);
      assertTrue(voicePackLang !== undefined);

      setVoicePackServerStatus(voicePackLang, {
        id: 'Successful response',
        code: VoicePackServerStatusSuccessCode.INSTALLING,
      });
      app.languageChanged();

      assertFalse(sentRequest);
    });

    test('and gets voice pack info if no status yet', () => {
      const lang = 'bn-bd';
      chrome.readingMode.baseLanguageForSpeech = lang;

      app.languageChanged();

      assertTrue(sentRequest);
    });

    test('and gets voice pack info if we know it exists', () => {
      const lang = 'de-de';
      chrome.readingMode.baseLanguageForSpeech = lang;

      app.languageChanged();

      assertTrue(sentRequest);
    });
  });

});
