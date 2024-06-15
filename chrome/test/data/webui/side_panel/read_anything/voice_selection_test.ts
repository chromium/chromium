// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {FakeSpeechSynthesis} from './fake_speech_synthesis.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('Automatic voice selection', () => {
  const defaultLang = 'en-us';
  const pageLang = 'en';
  const differentLang = 'zh';

  const firstVoiceWithLang = {
    lang: defaultLang,
    name: 'Kristi',
  } as SpeechSynthesisVoice;
  const secondVoiceWithLang = {lang: defaultLang, name: 'Lauren'} as
      SpeechSynthesisVoice;
  const defaultVoiceForDifferentLang = {
    lang: differentLang,
    name: 'Eitan',
    default: true,
  } as SpeechSynthesisVoice;
  const voices = [
    firstVoiceWithLang,
    secondVoiceWithLang,
    defaultVoiceForDifferentLang,
  ];

  let testBrowserProxy: TestColorUpdaterBrowserProxy;
  let app: ReadAnythingElement;

  setup(() => {
    suppressInnocuousErrors();
    testBrowserProxy = new TestColorUpdaterBrowserProxy();
    BrowserProxy.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.baseLanguageForSpeech = pageLang;
    chrome.readingMode.isReadAloudEnabled = true;

    app = document.createElement('read-anything-app');
    document.body.appendChild(app);

    app.synth = new FakeSpeechSynthesis();

    app.synth.getVoices = () => voices;

    // Initializes some class variables needed for voice selection logic
    app.restoreEnabledLanguagesFromPref();
  });

  suite('with no user selected voices', () => {
    setup(() => {
      chrome.readingMode.getStoredVoice = () => '';
      app.selectPreferredVoice();
    });

    test('it chooses the first voice with the same language', () => {
      assertEquals(app.getSpeechSynthesisVoice(), firstVoiceWithLang);
    });

    test('it switches to a Natural voice if it later becomes available', () => {
      const voices = app.synth.getVoices();
      app.synth.getVoices = () => {
        return voices.concat(
            {lang: defaultLang, name: 'Wall-e (Natural)'} as
                SpeechSynthesisVoice,
            {lang: defaultLang, name: 'Andy (Natural)'} as SpeechSynthesisVoice,
        );
      };
      app.onVoicesChanged();

      assertEquals(app.getSpeechSynthesisVoice()?.name, 'Wall-e (Natural)');
    });
  });

  suite('with a user selected voices', () => {
    setup(() => {
      chrome.readingMode.getStoredVoice = () => secondVoiceWithLang.name;
      app.selectPreferredVoice();
    });
    test('it chooses the user stored voice', () => {
      assertEquals(app.getSpeechSynthesisVoice(), secondVoiceWithLang);
    });

    test(
        'it does not switch to a Natural voice when it later becomes available',
        () => {
          const voices = app.synth.getVoices();
          app.synth.getVoices = () => {
            return voices.concat(
                {lang: defaultLang, name: 'Wall-e (Natural)'} as
                    SpeechSynthesisVoice,
                {lang: defaultLang, name: 'Andy (Natural)'} as
                    SpeechSynthesisVoice,
            );
          };
          app.onVoicesChanged();

          assertEquals(app.getSpeechSynthesisVoice(), secondVoiceWithLang);
        });
  });
});
