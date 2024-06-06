// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {convertLangOrLocaleForVoicePackManager, VoiceClientSideStatusCode, VoicePackServerStatusErrorCode, VoicePackServerStatusSuccessCode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {VoicePackStatus} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {FakeReadingMode} from './fake_reading_mode.js';
import {FakeSpeechSynthesis} from './fake_speech_synthesis.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('UpdateVoicePack', () => {
  let app: ReadAnythingElement;

  function getVoicePackServerInstallStatus(lang: string): VoicePackStatus {
    const convertedLang: string|undefined =
        convertLangOrLocaleForVoicePackManager(lang);
    // @ts-ignore
    return app.voicePackInstallStatusServerResponses[convertedLang!];
  }

  function getVoicePackLocalStatus(lang: string): VoiceClientSideStatusCode {
    // @ts-ignore
    return app.getVoicePackLocalStatus_(lang);
  }

  function addNaturalVoicesForLang(lang: string) {
    const voices = app.synth.getVoices();
    app.synth.getVoices = () => {
      return voices.concat(
          {lang: lang, name: 'Wall-e (Natural)'} as SpeechSynthesisVoice,
          {lang: lang, name: 'Andy (Natural)'} as SpeechSynthesisVoice,
      );
    };
  }

  function enabledLangs(): string[] {
    // @ts-ignore
    return app.enabledLanguagesInPref;
  }

  setup(() => {
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    app = document.createElement('read-anything-app');
    document.body.appendChild(app);
    app.synth = new FakeSpeechSynthesis();
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
            getVoicePackServerInstallStatus(voicePackLang).code,
            VoicePackServerStatusSuccessCode.NOT_INSTALLED);
        assertEquals(
            getVoicePackServerInstallStatus(lang).id, 'Successful response');
        assertEquals(sentInstallRequestFor, voicePackLang);
      });
    });
  });

  test(
      'unavailable even if natural voices are in the list for a different lang',
      () => {
        const lang = 'fr';
        addNaturalVoicesForLang('it');

        app.updateVoicePackStatus(lang, 'kInstalled');

        assertEquals(
            getVoicePackServerInstallStatus(lang).code,
            VoicePackServerStatusSuccessCode.INSTALLED);
        assertEquals(
            getVoicePackServerInstallStatus(lang).id, 'Successful response');
        assertEquals(
            getVoicePackLocalStatus(lang),
            VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE);
      });

  test(
      'unavailable if non-natural voices are in the list for a different lang',
      () => {
        const lang = 'de';

        // Installed 'de' language pack, but the fake available voice list
        // only has english voices.
        app.updateVoicePackStatus(lang, 'kInstalled');

        assertEquals(
            getVoicePackServerInstallStatus(lang).code,
            VoicePackServerStatusSuccessCode.INSTALLED);
        assertEquals(
            getVoicePackLocalStatus(lang),
            VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE);
      });

  test('installed if non-natural voices are in the list for this lang', () => {
    const lang = 'en';

    app.updateVoicePackStatus(lang, 'kInstalled');

    assertEquals(
        getVoicePackServerInstallStatus(lang).code,
        VoicePackServerStatusSuccessCode.INSTALLED);
    assertEquals(
        getVoicePackServerInstallStatus(lang).id, 'Successful response');
    assertEquals(
        getVoicePackLocalStatus(lang),
        VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE);
  });

  test(
      'refreshes getVoices() and marks newly available voices as available',
      () => {
        // Confirm en-us is not in the voice list yet
        const lang = 'en-us';
        assertFalse(
            app.synth.getVoices().some(v => v.lang.toLowerCase() === lang));

        addNaturalVoicesForLang(lang);
        app.updateVoicePackStatus(lang, 'kInstalled');

        // Confirm that updateVoicePackStatus refreshes the voice list and marks
        // the language as available
        assertEquals(
            getVoicePackServerInstallStatus(lang).code,
            VoicePackServerStatusSuccessCode.INSTALLED);
        assertEquals(
            getVoicePackServerInstallStatus(lang).id, 'Successful response');

        assertEquals(
            getVoicePackLocalStatus(lang), VoiceClientSideStatusCode.AVAILABLE);
      });

  test('with error code marks the status', () => {
    const lang = 'en-us';
    app.updateVoicePackStatus(lang, 'kOther');
    assertEquals(
        getVoicePackServerInstallStatus(lang).code,
        VoicePackServerStatusErrorCode.OTHER);
    assertEquals(
        getVoicePackServerInstallStatus(lang).id, 'Unsuccessful response');

    assertEquals(
        getVoicePackLocalStatus(lang),
        VoiceClientSideStatusCode.ERROR_INSTALLING);
  });

  suite('updateVoicePackStatusFromInstallResponse', () => {
    suite('with error code', () => {
      const lang = 'pt-br';

      setup(() => {
        enabledLangs().push(lang);
        assertTrue(enabledLangs().includes(lang));
      });

      test('and no other voices for language, disables language', () => {
        app.synth.getVoices = () => [];
        app.updateVoicePackStatusFromInstallResponse(lang, 'kOther');
        assertFalse(enabledLangs().includes(lang));
      });

      test('and only eSpeak voices for language, disables language', () => {
        app.synth.getVoices = () => {
          return [
            {lang: lang, name: 'eSpeak Portuguese'} as SpeechSynthesisVoice,
          ];
        };

        app.updateVoicePackStatusFromInstallResponse(lang, 'kOther');

        assertFalse(enabledLangs().includes(lang));
      });

      test(
          'and has other Google voices for language, keeps language enabled',
          () => {
            app.synth.getVoices = () => {
              return [
                {lang: lang, name: 'ChromeOS Portuguese 1'} as
                    SpeechSynthesisVoice,
                {lang: lang, name: 'ChromeOS Portuguese 2'} as
                    SpeechSynthesisVoice,
              ];
            };
            app.updateVoicePackStatusFromInstallResponse(lang, 'kOther');

            assertTrue(enabledLangs().includes(lang));
          });
    });
  });
});
