// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BrowserProxy, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {AVAILABLE_GOOGLE_TTS_LOCALES, convertLangOrLocaleForVoicePackManager, PACK_MANAGER_SUPPORTED_LANGS_AND_LOCALES} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createSpeechSynthesisVoice, emitEvent, setVoices, suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {FakeSpeechSynthesis} from './fake_speech_synthesis.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('LanguageChanged', () => {
  const langForDefaultVoice = 'en';
  const lang1 = 'zh';
  const lang2 = 'tr';
  const lang3 = 'pt-br';
  const langWithNoVoices = 'elvish';

  const defaultVoice = createSpeechSynthesisVoice({
    lang: langForDefaultVoice,
    name: 'Google Kristi',
    default: true,
  });
  const firstVoiceWithLang1 =
      createSpeechSynthesisVoice({lang: lang1, name: 'Google Lauren'});
  const defaultVoiceWithLang1 = createSpeechSynthesisVoice(
      {lang: lang1, name: 'Google Eitan', default: true});
  const firstVoiceWithLang2 =
      createSpeechSynthesisVoice({lang: lang2, name: 'Google Yu'});
  const secondVoiceWithLang2 =
      createSpeechSynthesisVoice({lang: lang2, name: 'Google Xiang'});
  const firstVoiceWithLang3 =
      createSpeechSynthesisVoice({lang: lang3, name: 'Google Kristi'});
  const naturalVoiceWithLang3 = createSpeechSynthesisVoice(
      {lang: lang3, name: 'Google Kristi (Natural)'});
  const otherVoice =
      createSpeechSynthesisVoice({lang: 'it', name: 'Google Shari'});
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
  let app: AppElement;
  let speechSynthesis: FakeSpeechSynthesis;

  function enableLangs(...langs: string[]) {
    for (const l of langs) {
      if (!app.enabledLangs.includes(l)) {
        app.enabledLangs.push(l);
      }
    }
  }

  function setInstalled(lang: string) {
    app.updateVoicePackStatus(lang, 'kInstalled');
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
    flush();

    speechSynthesis = new FakeSpeechSynthesis();
    app.synth = speechSynthesis;
    setVoices(app, speechSynthesis, voices);
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
    assertEquals(startingVoice, app.getSpeechSynthesisVoice());
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

      assertEquals(otherVoice, app.getSpeechSynthesisVoice());
    });

    test('and enables the stored voice language', () => {
      const voice = createSpeechSynthesisVoice({lang: 'es-us', name: 'Mush'});
      chrome.readingMode.getStoredVoice = () => voice.name;
      chrome.readingMode.baseLanguageForSpeech = 'es';
      setInstalled(voice.lang);
      setVoices(app, speechSynthesis, [voice]);
      assertFalse(app.enabledLangs.includes(voice.lang));

      app.languageChanged();

      assertTrue(app.enabledLangs.includes(voice.lang));
      assertEquals(voice, app.getSpeechSynthesisVoice());
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
          emitEvent(
              app, ToolbarEvent.VOICE, {detail: {selectedVoice: otherVoice}});
          app.languageChanged();
          assertEquals(otherVoice, app.getSpeechSynthesisVoice());
        });

        test('to a natural voice if there\'s no current voice', () => {
          app.languageChanged();
          assertEquals(naturalVoiceWithLang3, app.getSpeechSynthesisVoice());
        });

        test('to the device default if there\'s no natural', () => {
          setVoices(
              app, speechSynthesis,
              voices.filter(v => v !== naturalVoiceWithLang3));
          app.languageChanged();
          assertEquals(defaultVoice, app.getSpeechSynthesisVoice());
        });
      });

      test('to a voice in the enabled locale for this base language', () => {
        const voice =
            createSpeechSynthesisVoice({lang: 'es-us', name: 'Spanish'});
        app.enabledLangs = ['es-us'];
        setVoices(app, speechSynthesis, [voice]);
        flush();
        chrome.readingMode.baseLanguageForSpeech = 'es';

        app.languageChanged();

        assertEquals(voice, app.getSpeechSynthesisVoice());
      });

      test('to a voice in the available locale for this base language', () => {
        const voice =
            createSpeechSynthesisVoice({lang: 'en-au', name: 'Australian'});
        app.enabledLangs = [];
        setVoices(app, speechSynthesis, [voice]);
        setInstalled('en-au');
        flush();
        chrome.readingMode.baseLanguageForSpeech = 'en';

        app.languageChanged();

        assertEquals(voice, app.getSpeechSynthesisVoice());
      });

      suite('and this locale is enabled', () => {
        test('to a natural voice for this language', () => {
          chrome.readingMode.baseLanguageForSpeech = lang3;
          app.languageChanged();
          assertEquals(naturalVoiceWithLang3, app.getSpeechSynthesisVoice());
        });

        test(
            'to the default voice for this language if there\'s no natural voice',
            () => {
              chrome.readingMode.baseLanguageForSpeech = lang1;
              app.languageChanged();
              assertEquals(
                  defaultVoiceWithLang1, app.getSpeechSynthesisVoice());
            });

        test(
            'to the first listed voice for this language if there\'s no default',
            () => {
              chrome.readingMode.baseLanguageForSpeech = lang2;
              app.languageChanged();
              assertEquals(firstVoiceWithLang2, app.getSpeechSynthesisVoice());
            });
      });

      suite('and this locale is disabled', () => {
        test('and it enables pack manager locale', () => {
          app.enabledLangs = [];
          flush();
          chrome.readingMode.baseLanguageForSpeech = lang3;

          app.languageChanged();

          assertTrue(app.enabledLangs.includes(lang3));
          assertEquals(naturalVoiceWithLang3, app.getSpeechSynthesisVoice());
        });

        test(
            'and it enables other locale if not supported by pack manager',
            () => {
              app.enabledLangs = [];
              flush();
              chrome.readingMode.baseLanguageForSpeech = lang1;

              app.languageChanged();

              assertTrue(app.enabledLangs.includes(lang1));
              assertEquals(
                  defaultVoiceWithLang1, app.getSpeechSynthesisVoice());
            });


        test('to voice in different locale and same language', () => {
          const voice = createSpeechSynthesisVoice(
              {lang: 'en-GB', name: 'British', default: true});
          app.enabledLangs = ['en-gb'];
          setVoices(app, speechSynthesis, [voice]);
          setInstalled('en-gb');
          setInstalled('en-us');
          flush();
          chrome.readingMode.baseLanguageForSpeech = 'en-US';

          app.languageChanged();

          assertEquals(voice, app.getSpeechSynthesisVoice());
        });

        test('to natural enabled voice if no same locale', () => {
          app.enabledLangs = [lang3];
          setVoices(app, speechSynthesis, [naturalVoiceWithLang3]);
          chrome.readingMode.baseLanguageForSpeech = lang2;

          app.languageChanged();

          assertEquals(naturalVoiceWithLang3, app.getSpeechSynthesisVoice());
        });

        test('to default enabled voice if no natural voice', () => {
          app.enabledLangs = [lang1];
          setVoices(app, speechSynthesis, [defaultVoiceWithLang1]);
          chrome.readingMode.baseLanguageForSpeech = lang2;

          app.languageChanged();

          assertEquals(defaultVoiceWithLang1, app.getSpeechSynthesisVoice());
        });

        test('to undefined if no enabled languages', () => {
          app.enabledLangs = [];
          flush();
          chrome.readingMode.baseLanguageForSpeech = lang2;

          app.languageChanged();

          assertEquals(undefined, app.getSpeechSynthesisVoice());
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

      app.updateVoicePackStatus(lang, 'kInstalling');
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
