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
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createSpeechSynthesisVoice, suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('LanguageChanged', () => {
  const langForDefaultVoice = 'en';
  const lang1 = 'zh';
  const lang2 = 'tr';
  const lang3 = 'pt-br';
  const langWithNoVoices = 'elvish';

  const defaultVoice = createSpeechSynthesisVoice({
    lang: langForDefaultVoice,
    name: 'Kristi',
    default: true,
  });
  const firstVoiceWithLang1 =
      createSpeechSynthesisVoice({lang: lang1, name: 'Lauren'});
  const defaultVoiceWithLang1 =
      createSpeechSynthesisVoice({lang: lang1, name: 'Eitan', default: true});
  const firstVoiceWithLang2 =
      createSpeechSynthesisVoice({lang: lang2, name: 'Yu'});
  const secondVoiceWithLang2 =
      createSpeechSynthesisVoice({lang: lang2, name: 'Xiang'});
  const firstVoiceWithLang3 =
      createSpeechSynthesisVoice({lang: lang3, name: 'Kristi'});
  const naturalVoiceWithLang3 =
      createSpeechSynthesisVoice({lang: lang3, name: 'Kristi (Natural)'});
  const otherVoice = createSpeechSynthesisVoice({lang: 'it', name: 'Shari'});
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

  function enableLangs(...langs: string[]) {
    for (const l of langs) {
      if (!app.enabledLangs.includes(l)) {
        app.enabledLangs.push(l);
      }
    }
  }

  function setInstalled(lang: string) {
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

    app.availableVoices = voices;
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
    assertEquals(startingVoice, app.selectedVoice);
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

      assertEquals(otherVoice, app.selectedVoice);
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
          app.selectedVoice = otherVoice;
          app.languageChanged();
          assertEquals(otherVoice, app.selectedVoice);
        });

        test('to a natural voice if there\'s no current voice', () => {
          app.languageChanged();
          assertEquals(naturalVoiceWithLang3, app.selectedVoice);
        });

        test('to the device default if there\'s no natural', () => {
          app.availableVoices = voices.filter(v => v !== naturalVoiceWithLang3);
          flush();

          app.languageChanged();
          assertEquals(defaultVoice, app.selectedVoice);
        });
      });

      suite('and this locale is enabled', () => {
        test('to a natural voice for this language', () => {
          chrome.readingMode.baseLanguageForSpeech = lang3;
          app.languageChanged();
          assertEquals(naturalVoiceWithLang3, app.selectedVoice);
        });

        test(
            'to the default voice for this language if there\'s no natural voice',
            () => {
              chrome.readingMode.baseLanguageForSpeech = lang1;
              app.languageChanged();
              assertEquals(defaultVoiceWithLang1, app.selectedVoice);
            });

        test(
            'to the first listed voice for this language if there\'s no default',
            () => {
              chrome.readingMode.baseLanguageForSpeech = lang2;
              app.languageChanged();
              assertEquals(firstVoiceWithLang2, app.selectedVoice);
            });
      });

      suite('and this locale is disabled', () => {
        test('and it enables pack manager locale', () => {
          app.enabledLangs = [];
          flush();
          chrome.readingMode.baseLanguageForSpeech = lang3;

          app.languageChanged();

          assertTrue(app.enabledLangs.includes(lang3));
          assertEquals(naturalVoiceWithLang3, app.selectedVoice);
        });

        test(
            'and it enables other locale if not supported by pack manager',
            () => {
              app.enabledLangs = [];
              flush();
              chrome.readingMode.baseLanguageForSpeech = lang1;

              app.languageChanged();

              assertTrue(app.enabledLangs.includes(lang1));
              assertEquals(defaultVoiceWithLang1, app.selectedVoice);
            });


        test('to voice in different locale and same language', () => {
          const voice = createSpeechSynthesisVoice(
              {lang: 'en-GB', name: 'British', default: true});
          app.enabledLangs = ['en-gb'];
          app.availableVoices = [voice];
          setInstalled('en-gb');
          setInstalled('en-us');
          flush();
          chrome.readingMode.baseLanguageForSpeech = 'en-US';

          app.languageChanged();

          assertEquals(voice, app.selectedVoice);
        });

        test('to natural enabled voice if no same locale', () => {
          app.enabledLangs = [lang3];
          app.availableVoices = [naturalVoiceWithLang3];
          flush();
          chrome.readingMode.baseLanguageForSpeech = lang2;

          app.languageChanged();

          assertEquals(naturalVoiceWithLang3, app.selectedVoice);
        });

        test('to default enabled voice if no natural voice', () => {
          app.enabledLangs = [lang1];
          app.availableVoices = [defaultVoiceWithLang1];
          flush();
          chrome.readingMode.baseLanguageForSpeech = lang2;

          app.languageChanged();

          assertEquals(defaultVoiceWithLang1, app.selectedVoice);
        });

        test('to undefined if no enabled languages', () => {
          app.enabledLangs = [];
          flush();
          chrome.readingMode.baseLanguageForSpeech = lang2;

          app.languageChanged();

          assertEquals(undefined, app.selectedVoice);
        });
      });
    });
  });

  suite('with flag tries to install voice pack', () => {
    let sentRequest: boolean;

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

      app.setVoicePackServerStatus(voicePackLang, {
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
