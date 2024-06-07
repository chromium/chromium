// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
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
    // @ts-ignore
    app.getVoices(true);
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
    app.getSpeechSynthesisVoice();
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

  suite('download notification', () => {
    let toast: CrToastElement;

    setup(() => {
      toast = document.querySelector<CrToastElement>('#toast')!;
      app.getSpeechSynthesisVoice();
    });

    test('does not show if already installed', () => {
      const lang = 'en';

      // The first call to update status should be the existing status from
      // the server.
      app.updateVoicePackStatus(lang, 'kInstalled');

      assertFalse(toast.open);
    });

    test('does not show if still installing', () => {
      const lang = 'en';

      // existing status
      app.updateVoicePackStatus(lang, 'kInstalled');
      // then we request install
      app.updateVoicePackStatus(lang, 'kInstalling');

      assertFalse(toast.open);
    });

    test('does not show if error while installing', () => {
      const lang = 'en';

      // existing status
      app.updateVoicePackStatus(lang, 'kInstalled');
      // then we request install
      app.updateVoicePackStatus(lang, 'kInstalling');
      // install error
      app.updateVoicePackStatus(lang, 'kOther');

      assertFalse(toast.open);
    });

    test('shows after installed', () => {
      const lang = 'en';

      // existing status
      app.updateVoicePackStatus(lang, 'kInstalled');
      // then we request install
      app.updateVoicePackStatus(lang, 'kInstalling');
      // install completes
      app.updateVoicePackStatus(lang, 'kInstalled');

      assertTrue(toast.open);
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
        const lang = 'en-us';
        // set installing status so that the old status is not empty.
        app.updateVoicePackStatus(lang, 'kInstalling');
        // Confirm en-us is not in the voice list yet
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
        // @ts-ignore
        assertTrue(app.getVoices().some(v => v.lang.toLowerCase() === lang));
        // @ts-ignore
        assertEquals(app.selectedVoice, undefined);
      });

  test(
      'with flag switches to newly available voices if it\'s for the current language',
      () => {
        const lang = 'en-us';
        chrome.readingMode.isLanguagePackDownloadingEnabled = true;
        chrome.readingMode.baseLanguageForSpeech = lang;
        // @ts-ignore
        app.selectedVoice = app.synth.getVoices()[0];
        app.$.toolbar.updateFonts = () => {};
        chrome.readingMode.isAutoVoiceSwitchingEnabled = true;
        chrome.readingMode.getStoredVoice = () => '';
        app.languageChanged();
        // Confirm en-us is not in the voice list yet
        chrome.readingMode.baseLanguageForSpeech = lang;
        assertFalse(
            app.synth.getVoices().some(v => v.lang.toLowerCase() === lang));

        addNaturalVoicesForLang(lang);
        app.updateVoicePackStatus(lang, 'kInstalled');

        // @ts-ignore
        assertEquals(app.selectedVoice.lang, lang);
        // @ts-ignore
        assertTrue(app.selectedVoice.name.includes('Natural'));
      });

  test(
      'with flag does not switch to newly available voices if it\'s not for the current language',
      () => {
        const lang = 'en-us';
        chrome.readingMode.isLanguagePackDownloadingEnabled = true;
        chrome.readingMode.baseLanguageForSpeech = 'pt-br';
        const currentVoice = {
          name: 'Portuguese voice 1',
          lang: chrome.readingMode.baseLanguageForSpeech,
        } as SpeechSynthesisVoice;
        // @ts-ignore
        app.selectedVoice = currentVoice;
        app.$.toolbar.updateFonts = () => {};
        chrome.readingMode.isAutoVoiceSwitchingEnabled = true;
        chrome.readingMode.getStoredVoice = () => '';
        app.languageChanged();
        chrome.readingMode.baseLanguageForSpeech = lang;
        assertFalse(
            app.synth.getVoices().some(v => v.lang.toLowerCase() === lang));

        addNaturalVoicesForLang(lang);
        app.updateVoicePackStatus(lang, 'kInstalled');

        // The selected voice should stay the same as it was.
        // @ts-ignore
        assertEquals(app.selectedVoice, currentVoice);
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

      function setAvailableVoices(voices: SpeechSynthesisVoice[]) {
        // @ts-ignore
        app.availableVoices = voices;
      }

      setup(() => {
        enabledLangs().push(lang);
        assertTrue(enabledLangs().includes(lang));
      });

      test('and no other voices for language, disables language', () => {
        setAvailableVoices([]);
        app.updateVoicePackStatusFromInstallResponse(lang, 'kOther');
        assertFalse(enabledLangs().includes(lang));
      });

      test('and only eSpeak voices for language, disables language', () => {
        setAvailableVoices([
          {lang: lang, name: 'eSpeak Portuguese'} as SpeechSynthesisVoice,
        ]);

        app.updateVoicePackStatusFromInstallResponse(lang, 'kOther');

        assertFalse(enabledLangs().includes(lang));
      });

      test(
          'and when language-pack lang does not match voice lang, ' +
              'still disables language',
          () => {
            enabledLangs().push('it-it');
            setAvailableVoices([]);

            app.updateVoicePackStatusFromInstallResponse('it', 'kOther');

            assertFalse(enabledLangs().includes('it-it'));
          });

      test(
          'and when language-pack lang does not match voice lang, with ' +
              'e-speak voices, still disables language',
          () => {
            enabledLangs().push('it-it');
            setAvailableVoices([
              {lang: 'it', name: 'eSpeak Italian '} as SpeechSynthesisVoice,
            ]);

            app.updateVoicePackStatusFromInstallResponse('it', 'kOther');

            assertFalse(enabledLangs().includes('it-it'));
          });

      test(
          'and has other Google voices for language, keeps language enabled',
          () => {
            setAvailableVoices([
              {lang: lang, name: 'ChromeOS Portuguese 1'} as
                  SpeechSynthesisVoice,
              {lang: lang, name: 'ChromeOS Portuguese 2'} as
                  SpeechSynthesisVoice,
            ]);
            app.updateVoicePackStatusFromInstallResponse(lang, 'kOther');

            assertTrue(enabledLangs().includes(lang));
          });
    });
  });
});
