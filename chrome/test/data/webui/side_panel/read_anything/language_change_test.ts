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
import {PACK_MANAGER_SUPPORTED_LANGS_AND_LOCALES, VoicePackStatus} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {FakeSpeechSynthesis} from './fake_speech_synthesis.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('LanguageChanged', () => {
  const langForDefaultVoice = 'en';
  const lang1 = 'zh';
  const lang2 = 'tr';
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
  const otherVoice = {lang: 'it', name: 'Shari'} as SpeechSynthesisVoice;
  const voices = [
    defaultVoice,
    firstVoiceWithLang1,
    defaultVoiceWithLang1,
    otherVoice,
    firstVoiceWithLang2,
    secondVoiceWithLang2,
  ];

  let testBrowserProxy: TestColorUpdaterBrowserProxy;
  let app: ReadAnythingElement;

  function selectedVoice(): SpeechSynthesisVoice {
    // Bypass Typescript compiler to allow us to set a private property
    // @ts-ignore
    return app.selectedVoice;
  }

  function voicePackInstallStatus(): {[language: string]: VoicePackStatus} {
    // Bypass Typescript compiler to allow us to set a private property
    // @ts-ignore
    return app.voicePackInstallStatus;
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
    });

    test('to the stored voice for this language if there is one', () => {
      chrome.readingMode.getStoredVoice = () => otherVoice.name;
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
        test('to the device default if there\'s no current voice', () => {
          app.languageChanged();
          assertEquals(selectedVoice(), defaultVoice);
        });
      });

      suite('and this locale is enabled', () => {
        test('to the default voice for this language', () => {
          // @ts-ignore
          app.enabledLanguagesInPref = [lang1];
          chrome.readingMode.baseLanguageForSpeech = lang1;
          app.languageChanged();
          assertEquals(selectedVoice(), defaultVoiceWithLang1);
        });

        test(
            'to the first listed voice for this language if there\'s no default',
            () => {
              // @ts-ignore
              app.enabledLanguagesInPref = [lang2];
              chrome.readingMode.baseLanguageForSpeech = lang2;
              app.languageChanged();
              assertEquals(selectedVoice(), firstVoiceWithLang2);
            });
      });

      suite('and this locale is disabled', () => {
        test('to voice in different locale and same language', () => {
          const voice = {lang: 'en-GB', name: 'British', default: true} as
              SpeechSynthesisVoice;
          // @ts-ignore
          app.enabledLanguagesInPref = ['en-gb'];
          // @ts-ignore
          app.availableVoices = [voice];
          flush();
          chrome.readingMode.baseLanguageForSpeech = 'en-US';

          app.languageChanged();

          assertEquals(selectedVoice(), voice);
        });

        test('to default enabled voice if no same locale', () => {
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

    function setInstallStatus(lang: string, status: VoicePackStatus) {
      // @ts-ignore
      app.setVoicePackStatus_(lang, status);
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
      assertEquals(
          voicePackInstallStatus()[chrome.readingMode.baseLanguageForSpeech],
          VoicePackStatus.NONE);
    });

    test('but doesn\'t if the pack was removed by the user', () => {
      const lang = 'ko';
      chrome.readingMode.baseLanguageForSpeech = lang;
      setInstallStatus(lang, VoicePackStatus.REMOVED_BY_USER);

      app.languageChanged();

      assertFalse(sentRequest);
      assertEquals(
          voicePackInstallStatus()[lang], VoicePackStatus.REMOVED_BY_USER);
    });

    test('and refreshes voice list if already downloaded', () => {
      const lang = 'it';
      chrome.readingMode.baseLanguageForSpeech = lang;
      app.synth = new FakeSpeechSynthesis();
      const voices = app.synth.getVoices();
      app.synth.getVoices = () => {
        return voices.concat(
            {lang: lang, name: 'Wall-e (Natural)'} as SpeechSynthesisVoice,
            {lang: lang, name: 'Andy (Natural)'} as SpeechSynthesisVoice,
        );
      };
      setInstallStatus(lang, VoicePackStatus.DOWNLOADED);

      app.languageChanged();

      assertFalse(sentRequest);
      assertEquals(voicePackInstallStatus()[lang], VoicePackStatus.INSTALLED);
    });

    test('and gets voice pack info if no status yet', () => {
      const lang = 'bn';
      chrome.readingMode.baseLanguageForSpeech = lang;

      app.languageChanged();

      assertTrue(sentRequest);
      assertEquals(voicePackInstallStatus()[lang], VoicePackStatus.EXISTS);
    });

    test('and gets voice pack info if we know it exists', () => {
      const lang = 'de';
      chrome.readingMode.baseLanguageForSpeech = lang;

      app.languageChanged();

      assertTrue(sentRequest);
      assertEquals(voicePackInstallStatus()[lang], VoicePackStatus.EXISTS);
    });
  });

});
