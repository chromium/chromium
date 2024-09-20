// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createSpeechSynthesisVoice, suppressInnocuousErrors} from './common.js';
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

  let testBrowserProxy: TestColorUpdaterBrowserProxy;
  let app: AppElement;

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

  test('with no user selected voices', () => {
    chrome.readingMode.getStoredVoice = () => '';
    app.selectPreferredVoice();

    // Test that it chooses the first voice with the same language
    assertEquals(firstVoiceWithLang, app.getSpeechSynthesisVoice());

    // Test that it switches to a Natural voice if it later becomes available
    const voices = app.synth.getVoices();
    app.synth.getVoices = () => {
      return voices.concat(
          createSpeechSynthesisVoice(
              {lang: defaultLang, name: 'Google Wall-e (Natural)'}),
          createSpeechSynthesisVoice(
              {lang: defaultLang, name: 'Google Andy (Natural)'}),
      );
    };
    app.onVoicesChanged();

    assertEquals(
        'Google Wall-e (Natural)', app.getSpeechSynthesisVoice()?.name);
  });

  test('with a user selected voices', () => {
    chrome.readingMode.getStoredVoice = () => secondVoiceWithLang.name;
    app.selectPreferredVoice();
    // Test that it chooses the user stored voice
    assertEquals(secondVoiceWithLang, app.getSpeechSynthesisVoice());

    // Test that it does not switch to a Natural voice when it later becomes
    // available',
    const voices = app.synth.getVoices();
    app.synth.getVoices = () => {
      return voices.concat(
          createSpeechSynthesisVoice(
              {lang: defaultLang, name: 'Google Wall-e (Natural)'}),
          createSpeechSynthesisVoice(
              {lang: defaultLang, name: 'Google Andy (Natural)'}),
      );
    };
    app.onVoicesChanged();

    assertEquals(secondVoiceWithLang, app.getSpeechSynthesisVoice());
  });
});
