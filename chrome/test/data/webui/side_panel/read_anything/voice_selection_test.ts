// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createApp, createSpeechSynthesisVoice, setVoices} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {FakeSpeechSynthesis} from './fake_speech_synthesis.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('Automatic voice selection', () => {
  const defaultLang = 'en-us';
  const pageLang = 'en';
  const differentLang = 'zh';

  const firstVoiceWithLang = createSpeechSynthesisVoice({
    lang: defaultLang,
    name: 'Google Kristi',
  });
  const secondVoiceWithLang =
      createSpeechSynthesisVoice({lang: defaultLang, name: 'Google Lauren'});
  const defaultVoiceForDifferentLang = createSpeechSynthesisVoice({
    lang: differentLang,
    name: 'Google Eitan',
    default: true,
  });
  const voices = [
    firstVoiceWithLang,
    secondVoiceWithLang,
    defaultVoiceForDifferentLang,
  ];

  let app: AppElement;
  let speechSynthesis: FakeSpeechSynthesis;

  function addNaturalVoices() {
    setVoices(
        app, speechSynthesis,
        voices.concat(
            createSpeechSynthesisVoice(
                {lang: defaultLang, name: 'Google Wall-e (Natural)'}),
            createSpeechSynthesisVoice(
                {lang: defaultLang, name: 'Google Andy (Natural)'}),
            ));
  }

  setup(async () => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.baseLanguageForSpeech = pageLang;
    chrome.readingMode.isReadAloudEnabled = true;

    app = await createApp();
    speechSynthesis = new FakeSpeechSynthesis();
    app.synth = speechSynthesis;
    setVoices(app, speechSynthesis, voices);

    // Initializes some class variables needed for voice selection logic
    app.restoreEnabledLanguagesFromPref();
  });

  test(
      'with no user selected voices, switches to a Natural voice if it later ' +
          'becomes available',
      () => {
        chrome.readingMode.getStoredVoice = () => '';
        app.selectPreferredVoice();
        assertEquals(firstVoiceWithLang, app.getSpeechSynthesisVoice());

        addNaturalVoices();

        assertEquals(
            'Google Wall-e (Natural)', app.getSpeechSynthesisVoice()?.name);
      });

  test(
      'with a user selected voices, does not switch to a Natural voice if it ' +
          'later becomes available',
      () => {
        chrome.readingMode.getStoredVoice = () => secondVoiceWithLang.name;
        app.selectPreferredVoice();
        assertEquals(secondVoiceWithLang, app.getSpeechSynthesisVoice());

        addNaturalVoices();

        assertEquals(secondVoiceWithLang, app.getSpeechSynthesisVoice());
      });
});
