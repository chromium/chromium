// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {convertLangOrLocaleForVoicePackManager, VoicePackStatus} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

import {FakeReadingMode} from './fake_reading_mode.js';
import {FakeSpeechSynthesis} from './fake_speech_synthesis.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('UpdateVoicePack', () => {
  let app: ReadAnythingElement;

  function getInstallStatus(lang: string) {
    const convertedLang: string|undefined =
        convertLangOrLocaleForVoicePackManager(lang);
    // @ts-ignore
    return app.voicePackInstallStatus[convertedLang!];
  }

  setup(() => {
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    app = document.createElement('read-anything-app');
    document.body.appendChild(app);
  });

  suite('updateVoicePackStatus', () => {
    let sentInstallRequestFor: string = '';

    suite('voice pack not installed', () => {
      setup(() => {
        chrome.readingMode.sendInstallVoicePackRequest = (lang) => {
          sentInstallRequestFor = lang;
        };
      });

      test('request install if we need to', () => {
        const lang = 'it-it';
        chrome.readingMode.isLanguagePackDownloadingEnabled = true;
        chrome.readingMode.baseLanguageForSpeech = lang;
        app.$.toolbar.updateFonts = () => {};
        app.languageChanged();

        const voicePackLang = convertLangOrLocaleForVoicePackManager(lang)!;

        app.updateVoicePackStatus(voicePackLang, 'kNotInstalled');

        assertEquals(
            getInstallStatus(voicePackLang), VoicePackStatus.INSTALLING);
        assertEquals(sentInstallRequestFor, voicePackLang);
      });
    });
  });

  suite('voice pack status is', () => {
    function addNaturalVoicesForLang(lang: string) {
      app.synth = new FakeSpeechSynthesis();
      const voices = app.synth.getVoices();
      app.synth.getVoices = () => {
        return voices.concat(
            {lang: lang, name: 'Wall-e (Natural)'} as SpeechSynthesisVoice,
            {lang: lang, name: 'Andy (Natural)'} as SpeechSynthesisVoice,
        );
      };
    }

    test(
        'downloaded if natural voices are in the list for a different lang',
        () => {
          const lang = 'fr';
          addNaturalVoicesForLang('it');

          app.updateVoicePackStatus(lang, 'kInstalled');

          assertEquals(getInstallStatus(lang), VoicePackStatus.DOWNLOADED);
        });

    test(
        'downloaded if non-natural voices are in the list for a different lang',
        () => {
          const lang = 'de';
          app.synth = new FakeSpeechSynthesis();

          // Installed 'de' language pack, but the fake available voice list
          // only has english voices.
          app.updateVoicePackStatus(lang, 'kInstalled');

          assertEquals(getInstallStatus(lang), VoicePackStatus.DOWNLOADED);
        });

    test(
        'installed if non-natural voices are in the list for this lang', () => {
          const lang = 'en';
          app.synth = new FakeSpeechSynthesis();

          app.updateVoicePackStatus(lang, 'kInstalled');

          assertEquals(getInstallStatus(lang), VoicePackStatus.INSTALLED);
        });

    test('installed if natural voices are in the list for this lang', () => {
      const lang = 'en';
      addNaturalVoicesForLang(lang);

      app.updateVoicePackStatus(lang, 'kInstalled');

      assertEquals(getInstallStatus(lang), VoicePackStatus.INSTALLED);
    });
  });
});
