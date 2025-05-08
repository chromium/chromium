// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy, SpeechBrowserProxyImpl, ToolbarEvent, VoicePackController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createAndSetVoices, createApp, createSpeechSynthesisVoice, emitEvent, setupBasicSpeech, setVoices} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';

// TODO: b/40927698 - Add more tests.
suite('PrefsTest', () => {
  let app: AppElement;
  let speech: TestSpeechBrowserProxy;
  let voicePackController: VoicePackController;

  setup(async () => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.isReadAloudEnabled = true;
    speech = new TestSpeechBrowserProxy();
    SpeechBrowserProxyImpl.setInstance(speech);
    voicePackController = new VoicePackController();
    VoicePackController.setInstance(voicePackController);
    app = await createApp();
  });


  suite('on restore settings from prefs', () => {
    setup(() => {
      // We are not testing the toolbar for this suite.
      app.$.toolbar.restoreSettingsFromPrefs = () => {};
    });

    test('removes unavailable languages from prefs', () => {
      const previouslyAvailableLang = 'pt-pt';
      chrome.readingMode.onLanguagePrefChange(previouslyAvailableLang, true);
      setupBasicSpeech(app, speech);

      app.restoreSettingsFromPrefs();

      assertFalse(voicePackController.isLangEnabled(previouslyAvailableLang));
      assertFalse(chrome.readingMode.getLanguagesEnabledInPref().includes(
          previouslyAvailableLang));
    });

    test('adds initially populated languages to prefs', () => {
      const previouslyAvailableLang = 'pt-pt';
      const availableLang = 'pt-br';
      chrome.readingMode.onLanguagePrefChange(previouslyAvailableLang, true);
      createAndSetVoices(app, speech, [
        {lang: availableLang, name: 'Google Galinda'},
      ]);

      app.restoreSettingsFromPrefs();

      assertFalse(voicePackController.isLangEnabled(previouslyAvailableLang));
      assertFalse(chrome.readingMode.getLanguagesEnabledInPref().includes(
          previouslyAvailableLang));
      assertTrue(voicePackController.isLangEnabled(availableLang));
      assertTrue(chrome.readingMode.getLanguagesEnabledInPref().includes(
          availableLang));
    });

    // <if expr="not is_chromeos">
    test('adds unavailable language to prefs once available', () => {
      const previouslyAvailableLang = 'da-dk';
      chrome.readingMode.onLanguagePrefChange(previouslyAvailableLang, true);
      createAndSetVoices(app, speech, [
        {lang: 'en-us', name: 'Google Fiyero'},
      ]);

      app.restoreSettingsFromPrefs();

      assertFalse(voicePackController.isLangEnabled(previouslyAvailableLang));
      assertFalse(chrome.readingMode.getLanguagesEnabledInPref().includes(
          previouslyAvailableLang));

      // The previously unavailable language is now available.
      createAndSetVoices(app, speech, [
        {lang: 'en-us', name: 'Google Fiyero'},
        {lang: 'da-dk', name: 'Doctor Dillamond'},
      ]);

      assertTrue(voicePackController.isLangEnabled(previouslyAvailableLang));
      assertTrue(chrome.readingMode.getLanguagesEnabledInPref().includes(
          previouslyAvailableLang));
    });
    // </if>

    suite('with no initial voices', () => {
      setup(() => {
        chrome.readingMode.baseLanguageForSpeech = 'en';

        // Set synthesis to have no available voices
        setVoices(app, speech, []);
        voicePackController.setCurrentVoice(null);
      });

      test('with no settings, voice selected in onVoicesChanged', () => {
        chrome.readingMode.getStoredVoice = () => '';

        // When there's no voices available, there shouldn't be a speech
        // synthesis voice selected.
        app.restoreSettingsFromPrefs();
        assertFalse(!!voicePackController.getCurrentVoice());

        // Update the speech synthesis engine with voices.
        setupBasicSpeech(app, speech);

        // Once voices are available, settings should be restored.
        assertTrue(!!voicePackController.getCurrentVoice());
      });

      test('with no settings, dfferent language voice selected', () => {
        chrome.readingMode.getStoredVoice = () => '';

        // When there's no voices available, there shouldn't be a speech
        // synthesis voice selected.
        app.restoreSettingsFromPrefs();
        assertFalse(!!voicePackController.getCurrentVoice());

        // Update the speech synthesis engine with voices.
        setupBasicSpeech(app, speech);

        // Once voices are available, settings should be restored.
        assertTrue(!!voicePackController.getCurrentVoice());
      });

      test(
          'with no initial voices and previously selected voice, correct ' +
              'voice selected after onVoicesChanged',
          () => {
            chrome.readingMode.getStoredVoice = () => 'Google Kristi';

            // When there's no voices available, there shouldn't be a speech
            // synthesis voice selected.
            app.restoreSettingsFromPrefs();
            assertFalse(!!voicePackController.getCurrentVoice());

            // Update the speech synthesis engine with voices.
            createAndSetVoices(app, speech, [
              {lang: 'en', name: 'Google Lauren'},
              {lang: 'en', name: 'Google Eitan'},
              {lang: 'en-uk', name: 'Google Kristi'},
            ]);

            // Once voices are available, settings should be restored.
            const selectedVoice = voicePackController.getCurrentVoice();
            assertTrue(!!selectedVoice);
            assertEquals('Google Kristi', selectedVoice.name);
          });

      test(
          'onVoicesChanged after settings restored, settings aren\'t updated',
          () => {
            chrome.readingMode.getStoredVoice = () => 'Google Shari';

            // When there's no voices available, there shouldn't be a speech
            // synthesis voice selected.
            app.restoreSettingsFromPrefs();
            assertFalse(!!voicePackController.getCurrentVoice());

            const futureSelectedVoice =
                createSpeechSynthesisVoice({lang: 'en', name: 'Google Kristi'});

            // Update the speech synthesis engine with voices.
            setVoices(app, speech, [
              createSpeechSynthesisVoice({lang: 'en', name: 'Google Lauren'}),
              createSpeechSynthesisVoice({lang: 'en', name: 'Google Shari'}),
              futureSelectedVoice,
            ]);

            // Once voices are available, settings should be restored.
            let selectedVoice = voicePackController.getCurrentVoice();
            assertTrue(!!selectedVoice);
            assertEquals('Google Shari', selectedVoice.name);

            emitEvent(
                app, ToolbarEvent.VOICE,
                {detail: {selectedVoice: futureSelectedVoice}});
            selectedVoice = voicePackController.getCurrentVoice();
            assertTrue(!!selectedVoice);
            assertEquals('Google Kristi', selectedVoice.name);

            // We have to update the stored voice so onVoicesChanged recognizes
            // a user chosen voice.
            chrome.readingMode.getStoredVoice = () => 'Google Kristi';
            app.onVoicesChanged();

            // After onVoicesChanged, the most recently selected voice should
            // be used.
            selectedVoice = voicePackController.getCurrentVoice();
            assertTrue(!!selectedVoice);
            assertEquals('Google Kristi', selectedVoice.name);
          });
    });

    suite('populates enabled languages', () => {
      const langs = ['si', 'km', 'th'];
      const locales = ['si-lk', 'km-kh', 'th-th'];

      setup(() => {
        createAndSetVoices(app, speech, [
          {lang: langs[0], name: 'Google Frodo'},
          {lang: langs[1], name: 'Google Merry'},
          {lang: langs[2], name: 'Google Pippin'},
        ]);
      });

      test('with langs stored in prefs', () => {
        chrome.readingMode.getLanguagesEnabledInPref = () => langs;

        app.restoreSettingsFromPrefs();

        assertArrayEquals(
            langs.concat(locales), voicePackController.getEnabledLangs());
      });

      test('with browser lang', () => {
        chrome.readingMode.baseLanguageForSpeech = langs[1]!;

        app.restoreSettingsFromPrefs();

        assertArrayEquals(
            [langs[1], locales[1]], voicePackController.getEnabledLangs());
      });
    });
  });
});
