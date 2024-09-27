// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

// <if expr="chromeos_ash">
import type {LanguageToastElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
// </if>
import {BrowserProxy, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {AppElement, NotificationType, VoiceNotificationListener} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {convertLangOrLocaleForVoicePackManager, VoiceClientSideStatusCode, VoiceNotificationManager, VoicePackServerStatusErrorCode, VoicePackServerStatusSuccessCode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

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

  suite('setVoicePackLocalStatus', () => {
    let listener: VoiceNotificationListener;
    let listenerNotified: boolean;

    setup(() => {
      listenerNotified = false;
      listener = {
        notify(_language: string, _type: NotificationType): void {
          listenerNotified = true;
        },
      };
      VoiceNotificationManager.setInstance(new VoiceNotificationManager());
      VoiceNotificationManager.getInstance().addListener(listener);
    });

    test('no notification for non-Google language', () => {
      app.setVoicePackLocalStatus(
          'zh', VoiceClientSideStatusCode.ERROR_INSTALLING);
      assertFalse(listenerNotified);
    });

    test('no notification for invalid language', () => {
      app.setVoicePackLocalStatus(
          'klingon', VoiceClientSideStatusCode.ERROR_INSTALLING);
      assertFalse(listenerNotified);
    });

    test('no notification for same status', () => {
      app.setVoicePackLocalStatus(
          'pt-br', VoiceClientSideStatusCode.ERROR_INSTALLING);
      assertTrue(listenerNotified);
      listenerNotified = false;

      app.setVoicePackLocalStatus(
          'pt-br', VoiceClientSideStatusCode.ERROR_INSTALLING);

      assertFalse(listenerNotified);
    });

    test('notifies for new status with Google-supported language', () => {
      app.setVoicePackLocalStatus(
          'it-it', VoiceClientSideStatusCode.ERROR_INSTALLING);
      assertTrue(listenerNotified);

      listenerNotified = false;
      app.setVoicePackLocalStatus('it-it', VoiceClientSideStatusCode.AVAILABLE);
      assertTrue(listenerNotified);

      listenerNotified = false;
      app.setVoicePackLocalStatus(
          'hi', VoiceClientSideStatusCode.ERROR_INSTALLING);
      assertTrue(listenerNotified);
    });
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

  // <if expr="chromeos_ash">
  suite('download notification', () => {
    const lang = 'en-us';
    let toast: LanguageToastElement;

    function installLanguage(): Promise<void> {
      setNaturalVoicesForLang(lang);
      // existing status
      app.updateVoicePackStatus(lang, 'kNotInstalled');
      // then we request install
      app.setVoicePackLocalStatus(
          lang, VoiceClientSideStatusCode.SENT_INSTALL_REQUEST);
      app.updateVoicePackStatus(lang, 'kInstalling');
      // install completes
      app.updateVoicePackStatus(lang, 'kInstalled');
      return microtasksFinished();
    }

    setup(() => {
      toast = app.$.languageToast;
      app.getSpeechSynthesisVoice();
    });

    test('does not show if already installed', async () => {
      // The first call to update status should be the existing status from
      // the server.
      app.updateVoicePackStatus(lang, 'kInstalled');
      await microtasksFinished();

      assertFalse(toast.$.toast.open);
    });

    test('does not show if still installing', async () => {
      // existing status
      app.updateVoicePackStatus(lang, 'kNotInstalled');
      // then we request install
      app.updateVoicePackStatus(lang, 'kInstalling');
      await microtasksFinished();

      assertFalse(toast.$.toast.open);
    });

    test('does not show if error while installing', async () => {
      // existing status
      app.updateVoicePackStatus(lang, 'kNotInstalled');
      // then we request install
      app.updateVoicePackStatus(lang, 'kInstalling');
      // install error
      app.updateVoicePackStatus(lang, 'kOther');
      await microtasksFinished();

      assertFalse(toast.$.toast.open);
    });

    test('shows after installed', async () => {
      await installLanguage();
      assertTrue(toast.$.toast.open);
    });

    test('does not show with language menu open', async () => {
      emitEvent(app, ToolbarEvent.LANGUAGE_MENU_OPEN);
      await installLanguage();
      assertFalse(toast.$.toast.open);
    });

    test('shows again after language menu close', async () => {
      emitEvent(app, ToolbarEvent.LANGUAGE_MENU_OPEN);
      await installLanguage();

      emitEvent(app, ToolbarEvent.LANGUAGE_MENU_CLOSE);
      await installLanguage();
      assertTrue(toast.$.toast.open);
    });
  });
  // </if>

  test(
      'unavailable even if natural voices are in the list for a different lang',
      async () => {
        const lang = 'fr';
        setNaturalVoicesForLang('it');

        app.updateVoicePackStatus(lang, 'kInstalled');
        await microtasksFinished();

        const status = app.getVoicePackStatusForTesting(lang);
        assertEquals(
            status.server.code, VoicePackServerStatusSuccessCode.INSTALLED);
        assertEquals('Successful response', status.server.id);
        assertEquals(
            status.client, VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE);
      });

  test(
      'unavailable if non-natural voices are in the list for a different lang',
      async () => {
        const lang = 'de';

        // Installed 'de' language pack, but the fake available voice list
        // only has english voices.
        app.updateVoicePackStatus(lang, 'kInstalled');
        await microtasksFinished();

        const status = app.getVoicePackStatusForTesting(lang);
        assertEquals(
            status.server.code, VoicePackServerStatusSuccessCode.INSTALLED);
        assertEquals(
            status.client, VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE);
      });

  test(
      'unavailable if only non-natural voices are in the list for this lang',
      async () => {
        const lang = 'en';

        app.updateVoicePackStatus(lang, 'kInstalled');
        await microtasksFinished();

        const status = app.getVoicePackStatusForTesting(lang);
        assertEquals(
            status.server.code, VoicePackServerStatusSuccessCode.INSTALLED);
        assertEquals('Successful response', status.server.id);
        assertEquals(
            status.client, VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE);
      });

  test(
      'available if natural voices are unsupported for this lang and voices are available',
      async () => {
        const lang = 'yue';
        createAndSetVoices(app, speechSynthesis, [
          {lang: 'yue-hk', name: 'Cantonese'},
        ]);

        app.updateVoicePackStatus(lang, 'kInstalled');
        await microtasksFinished();

        const status = app.getVoicePackStatusForTesting(lang);
        assertEquals(
            status.server.code, VoicePackServerStatusSuccessCode.INSTALLED);
        assertEquals('Successful response', status.server.id);
        assertEquals(status.client, VoiceClientSideStatusCode.AVAILABLE);
      });

  test(
      'unavailable if natural voices are unsupported for this lang and voices unavailable',
      async () => {
        const lang = 'yue';

        app.updateVoicePackStatus(lang, 'kInstalled');
        await microtasksFinished();

        const status = app.getVoicePackStatusForTesting(lang);
        assertEquals(
            status.server.code, VoicePackServerStatusSuccessCode.INSTALLED);
        assertEquals('Successful response', status.server.id);
        assertEquals(
            status.client, VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE);
      });

  test('available if natural voices are installed for this lang', async () => {
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
    await microtasksFinished();

    const status = app.getVoicePackStatusForTesting(lang);
    assertEquals(
        VoicePackServerStatusSuccessCode.INSTALLED, status.server.code);
    assertEquals('Successful response', status.server.id);
    // This would be INSTALLED_AND_UNAVIALABLE if the voice list wasn't
    // refreshed.
    assertEquals(VoiceClientSideStatusCode.AVAILABLE, status.client);
  });

  test(
      'with flag switches to newly available voices if it\'s for the current language',
      async () => {
        const lang = 'en-us';
        chrome.readingMode.isLanguagePackDownloadingEnabled = true;
        chrome.readingMode.isAutoVoiceSwitchingEnabled = true;
        chrome.readingMode.baseLanguageForSpeech = lang;
        app.enabledLangs = [lang];
        chrome.readingMode.getStoredVoice = () => '';
        setNaturalVoicesForLang(lang);
        app.updateVoicePackStatus(lang, 'kInstalled');
        await microtasksFinished();

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
        return microtasksFinished();
      });

      test('and no other voices for language, disables language', async () => {
        createAndSetVoices(app, speechSynthesis, []);
        app.updateVoicePackStatusFromInstallResponse(lang, 'kOther');
        await microtasksFinished();

        assertFalse(app.enabledLangs.includes(lang));
      });

      test(
          'and only eSpeak voices for language, disables language',
          async () => {
            createAndSetVoices(app, speechSynthesis, [
              {lang: lang, name: 'eSpeak Portuguese'},
            ]);

            app.updateVoicePackStatusFromInstallResponse(lang, 'kOther');
            await microtasksFinished();

            assertFalse(app.enabledLangs.includes(lang));
          });

      test(
          'and when language-pack lang does not match voice lang, ' +
              'still disables language',
          async () => {
            app.enabledLangs.push('it-it');
            createAndSetVoices(app, speechSynthesis, []);

            app.updateVoicePackStatusFromInstallResponse('it', 'kOther');
            await microtasksFinished();

            assertFalse(app.enabledLangs.includes('it-it'));
          });

      test(
          'and when language-pack lang does not match voice lang, with ' +
              'e-speak voices, still disables language',
          async () => {
            app.enabledLangs.push('it-it');
            createAndSetVoices(app, speechSynthesis, [
              {lang: 'it', name: 'eSpeak Italian '},
            ]);

            app.updateVoicePackStatusFromInstallResponse('it', 'kOther');
            await microtasksFinished();

            assertFalse(app.enabledLangs.includes('it-it'));
          });

      test(
          'and has other Google voices for language, keeps language enabled',
          async () => {
            createAndSetVoices(app, speechSynthesis, [
              {lang: lang, name: 'ChromeOS Portuguese 1'},
              {lang: lang, name: 'ChromeOS Portuguese 2'},
            ]);
            app.onVoicesChanged();
            app.updateVoicePackStatusFromInstallResponse(lang, 'kOther');
            await microtasksFinished();

            assertTrue(app.enabledLangs.includes(lang));
          });
    });
  });
});
