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
        app.resetVoiceForTesting();
      });

      test('with no settings, voice selected in onVoicesChanged', () => {
        chrome.readingMode.getStoredVoice = () => '';

        // When there's no voices available, there shouldn't be a speech
        // synthesis voice selected.
        app.restoreSettingsFromPrefs();
        assertFalse(!!app.getSpeechSynthesisVoice());

        // Update the speech synthesis engine with voices.
        setupBasicSpeech(app, speech);

        // Once voices are available, settings should be restored.
        assertTrue(!!app.getSpeechSynthesisVoice());
      });

      test(
          'with no settings, dfferent language voice selected in onVoicesChanged',
          () => {
            chrome.readingMode.getStoredVoice = () => '';

            // When there's no voices available, there shouldn't be a speech
            // synthesis voice selected.
            app.restoreSettingsFromPrefs();
            assertFalse(!!app.getSpeechSynthesisVoice());

            // Update the speech synthesis engine with voices.
            setupBasicSpeech(app, speech);

            // Once voices are available, settings should be restored.
            assertTrue(!!app.getSpeechSynthesisVoice());
          });

      test(
          'with no initial voices and previously selected voice, correct voice selected after onVoicesChanged',
          () => {
            chrome.readingMode.getStoredVoice = () => 'Google Kristi';

            // When there's no voices available, there shouldn't be a speech
            // synthesis voice selected.
            app.restoreSettingsFromPrefs();
            assertFalse(!!app.getSpeechSynthesisVoice());

            // Update the speech synthesis engine with voices.
            createAndSetVoices(app, speech, [
              {lang: 'en', name: 'Google Lauren'},
              {lang: 'en', name: 'Google Eitan'},
              {lang: 'en-uk', name: 'Google Kristi'},
            ]);

            // Once voices are available, settings should be restored.
            const selectedVoice = app.getSpeechSynthesisVoice();
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
            assertFalse(!!app.getSpeechSynthesisVoice());

            const futureSelectedVoice =
                createSpeechSynthesisVoice({lang: 'en', name: 'Google Kristi'});

            // Update the speech synthesis engine with voices.
            setVoices(app, speech, [
              createSpeechSynthesisVoice({lang: 'en', name: 'Google Lauren'}),
              createSpeechSynthesisVoice({lang: 'en', name: 'Google Shari'}),
              futureSelectedVoice,
            ]);

            // Once voices are available, settings should be restored.
            let selectedVoice = app.getSpeechSynthesisVoice();
            assertTrue(!!selectedVoice);
            assertEquals('Google Shari', selectedVoice.name);

            emitEvent(
                app, ToolbarEvent.VOICE,
                {detail: {selectedVoice: futureSelectedVoice}});
            selectedVoice = app.getSpeechSynthesisVoice();
            assertTrue(!!selectedVoice);
            assertEquals('Google Kristi', selectedVoice.name);

            // We have to update the stored voice so onVoicesChanged recognizes
            // a user chosen voice.
            chrome.readingMode.getStoredVoice = () => 'Google Kristi';
            app.onVoicesChanged();

            // After onVoicesChanged, the most recently selected voice should
            // be used.
            selectedVoice = app.getSpeechSynthesisVoice();
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

    suite('initializes voice', () => {
      const langForDefaultVoice = 'en';
      const lang1 = 'zh';
      const lang2 = 'tr';
      const langWithNoVoices = 'elvish';

      const defaultVoice = createSpeechSynthesisVoice({
        lang: langForDefaultVoice,
        name: 'Google Kristi',
        default: true,
      });
      const firstVoiceWithLang1 =
          createSpeechSynthesisVoice({lang: lang1, name: 'Google Lauren'});
      const defaultVoiceWithLang1 = createSpeechSynthesisVoice({
        lang: lang1,
        name: 'Google Eitan',
        default: true,
      });
      const firstVoiceWithLang2 =
          createSpeechSynthesisVoice({lang: lang2, name: 'Google Yu'});
      const secondVoiceWithLang2 =
          createSpeechSynthesisVoice({lang: lang2, name: 'Google Xiang'});
      const otherVoice =
          createSpeechSynthesisVoice({lang: 'it', name: 'Google Shari'});
      const voices = [
        defaultVoice,
        firstVoiceWithLang1,
        defaultVoiceWithLang1,
        otherVoice,
        firstVoiceWithLang2,
        secondVoiceWithLang2,
      ];

      setup(() => {
        setVoices(app, speech, voices);
      });

      test('to the stored voice for this language if there is one', () => {
        chrome.readingMode.getStoredVoice = () => otherVoice.name;
        app.restoreSettingsFromPrefs();
        assertEquals(otherVoice, app.getSpeechSynthesisVoice());
      });

      test('to a default voice if the stored voice is invalid', () => {
        chrome.readingMode.getStoredVoice = () => 'Matt';
        voicePackController.enableLang(langForDefaultVoice);
        app.restoreSettingsFromPrefs();
        assertEquals(defaultVoice, app.getSpeechSynthesisVoice());
      });

      suite('when there is no stored voice for this language', () => {
        setup(() => {
          chrome.readingMode.getStoredVoice = () => '';
        });

        test('to the default voice for this language', () => {
          voicePackController.enableLang(lang1);
          voicePackController.setCurrentLanguage(lang1);
          app.restoreSettingsFromPrefs();
          assertEquals(defaultVoiceWithLang1, app.getSpeechSynthesisVoice());
        });

        test('uses current voice if there\'s none for this language', () => {
          voicePackController.setCurrentLanguage(langWithNoVoices);
          emitEvent(
              app, ToolbarEvent.VOICE, {detail: {selectedVoice: otherVoice}});
          voicePackController.enableLang(otherVoice.lang);
          app.restoreSettingsFromPrefs();
          assertEquals(otherVoice, app.getSpeechSynthesisVoice());
        });

        test('uses the device default if there\'s no current voice', () => {
          voicePackController.setCurrentLanguage(langWithNoVoices);
          voicePackController.enableLang(langForDefaultVoice);
          voicePackController.enableLang(otherVoice.lang);
          app.restoreSettingsFromPrefs();
          assertEquals(defaultVoice, app.getSpeechSynthesisVoice());
        });

        test(
            'to the first listed voice for this language if there\'s no default',
            () => {
              voicePackController.enableLang(lang2);
              voicePackController.setCurrentLanguage(lang2);
              app.restoreSettingsFromPrefs();
              const currentSelectedVoice = app.getSpeechSynthesisVoice();
              assertTrue(!!currentSelectedVoice);
              assertEquals(firstVoiceWithLang2.name, currentSelectedVoice.name);
              assertEquals(firstVoiceWithLang2.lang, currentSelectedVoice.lang);
            });
      });
    });
  });
});
