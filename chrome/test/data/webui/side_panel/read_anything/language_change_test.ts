// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';

import {BrowserProxy} from '//resources/cr_components/color_change_listener/browser_proxy.js';
import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {ReadAnythingElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/app.js';
import {PACK_MANAGER_SUPPORTED_LANGS_AND_LOCALES, VoicePackStatus} from 'chrome-untrusted://read-anything-side-panel.top-chrome/voice_language_util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
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

  suite('updates selected voice', () => {
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

      test('to the default voice for this language', () => {
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
  });

  test('updates voice pack status to none if unsupported', () => {
    chrome.readingMode.baseLanguageForSpeech = 'zh';
    let sentRequest = false;
    chrome.readingMode.sendGetVoicePackInfoRequest = () => {
      sentRequest = true;
    };

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
});
