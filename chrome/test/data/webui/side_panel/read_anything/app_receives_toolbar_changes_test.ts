// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {ToolbarEvent, VoiceClientSideStatusCode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {hasStyle} from 'chrome-untrusted://webui-test/test_util.js';

import {emitEvent, suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {FakeSpeechSynthesis} from './fake_speech_synthesis.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('AppReceivesToolbarChanges', () => {
  let testBrowserProxy: TestColorUpdaterBrowserProxy;
  let app: AppElement;

  function containerLetterSpacing(): number {
    return +window.getComputedStyle(app.$.container)
                .getPropertyValue('--letter-spacing')
                .replace('em', '');
  }

  function containerLineSpacing(): number {
    return +window.getComputedStyle(app.$.container)
                .getPropertyValue('--line-height');
  }

  function containerFontSize(): number {
    return +window.getComputedStyle(app.$.container)
                .getPropertyValue('--font-size')
                .replace('em', '');
  }

  function containerFont(): string {
    return window.getComputedStyle(app.$.container)
        .getPropertyValue('font-family');
  }

  function assertFontsEqual(actual: string, expected: string): void {
    assertEquals(
        expected.trim().toLowerCase().replaceAll('"', ''),
        actual.trim().toLowerCase().replaceAll('"', ''));
  }

  function emitFont(fontName: string): void {
    chrome.readingMode.fontName = fontName;
    emitEvent(app, ToolbarEvent.FONT);
  }

  function emitFontSize(size: number): void {
    chrome.readingMode.fontSize = size;
    emitEvent(app, ToolbarEvent.FONT_SIZE);
  }

  function emitLineSpacing(spacingEnumValue: number): void {
    chrome.readingMode.onLineSpacingChange(spacingEnumValue);
    emitEvent(app, ToolbarEvent.LINE_SPACING);
  }

  function emitLetterSpacing(spacingEnumValue: number): void {
    chrome.readingMode.onLetterSpacingChange(spacingEnumValue);
    emitEvent(app, ToolbarEvent.LETTER_SPACING);
  }

  function emitColorTheme(colorEnumValue: number): void {
    chrome.readingMode.onThemeChange(colorEnumValue);
    emitEvent(app, ToolbarEvent.THEME);
  }

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

  test('on letter spacing change container letter spacing updated', () => {
    for (let letterSpacingEnum = 0; letterSpacingEnum < 4;
         letterSpacingEnum++) {
      emitLetterSpacing(letterSpacingEnum);
      assertEquals(letterSpacingEnum, containerLetterSpacing());
    }
  });

  test('on line spacing change container line spacing updated', () => {
    for (let lineSpacingEnum = 0; lineSpacingEnum < 4; lineSpacingEnum++) {
      emitLineSpacing(lineSpacingEnum);
      assertEquals(lineSpacingEnum, containerLineSpacing());
    }
  });

  test('on font size change container font size updated', () => {
    const fontSize1 = 12;
    emitFontSize(fontSize1);
    assertEquals(fontSize1, containerFontSize());

    const fontSize2 = 16;
    emitFontSize(fontSize2);
    assertEquals(fontSize2, containerFontSize());

    const fontSize3 = 9;
    emitFontSize(fontSize3);
    assertEquals(fontSize3, containerFontSize());
  });

  suite('on color theme change', () => {
    setup(() => {
      app = document.createElement('read-anything-app');
      document.body.appendChild(app);
    });

    test('color theme updates container colors', () => {
      // Set background color css variables. In prod code this is done in a
      // parent element.
      app.style.setProperty(
          '--color-read-anything-background-dark', 'DarkSlateGray');
      app.style.setProperty(
          '--color-read-anything-background-light', 'LightGray');
      app.style.setProperty(
          '--color-read-anything-background-yellow', 'yellow');
      app.style.setProperty('--color-read-anything-background-blue', 'blue');

      emitColorTheme(chrome.readingMode.darkTheme);
      assertTrue(
          hasStyle(app.$.container, '--background-color', 'DarkSlateGray'));

      emitColorTheme(chrome.readingMode.lightTheme);
      assertTrue(hasStyle(app.$.container, '--background-color', 'LightGray'));

      emitColorTheme(chrome.readingMode.yellowTheme);
      assertTrue(hasStyle(app.$.container, '--background-color', 'yellow'));

      emitColorTheme(chrome.readingMode.blueTheme);
      assertTrue(hasStyle(app.$.container, '--background-color', 'blue'));
    });

    test('default theme uses default colors', () => {
      // Set background color css variables. In prod code this is done in a
      // parent element.
      app.style.setProperty('--color-sys-base-container-elevated', 'grey');
      emitColorTheme(chrome.readingMode.defaultTheme);

      assertTrue(hasStyle(app.$.container, '--background-color', 'grey'));
    });
  });

  test('on font change font updates container font', () => {
    const font1 = 'Andika';
    emitFont(font1);
    assertFontsEqual(containerFont(), font1);

    const font2 = 'Comic Neue';
    emitFont(font2);
    assertFontsEqual(containerFont(), font2);
  });

  suite('on language toggle', () => {
    function emitLanguageToggle(lang: string): void {
      emitEvent(app, ToolbarEvent.LANGUAGE_TOGGLE, {detail: {language: lang}});
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

      test(
          'when previous language install failed, directly installs lang without usual protocol of sending status request first',
          () => {
            const lang = 'en-us';
            app.updateVoicePackStatus(lang, 'kOther');
            emitLanguageToggle(lang);

            assertEquals(lang, sentInstallRequestFor);
            assertEquals(
                app.getVoicePackStatusForTesting(lang).client,
                VoiceClientSideStatusCode.SENT_INSTALL_REQUEST_ERROR_RETRY);
          });

      test(
          'when there is no status for lang, directly sends install request',
          () => {
            emitLanguageToggle('en-us');

            assertEquals('en-us', sentInstallRequestFor);
          });


      test(
          'when language status is uninstalled, does not directly install lang',
          () => {
            const lang = 'en-us';
            app.updateVoicePackStatus(lang, 'kNotInstalled');
            emitLanguageToggle(lang);

            assertEquals('', sentInstallRequestFor);
          });
      });

  });

  suite('on speech rate change', () => {
    function emitRate(): void {
      emitEvent(app, ToolbarEvent.RATE);
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
    let propagatedActiveState: boolean;

    setup(() => {
      chrome.readingMode.onSpeechPlayingStateChanged = isSpeechActive => {
        propagatedActiveState = isSpeechActive;
      };
      app.updateContent();
    });

    function emitPlayPause(): void {
      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
    }

    test('by default is paused', () => {
      assertFalse(app.speechPlayingState.isSpeechActive);
      assertFalse(propagatedActiveState);
      assertFalse(app.speechPlayingState.hasSpeechBeenTriggered);

      // isSpeechTreeInitialized is set in updateContent
      assertTrue(app.speechPlayingState.isSpeechTreeInitialized);
    });


    test('on first click starts speech', () => {
      emitPlayPause();
      assertTrue(app.speechPlayingState.isSpeechActive);
      assertTrue(app.speechPlayingState.isSpeechTreeInitialized);
      assertTrue(app.speechPlayingState.hasSpeechBeenTriggered);
      assertTrue(propagatedActiveState);
    });

    test('on second click stops speech', () => {
      emitPlayPause();
      emitPlayPause();

      assertFalse(app.speechPlayingState.isSpeechActive);
      assertTrue(app.speechPlayingState.isSpeechTreeInitialized);
      assertTrue(app.speechPlayingState.hasSpeechBeenTriggered);
      assertFalse(propagatedActiveState);
    });

    suite('on keyboard k pressed', () => {
      let kPress: KeyboardEvent;

      setup(() => {
        kPress = new KeyboardEvent('keydown', {key: 'k'});
      });

      test('first press plays', () => {
        app.$.appFlexParent!.dispatchEvent(kPress);
        assertTrue(app.speechPlayingState.isSpeechActive);
        assertTrue(propagatedActiveState);
      });

      test('second press pauses', () => {
        app.$.appFlexParent!.dispatchEvent(kPress);
        app.$.appFlexParent!.dispatchEvent(kPress);
        assertFalse(app.speechPlayingState.isSpeechActive);
        assertFalse(propagatedActiveState);
      });
    });
  });

  suite('on highlight toggle', () => {
    function highlightColor(): string {
      return window.getComputedStyle(app.$.container)
          .getPropertyValue('--current-highlight-bg-color');
    }

    function emitHighlight(highlightOn: boolean): void {
      if (highlightOn) {
        chrome.readingMode.turnedHighlightOn();
      } else {
        chrome.readingMode.turnedHighlightOff();
      }
      emitEvent(app, ToolbarEvent.HIGHLIGHT_TOGGLE);
    }

    setup(() => {
      emitColorTheme(chrome.readingMode.defaultTheme);
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

    test('new theme uses colored highlight with highlights on', () => {
      emitHighlight(true);
      emitColorTheme(chrome.readingMode.blueTheme);
      assertNotEquals('transparent', highlightColor());
    });

    test('new theme uses transparent highlight with highlights off', () => {
      emitHighlight(false);
      emitColorTheme(chrome.readingMode.yellowTheme);
      assertEquals('transparent', highlightColor());
    });
  });

  suite('on granularity change', () => {
    setup(() => {
      app.updateContent();
    });

    function emitNextGranularity(): void {
      emitEvent(app, ToolbarEvent.NEXT_GRANULARITY);
    }

    function emitPreviousGranularity(): void {
      emitEvent(app, ToolbarEvent.PREVIOUS_GRANULARITY);
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

    test('previous propagates change', () => {
      let movedToPrevious: boolean = false;
      chrome.readingMode.movePositionToPreviousGranularity = () => {
        movedToPrevious = true;
      };

      emitPreviousGranularity();

      assertTrue(movedToPrevious);
    });

    test('previous highlights text', () => {
      emitPreviousGranularity();
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertTrue(!!currentHighlight!.textContent);
    });
    });
});
