// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// import {flush} from
// '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BrowserProxy, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createAndSetVoices, createSpeechSynthesisVoice, emitEvent, setVoices, suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {FakeSpeechSynthesis} from './fake_speech_synthesis.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

// TODO: b/40927698 - Add more tests.
suite('PrefsTest', () => {
  let app: AppElement;
  let testBrowserProxy: TestColorUpdaterBrowserProxy;
  let speechSynthesis: FakeSpeechSynthesis;

  setup(() => {
    suppressInnocuousErrors();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testBrowserProxy = new TestColorUpdaterBrowserProxy();
    BrowserProxy.setInstance(testBrowserProxy);
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.isReadAloudEnabled = true;
    app = document.createElement('read-anything-app');
    document.body.appendChild(app);
    speechSynthesis = new FakeSpeechSynthesis();
    app.synth = speechSynthesis;
  });


  suite('on restore settings from prefs', () => {
    setup(() => {
      // We are not testing the toolbar for this suite.
      app.$.toolbar.restoreSettingsFromPrefs = () => {};
    });

    suite('with no initial voices', () => {
      setup(() => {
        chrome.readingMode.baseLanguageForSpeech = 'en';

        // Set synthesis to have no available voices
        setVoices(app, speechSynthesis, []);
        app.resetVoiceForTesting();
      });

      test('with no settings, voice selected in onVoicesChanged', () => {
        chrome.readingMode.getStoredVoice = () => '';

        // When there's no voices available, there shouldn't be a speech
        // synthesis voice selected.
        app.restoreSettingsFromPrefs();
        assertFalse(!!app.getSpeechSynthesisVoice());

        // Update the speech synthesis engine with voices.
        setVoices(
            app, speechSynthesis,
            [createSpeechSynthesisVoice({lang: 'en', name: 'Google Yu'})]);
        app.onVoicesChanged();

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
            setVoices(app, speechSynthesis, [
              createSpeechSynthesisVoice({lang: 'es', name: 'Google Kristi'}),
            ]);
            app.onVoicesChanged();

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
            setVoices(app, speechSynthesis, [
              createSpeechSynthesisVoice({lang: 'en', name: 'Google Lauren'}),
              createSpeechSynthesisVoice({lang: 'en', name: 'Google Eitan'}),
              createSpeechSynthesisVoice(
                  {lang: 'en-uk', name: 'Google Kristi'}),
            ]);
            app.onVoicesChanged();

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
            assertTrue(app.shouldAttemptLanguageSettingsRestore);

            const futureSelectedVoice =
                createSpeechSynthesisVoice({lang: 'en', name: 'Google Kristi'});

            // Update the speech synthesis engine with voices.
            setVoices(app, speechSynthesis, [
              createSpeechSynthesisVoice({lang: 'en', name: 'Google Lauren'}),
              createSpeechSynthesisVoice({lang: 'en', name: 'Google Shari'}),
              futureSelectedVoice,
            ]);
            app.onVoicesChanged();
            assertFalse(app.shouldAttemptLanguageSettingsRestore);

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
            assertFalse(app.shouldAttemptLanguageSettingsRestore);

            // After onVoicesChanged, the most recently selected voice should
            // be used.
            selectedVoice = app.getSpeechSynthesisVoice();
            assertTrue(!!selectedVoice);
            assertEquals('Google Kristi', selectedVoice.name);
          });
    });

    suite('populates enabled languages', () => {
      const langs = ['si', 'km', 'th'];

      setup(() => {
        createAndSetVoices(app, speechSynthesis, [
          {lang: langs[0]},
          {lang: langs[1]},
          {lang: langs[2]},
        ]);
      });

      test('with langs stored in prefs', () => {
        chrome.readingMode.getLanguagesEnabledInPref = () => langs;

        app.restoreSettingsFromPrefs();

        assertArrayEquals(app.enabledLangs, langs);
      });

      test('with browser lang', () => {
        chrome.readingMode.baseLanguageForSpeech = langs[1]!;

        app.restoreSettingsFromPrefs();

        assertArrayEquals(app.enabledLangs, [langs[1]!]);
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
        setVoices(app, speechSynthesis, voices);
      });

      test('to the stored voice for this language if there is one', () => {
        chrome.readingMode.getStoredVoice = () => otherVoice.name;
        app.restoreSettingsFromPrefs();
        assertEquals(otherVoice, app.getSpeechSynthesisVoice());
      });

      test('to a default voice if the stored voice is invalid', () => {
        chrome.readingMode.getStoredVoice = () => 'Matt';
        app.enabledLangs = [langForDefaultVoice];
        app.restoreSettingsFromPrefs();
        assertEquals(defaultVoice, app.getSpeechSynthesisVoice());
      });

      suite('when there is no stored voice for this language', () => {
        setup(() => {
          chrome.readingMode.getStoredVoice = () => '';
        });

        test('to the default voice for this language', () => {
          app.enabledLangs = [lang1];
          app.speechSynthesisLanguage = lang1;
          app.restoreSettingsFromPrefs();
          assertEquals(defaultVoiceWithLang1, app.getSpeechSynthesisVoice());
        });

        test('uses current voice if there\'s none for this language', () => {
          app.speechSynthesisLanguage = langWithNoVoices;
          emitEvent(
              app, ToolbarEvent.VOICE, {detail: {selectedVoice: otherVoice}});
          app.enabledLangs = [otherVoice.lang];
          app.restoreSettingsFromPrefs();
          assertEquals(otherVoice, app.getSpeechSynthesisVoice());
        });

        test('uses the device default if there\'s no current voice', () => {
          app.speechSynthesisLanguage = langWithNoVoices;
          app.enabledLangs = [langForDefaultVoice, otherVoice.lang];
          app.restoreSettingsFromPrefs();
          assertEquals(defaultVoice, app.getSpeechSynthesisVoice());
        });

        test(
            'to the first listed voice for this language if there\'s no default',
            () => {
              app.enabledLangs = [lang2];
              app.speechSynthesisLanguage = lang2;
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
