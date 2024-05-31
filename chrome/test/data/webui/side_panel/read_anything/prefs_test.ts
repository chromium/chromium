// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertArrayEquals, assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

// TODO: b/40927698 - Add more tests.
suite('PrefsTest', () => {
  let app: ReadAnythingElement;
  let testBrowserProxy: TestColorUpdaterBrowserProxy;

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
  });

  suite('on restore settings from prefs', () => {
    setup(() => {
      // We are not testing the toolbar for this suite.
      app.$.toolbar.restoreSettingsFromPrefs = () => {};
    });

    suite('populates enabled languages', () => {
      const langs = ['si', 'km', 'th'];

      function enabledLanguagesInPref(): string[] {
        // Bypass Typescript compiler to allow us to get a private property.
        // @ts-ignore
        return app.enabledLanguagesInPref;
      }

      setup(() => {
        // @ts-ignore
        app.availableVoices = [
          {lang: langs[0]!, name: ''},
          {lang: langs[1]!, name: ''},
          {lang: langs[2]!, name: ''},
        ];
        // @ts-ignore
        app.availableLangs = langs;
      });

      test('with langs stored in prefs', () => {
        chrome.readingMode.getLanguagesEnabledInPref = () => langs;

        app.restoreSettingsFromPrefs();

        assertArrayEquals(enabledLanguagesInPref(), langs);
      });

      test('with browser lang', () => {
        chrome.readingMode.baseLanguageForSpeech = langs[1]!;

        app.restoreSettingsFromPrefs();

        assertArrayEquals(enabledLanguagesInPref(), [langs[1]!]);
      });
    });

    suite('initializes voice', () => {
      const langForDefaultVoice = 'en';
      const lang1 = 'zh';
      const lang2 = 'tr';
      const langWithNoVoices = 'elvish';

      const defaultVoice = {
        lang: langForDefaultVoice,
        name: 'Kristi',
        default: true,
      } as SpeechSynthesisVoice;
      const firstVoiceWithLang1 = {lang: lang1, name: 'Lauren'} as
          SpeechSynthesisVoice;
      const defaultVoiceWithLang1 = {
        lang: lang1,
        name: 'Eitan',
        default: true,
      } as SpeechSynthesisVoice;
      const firstVoiceWithLang2 = {lang: lang2, name: 'Yu'} as
          SpeechSynthesisVoice;
      const secondVoiceWithLang2 = {lang: lang2, name: 'Xiang'} as
          SpeechSynthesisVoice;
      const otherVoice = {lang: 'it', name: 'Shari'} as SpeechSynthesisVoice;
      const voices = [
        defaultVoice,
        firstVoiceWithLang1,
        defaultVoiceWithLang1,
        otherVoice,
        firstVoiceWithLang2,
        secondVoiceWithLang2,
      ];

      function selectedVoice(): SpeechSynthesisVoice {
        // Bypass Typescript compiler to allow us to set a private property
        // @ts-ignore
        return app.selectedVoice;
      }

      setup(() => {
        // @ts-ignore
        app.availableVoices = voices;
        flush();
      });

      test('to the stored voice for this language if there is one', () => {
        chrome.readingMode.getStoredVoice = () => otherVoice.name;
        app.restoreSettingsFromPrefs();
        assertEquals(selectedVoice(), otherVoice);
      });

      test('to a default voice if the stored voice is invalid', () => {
        // @ts-ignore
        chrome.readingMode.getStoredVoice = () => 'Matt';
        // @ts-ignore
        app.enabledLanguagesInPref = [langForDefaultVoice];
        app.restoreSettingsFromPrefs();
        assertEquals(selectedVoice(), defaultVoice);
      });

      suite('when there is no stored voice for this language', () => {
        setup(() => {
          chrome.readingMode.getStoredVoice = () => '';
        });

        suite('and no voices at all for this language', () => {
          setup(() => {
            app.speechSynthesisLanguage = langWithNoVoices;
          });

          test('to the current voice if there is one', () => {
            // @ts-ignore
            app.selectedVoice = otherVoice;
            // @ts-ignore
            app.enabledLanguagesInPref = [otherVoice.lang];
            app.restoreSettingsFromPrefs();
            assertEquals(selectedVoice(), otherVoice);
          });

          test('to the device default if there\'s no current voice', () => {
            // @ts-ignore
            app.enabledLanguagesInPref = [langForDefaultVoice, otherVoice.lang];
            app.restoreSettingsFromPrefs();
            assertEquals(selectedVoice(), defaultVoice);
          });
        });

        test('to the default voice for this language', () => {
          // @ts-ignore
          app.enabledLanguagesInPref = [lang1];
          app.speechSynthesisLanguage = lang1;
          app.restoreSettingsFromPrefs();
          assertEquals(selectedVoice(), defaultVoiceWithLang1);
        });

        test(
            'to the first listed voice for this language if there\'s no default',
            () => {
              // @ts-ignore
              app.enabledLanguagesInPref = [lang2];
              app.speechSynthesisLanguage = lang2;
              app.restoreSettingsFromPrefs();
              assertEquals(selectedVoice().name, firstVoiceWithLang2.name);
              assertEquals(selectedVoice().lang, firstVoiceWithLang2.lang);
            });
      });
    });
  });
});
