// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {BrowserProxy, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {convertLangOrLocaleForVoicePackManager, VoiceClientSideStatusCode, VoicePackServerStatusErrorCode, VoicePackServerStatusSuccessCode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createAndSetVoices, createSpeechSynthesisVoice, emitEvent, setVoices} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {FakeSpeechSynthesis} from './fake_speech_synthesis.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('UpdateVoicePack', () => {
  let app: AppElement;
  let speechSynthesis: FakeSpeechSynthesis;

  function setNaturalVoicesForLang(lang: string) {
    createAndSetVoices(app, speechSynthesis, [
      {lang: lang, name: 'Wall-e (Natural)'},
      {lang: lang, name: 'Andy (Natural)'},
      {lang: lang, name: 'Buzz'},
    ]);
  }

  setup(() => {
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    app = document.createElement('read-anything-app');
    document.body.appendChild(app);
    speechSynthesis = new FakeSpeechSynthesis();
    app.synth = speechSynthesis;
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

        const serverStatus =
            app.getVoicePackStatusForTesting(voicePackLang).server;
        assertEquals(
            serverStatus.code, VoicePackServerStatusSuccessCode.NOT_INSTALLED);
        assertEquals('Successful response', serverStatus.id);
        assertEquals(voicePackLang, sentInstallRequestFor);
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
      app.updateVoicePackStatus(lang, 'kNotInstalled');
      // then we request install
      app.updateVoicePackStatus(lang, 'kInstalling');

      assertFalse(toast.open);
    });

    test('does not show if error while installing', () => {
      const lang = 'en';

      // existing status
      app.updateVoicePackStatus(lang, 'kNotInstalled');
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
      app.setVoicePackLocalStatus(
          lang, VoiceClientSideStatusCode.SENT_INSTALL_REQUEST);
      app.updateVoicePackStatus(lang, 'kInstalling');
      // install completes
      app.updateVoicePackStatus(lang, 'kInstalled');

      assertTrue(toast.open);
    });

    test('shows after installed with complete language locale', () => {
      const lang = 'ja';

      // existing status
      app.updateVoicePackStatus(lang, 'kNotInstalled');
      // then we request install
      app.updateVoicePackStatus(lang, 'kInstalling');
      // install completes
      app.updateVoicePackStatus(lang, 'kInstalled');

      assertTrue(toast.open);
      assertTrue(
          toast.querySelector('#toastTitle')!.textContent!.includes('ja-jp'));
    });
  });

  test(
      'unavailable even if natural voices are in the list for a different lang',
      () => {
        const lang = 'fr';
        setNaturalVoicesForLang('it');

        app.updateVoicePackStatus(lang, 'kInstalled');

        const status = app.getVoicePackStatusForTesting(lang);
        assertEquals(
            status.server.code, VoicePackServerStatusSuccessCode.INSTALLED);
        assertEquals('Successful response', status.server.id);
        assertEquals(
            status.client, VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE);
      });

  test(
      'unavailable if non-natural voices are in the list for a different lang',
      () => {
        const lang = 'de';

        // Installed 'de' language pack, but the fake available voice list
        // only has english voices.
        app.updateVoicePackStatus(lang, 'kInstalled');

        const status = app.getVoicePackStatusForTesting(lang);
        assertEquals(
            status.server.code, VoicePackServerStatusSuccessCode.INSTALLED);
        assertEquals(
            status.client, VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE);
      });

  test(
      'unavailable if only non-natural voices are in the list for this lang',
      () => {
        const lang = 'en';

        app.updateVoicePackStatus(lang, 'kInstalled');

        const status = app.getVoicePackStatusForTesting(lang);
        assertEquals(
            status.server.code, VoicePackServerStatusSuccessCode.INSTALLED);
        assertEquals('Successful response', status.server.id);
        assertEquals(
            status.client, VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE);
      });

  test(
      'available if natural voices are unsupported for this lang and voices are available',
      () => {
        const lang = 'yue';
        createAndSetVoices(app, speechSynthesis, [
          {lang: 'yue-hk', name: 'Cantonese'},
        ]);

        app.updateVoicePackStatus(lang, 'kInstalled');

        const status = app.getVoicePackStatusForTesting(lang);
        assertEquals(
            status.server.code, VoicePackServerStatusSuccessCode.INSTALLED);
        assertEquals('Successful response', status.server.id);
        assertEquals(status.client, VoiceClientSideStatusCode.AVAILABLE);
      });

  test(
      'unavailable if natural voices are unsupported for this lang and voices unavailable',
      () => {
        const lang = 'yue';

        app.updateVoicePackStatus(lang, 'kInstalled');

        const status = app.getVoicePackStatusForTesting(lang);
        assertEquals(
            status.server.code, VoicePackServerStatusSuccessCode.INSTALLED);
        assertEquals('Successful response', status.server.id);
        assertEquals(
            status.client, VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE);
      });

  test('available if natural voices are in installed for this lang', () => {
    const lang = 'en-us';
    // set installing status so that the old status is not empty.
    app.updateVoicePackStatus(lang, 'kInstalling');
    // set the voices on speech synthesis without triggering on voices
    // changed, so we can verify that updateVoicePackStatus calls it.
    speechSynthesis.setVoices([
      createSpeechSynthesisVoice({lang: lang, name: 'Wall-e (Natural)'}),
      createSpeechSynthesisVoice({lang: lang, name: 'Andy (Natural)'}),
    ]);

    app.updateVoicePackStatus(lang, 'kInstalled');

    const status = app.getVoicePackStatusForTesting(lang);
    assertEquals(
        status.server.code, VoicePackServerStatusSuccessCode.INSTALLED);
    assertEquals('Successful response', status.server.id);
    // This would be INSTALLED_AND_UNAVIALABLE if the voice list wasn't
    // refreshed.
    assertEquals(status.client, VoiceClientSideStatusCode.AVAILABLE);
  });

  test(
      'with flag switches to newly available voices if it\'s for the current language',
      () => {
        const lang = 'en-us';
        chrome.readingMode.isLanguagePackDownloadingEnabled = true;
        chrome.readingMode.isAutoVoiceSwitchingEnabled = true;
        chrome.readingMode.baseLanguageForSpeech = lang;
        app.enabledLangs = [lang];
        chrome.readingMode.getStoredVoice = () => '';
        setNaturalVoicesForLang(lang);

        app.updateVoicePackStatus(lang, 'kInstalled');

        const selectedVoice = app.getSpeechSynthesisVoice();
        assertTrue(!!selectedVoice);
        assertEquals(lang, selectedVoice.lang);
        assertTrue(selectedVoice.name.includes('Natural'));
      });

  test(
      'with flag does not switch to newly available voices if it\'s not for the current language',
      () => {
        const installedLang = 'en-us';
        chrome.readingMode.isLanguagePackDownloadingEnabled = true;
        chrome.readingMode.isAutoVoiceSwitchingEnabled = true;
        chrome.readingMode.baseLanguageForSpeech = 'pt-br';
        app.enabledLangs = [chrome.readingMode.baseLanguageForSpeech];
        const currentVoice = createSpeechSynthesisVoice({
          name: 'Portuguese voice 1',
          lang: chrome.readingMode.baseLanguageForSpeech,
        });
        emitEvent(
            app, ToolbarEvent.VOICE, {detail: {selectedVoice: currentVoice}});
        chrome.readingMode.getStoredVoice = () => '';
        setVoices(app, speechSynthesis, [currentVoice]);

        app.updateVoicePackStatus(installedLang, 'kInstalled');

        // The selected voice should stay the same as it was.
        assertEquals(currentVoice, app.getSpeechSynthesisVoice());
      });

  test('with error code marks the status', () => {
    const lang = 'en-us';

    app.updateVoicePackStatus(lang, 'kOther');

    const status = app.getVoicePackStatusForTesting(lang);
    assertEquals(status.server.code, VoicePackServerStatusErrorCode.OTHER);
    assertEquals('Unsuccessful response', status.server.id);

    assertEquals(status.client, VoiceClientSideStatusCode.ERROR_INSTALLING);
  });

  suite('updateVoicePackStatusFromInstallResponse', () => {
    suite('with error code', () => {
      const lang = 'pt-br';

      setup(() => {
        app.enabledLangs.push(lang);
        assertTrue(app.enabledLangs.includes(lang));
      });

      test('and no other voices for language, disables language', () => {
        createAndSetVoices(app, speechSynthesis, []);
        app.updateVoicePackStatusFromInstallResponse(lang, 'kOther');
        assertFalse(app.enabledLangs.includes(lang));
      });

      test('and only eSpeak voices for language, disables language', () => {
        createAndSetVoices(app, speechSynthesis, [
          {lang: lang, name: 'eSpeak Portuguese'},
        ]);

        app.updateVoicePackStatusFromInstallResponse(lang, 'kOther');

        assertFalse(app.enabledLangs.includes(lang));
      });

      test(
          'and when language-pack lang does not match voice lang, ' +
              'still disables language',
          () => {
            app.enabledLangs.push('it-it');
            createAndSetVoices(app, speechSynthesis, []);

            app.updateVoicePackStatusFromInstallResponse('it', 'kOther');

            assertFalse(app.enabledLangs.includes('it-it'));
          });

      test(
          'and when language-pack lang does not match voice lang, with ' +
              'e-speak voices, still disables language',
          () => {
            app.enabledLangs.push('it-it');
            createAndSetVoices(app, speechSynthesis, [
              {lang: 'it', name: 'eSpeak Italian '},
            ]);

            app.updateVoicePackStatusFromInstallResponse('it', 'kOther');

            assertFalse(app.enabledLangs.includes('it-it'));
          });

      test(
          'and has other Google voices for language, keeps language enabled',
          () => {
            createAndSetVoices(app, speechSynthesis, [
              {lang: lang, name: 'ChromeOS Portuguese 1'},
              {lang: lang, name: 'ChromeOS Portuguese 2'},
            ]);
            app.onVoicesChanged();
            app.updateVoicePackStatusFromInstallResponse(lang, 'kOther');

            assertTrue(app.enabledLangs.includes(lang));
          });
    });
  });
});
