// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {ReadAnythingElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {defaultFontName, FONT_EVENT, FONT_SIZE_EVENT, HIGHLIGHT_TOGGLE_EVENT, LANGUAGE_TOGGLE_EVENT, LETTER_SPACING_EVENT, LINE_SPACING_EVENT, NEXT_GRANULARITY_EVENT, PLAY_PAUSE_EVENT, PREVIOUS_GRANULARITY_EVENT, RATE_EVENT, THEME_EVENT, VoiceClientSideStatusCode, VoicePackServerStatusErrorCode, VoicePackServerStatusSuccessCode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {emitEvent, suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {FakeSpeechSynthesis} from './fake_speech_synthesis.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('AppReceivesToolbarChanges', () => {
  let testBrowserProxy: TestColorUpdaterBrowserProxy;
  let app: ReadAnythingElement;

  setup(() => {
    suppressInnocuousErrors();
    testBrowserProxy = new TestColorUpdaterBrowserProxy();
    BrowserProxy.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    app = document.createElement('read-anything-app');
    document.body.appendChild(app);
  });

  suite('on letter spacing change', () => {
    function containerLetterSpacing(): number {
      return +window.getComputedStyle(app.$.container)
                  .getPropertyValue('--letter-spacing')
                  .replace('em', '');
    }

    function emitLetterSpacing(spacing: number): void {
      emitEvent(app, LETTER_SPACING_EVENT, {detail: {data: spacing}});
    }

    test('container letter spacing updated', () => {
      const letterSpacing1 = 0.5;
      emitLetterSpacing(letterSpacing1);
      assertEquals(letterSpacing1, containerLetterSpacing());

      const letterSpacing2 = 1.2;
      emitLetterSpacing(letterSpacing2);
      assertEquals(letterSpacing2, containerLetterSpacing());

      const letterSpacing3 = 2;
      emitLetterSpacing(letterSpacing3);
      assertEquals(letterSpacing3, containerLetterSpacing());
    });
  });

  suite('on line spacing change', () => {
    function containerLineSpacing(): number {
      return +window.getComputedStyle(app.$.container)
                  .getPropertyValue('--line-height');
    }

    function emitLineSpacing(spacing: number): void {
      emitEvent(app, LINE_SPACING_EVENT, {detail: {data: spacing}});
    }

    test('container line spacing updated', () => {
      const lineSpacing1 = 0.5;
      emitLineSpacing(lineSpacing1);
      assertEquals(lineSpacing1, containerLineSpacing());

      const lineSpacing2 = 1.2;
      emitLineSpacing(lineSpacing2);
      assertEquals(lineSpacing2, containerLineSpacing());

      const lineSpacing3 = 2;
      emitLineSpacing(lineSpacing3);
      assertEquals(lineSpacing3, containerLineSpacing());
    });
  });

  suite('on font size change', () => {
    function containerFontSize(): number {
      return +window.getComputedStyle(app.$.container)
                  .getPropertyValue('--font-size')
                  .replace('em', '');
    }

    function emitFontSize(): void {
      emitEvent(app, FONT_SIZE_EVENT);
    }

    test('container font size updated', () => {
      const fontSize1 = 12;
      chrome.readingMode.fontSize = fontSize1;
      emitFontSize();
      assertEquals(fontSize1, containerFontSize());

      const fontSize2 = 16;
      chrome.readingMode.fontSize = fontSize2;
      emitFontSize();
      assertEquals(fontSize2, containerFontSize());

      const fontSize3 = 9;
      chrome.readingMode.fontSize = fontSize3;
      emitFontSize();
      assertEquals(fontSize3, containerFontSize());
    });
  });

  suite('on color theme change', () => {
    const colors = ['-yellow', '-blue', '-light', '-dark'];
    let updatedStyles: string[];

    setup(() => {
      app = document.createElement('read-anything-app');
      document.body.appendChild(app);

      // The actual theme colors we use are color constants the test doesn't
      // have access to, so we use this to verify that we update the styles with
      // every color
      app.updateStyles = (styles: any) => {
        updatedStyles = [];
        for (const [name, value] of Object.entries(styles)) {
          // The empty state body doesn't depend on the color suffix
          if (!name.includes('sp-empty-state-body-color')) {
            updatedStyles.push(value as string);
          }
        }
      };
    });

    function assertColorsChanged(suffix: string): void {
      for (const style of updatedStyles) {
        assertTrue(
            style.includes(suffix), style + 'does not contain ' + suffix);
      }
    }

    function assertDefaultColorsUsed(): void {
      for (const style of updatedStyles) {
        for (const color of colors) {
          assertFalse(style.includes(color), style + 'contains ' + color);
        }
      }
    }

    function emitColorTheme(color: string): void {
      emitEvent(app, THEME_EVENT, {detail: {data: color}});
      flush();
    }

    test('color theme updates container colors', () => {
      for (const color of colors) {
        emitColorTheme(color);
        assertColorsChanged(color);
      }
    });

    test('default theme uses default colors', () => {
      emitColorTheme('');
      assertDefaultColorsUsed();
    });
  });

  suite('on font change', () => {
    function containerFont(): string {
      return window.getComputedStyle(app.$.container)
          .getPropertyValue('font-family');
    }

    function emitFont(fontName: string): void {
      emitEvent(app, FONT_EVENT, {detail: {fontName}});
    }

    function assertFontsEqual(actual: string, expected: string): void {
      assertEquals(
          expected.trim().toLowerCase().replaceAll('"', ''),
          actual.trim().toLowerCase().replaceAll('"', ''));
    }

    test('valid font updates container font', () => {
      const font1 = 'Andika';
      emitFont(font1);
      assertFontsEqual(containerFont(), font1);

      const font2 = 'Comic Neue';
      emitFont(font2);
      assertFontsEqual(containerFont(), font2);
    });

    test('invalid font uses default', () => {
      const font1 = 'not a real font';
      emitFont(font1);
      assertFontsEqual(containerFont(), defaultFontName);

      const font2 = 'FakeFont';
      emitFont(font2);
      assertFontsEqual(containerFont(), defaultFontName);
    });

    test('unsupported font uses default', () => {
      const font1 = 'Comic Sans';
      emitFont(font1);
      assertFontsEqual(containerFont(), defaultFontName);

      const font2 = 'Times New Roman';
      emitFont(font2);
      assertFontsEqual(containerFont(), defaultFontName);
    });
  });

  suite('on language toggle', () => {
    function emitLanguageToggle(lang: string): void {
      emitEvent(app, LANGUAGE_TOGGLE_EVENT, {detail: {language: lang}});
    }

    test('enabled languages are added', () => {
      const firstLanguage = 'English';
      emitLanguageToggle(firstLanguage);
      assertTrue(app.enabledLangs.includes(firstLanguage));
      assertTrue(chrome.readingMode.getLanguagesEnabledInPref()
        .includes(firstLanguage));

      const secondLanguage = 'French';
      emitLanguageToggle(secondLanguage);
      assertTrue(app.enabledLangs.includes(secondLanguage));
      assertTrue(chrome.readingMode.getLanguagesEnabledInPref()
        .includes(secondLanguage));
    });

    test('disabled languages are removed', () => {
      const firstLanguage = 'English';
      emitLanguageToggle(firstLanguage);
      assertTrue(app.enabledLangs.includes(firstLanguage));
      assertTrue(chrome.readingMode.getLanguagesEnabledInPref()
        .includes(firstLanguage));

      emitLanguageToggle(firstLanguage);
      assertFalse(app.enabledLangs.includes(firstLanguage));
      assertFalse(chrome.readingMode.getLanguagesEnabledInPref()
        .includes(firstLanguage));
    });

    suite('with language downloading enabled', () => {
      let sentInstallRequestFor: string;

      setup(() => {
        chrome.readingMode.isLanguagePackDownloadingEnabled = true;

        sentInstallRequestFor = '';
        // Monkey patch sendInstallVoicePackRequest() to spy on the method
        chrome.readingMode.sendInstallVoicePackRequest = (language) => {
          sentInstallRequestFor = language;
        };
      });

      suite('when the previous install of the language failed', () => {
        const lang = 'en-us';
        setup(() => {
          app.setVoicePackServerStatus(lang, {
            id: 'Unsuccessful response',
            code: VoicePackServerStatusErrorCode.OTHER,
          });
        });

        test(
            'directly installs lang without usual protocol of sending status request first',
            () => {
              emitLanguageToggle(lang);

              assertEquals(lang, sentInstallRequestFor);
              assertEquals(
                  app.getVoicePackLocalStatus(lang),
                  VoiceClientSideStatusCode.SENT_INSTALL_REQUEST_ERROR_RETRY);
            });
      });

      suite('when there is no status for lang', () => {
        test('directly sends install request', () => {
          emitLanguageToggle('en-us');

          assertEquals('en-us', sentInstallRequestFor);
        });
      });


      suite('when the language status is uninstalled', () => {
        const lang = 'en-us';
        setup(() => {
          app.setVoicePackServerStatus(lang, {
            id: 'Successful response',
            code: VoicePackServerStatusSuccessCode.NOT_INSTALLED,
          });
        });

        test('does not directly install lang', () => {
          emitLanguageToggle(lang);

          assertEquals('', sentInstallRequestFor);
        });
      });
    });

  });

  suite('on speech rate change', () => {
    function emitRate(): void {
      emitEvent(app, RATE_EVENT);
    }

    test('speech rate updated', () => {
      const speechSynthesis = new FakeSpeechSynthesis();
      app.synth = speechSynthesis;
      app.playSpeech();

      const speechRate1 = 2;
      chrome.readingMode.speechRate = speechRate1;
      emitRate();
      assertTrue(speechSynthesis.spokenUtterances.every(
          utterance => utterance.rate === speechRate1));

      const speechRate2 = 0.5;
      chrome.readingMode.speechRate = speechRate2;
      emitRate();
      assertTrue(speechSynthesis.spokenUtterances.every(
          utterance => utterance.rate === speechRate2));

      const speechRate3 = 4;
      chrome.readingMode.speechRate = speechRate3;
      emitRate();
      assertTrue(speechSynthesis.spokenUtterances.every(
          utterance => utterance.rate === speechRate3));
    });
  });

  suite('play/pause', () => {
    let propagatedPauseState: boolean;

    setup(() => {
      chrome.readingMode.onSpeechPlayingStateChanged = paused => {
        propagatedPauseState = paused;
      };
      app.updateContent();
    });

    function emitPlayPause(): void {
      emitEvent(app, PLAY_PAUSE_EVENT);
    }

    suite('by default', () => {
      test('is paused', () => {
        assertTrue(app.speechPlayingState.paused);
        assertFalse(app.speechPlayingState.speechStarted);
        assertTrue(propagatedPauseState);
      });
    });

    suite('on first click', () => {
      setup(() => {
        emitPlayPause();
      });

      test('starts speech', () => {
        assertFalse(app.speechPlayingState.paused);
        assertTrue(app.speechPlayingState.speechStarted);
        assertFalse(propagatedPauseState);
      });
    });

    suite('on second click', () => {
      setup(() => {
        emitPlayPause();
        emitPlayPause();
      });

      test('stops speech', () => {
        assertTrue(app.speechPlayingState.paused);
        assertTrue(app.speechPlayingState.speechStarted);
        assertTrue(propagatedPauseState);
      });
    });

    suite('on keyboard k pressed', () => {
      let kPress: KeyboardEvent;

      setup(() => {
        kPress = new KeyboardEvent('keydown', {key: 'k'});
      });

      test('first press plays', () => {
        app.$.appFlexParent!.dispatchEvent(kPress);
        assertFalse(app.speechPlayingState.paused);
        assertFalse(propagatedPauseState);
      });

      test('second press pauses', () => {
        app.$.appFlexParent!.dispatchEvent(kPress);
        app.$.appFlexParent!.dispatchEvent(kPress);
        assertTrue(app.speechPlayingState.paused);
        assertTrue(propagatedPauseState);
      });
    });
  });

  suite('on highlight toggle', () => {
    function highlightColor(): string {
      return window.getComputedStyle(app.$.container)
          .getPropertyValue('--current-highlight-bg-color');
    }

    function emitHighlight(highlightOn: boolean): void {
      emitEvent(app, HIGHLIGHT_TOGGLE_EVENT, {detail: {highlightOn}});
    }

    setup(() => {
      emitEvent(app, THEME_EVENT, {detail: {data: ''}});
      app.updateContent();
      app.playSpeech();
    });

    test('on hide, uses transparent highlight', () => {
      emitHighlight(false);
      assertEquals('transparent', highlightColor());
    });

    test('on show, uses colored highlight', () => {
      emitHighlight(true);
      assertNotEquals('transparent', highlightColor());
    });

    suite('after update color theme', () => {
      test('uses colored highlight with highlights on', () => {
        emitHighlight(true);
        emitEvent(app, THEME_EVENT, {detail: {data: '-blue'}});
        assertNotEquals('transparent', highlightColor());
      });

      test('uses transparent highlight with highlights off', () => {
        emitHighlight(false);
        emitEvent(app, THEME_EVENT, {detail: {data: '-yellow'}});
        assertEquals('transparent', highlightColor());
      });
    });
  });

  suite('on granularity change', () => {
    setup(() => {
      app.updateContent();
    });

    function emitNextGranularity(): void {
      emitEvent(app, NEXT_GRANULARITY_EVENT);
    }

    function emitPreviousGranularity(): void {
      emitEvent(app, PREVIOUS_GRANULARITY_EVENT);
    }

    suite('next', () => {
      test('propagates change', () => {
        let movedToNext = false;
        chrome.readingMode.movePositionToNextGranularity = () => {
          movedToNext = true;
        };

        emitNextGranularity();

        assertTrue(movedToNext);
      });

      test('highlights text', () => {
        emitNextGranularity();
        const currentHighlight =
            app.$.container.querySelector('.current-read-highlight');
        assertTrue(!!currentHighlight!.textContent);
      });
    });

    suite('previous', () => {
      test('propagates change', () => {
        let movedToPrevious: boolean = false;
        chrome.readingMode.movePositionToPreviousGranularity = () => {
          movedToPrevious = true;
        };

        emitPreviousGranularity();

        assertTrue(movedToPrevious);
      });

      test('highlights text', () => {
        emitPreviousGranularity();
        const currentHighlight =
            app.$.container.querySelector('.current-read-highlight');
        assertTrue(!!currentHighlight!.textContent);
      });
    });
  });
});
