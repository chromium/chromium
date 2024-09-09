// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {ToolbarEvent, VoiceClientSideStatusCode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {hasStyle, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

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

  function emitFont(fontName: string) {
    chrome.readingMode.fontName = fontName;
    emitEvent(app, ToolbarEvent.FONT);
    return microtasksFinished();
  }

  function emitFontSize(size: number) {
    chrome.readingMode.fontSize = size;
    emitEvent(app, ToolbarEvent.FONT_SIZE);
    return microtasksFinished();
  }

  function emitLineSpacing(spacingEnumValue: number) {
    chrome.readingMode.onLineSpacingChange(spacingEnumValue);
    emitEvent(app, ToolbarEvent.LINE_SPACING);
    return microtasksFinished();
  }

  function emitLetterSpacing(spacingEnumValue: number) {
    chrome.readingMode.onLetterSpacingChange(spacingEnumValue);
    emitEvent(app, ToolbarEvent.LETTER_SPACING);
    return microtasksFinished();
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

  test(
      'on letter spacing change container letter spacing updated', async () => {
        for (let letterSpacingEnum = 0; letterSpacingEnum < 4;
             letterSpacingEnum++) {
          await emitLetterSpacing(letterSpacingEnum);
          assertEquals(letterSpacingEnum, containerLetterSpacing());
        }
      });

  test('on line spacing change container line spacing updated', async () => {
    for (let lineSpacingEnum = 0; lineSpacingEnum < 4; lineSpacingEnum++) {
      await emitLineSpacing(lineSpacingEnum);
      assertEquals(lineSpacingEnum, containerLineSpacing());
    }
  });

  test('on font size change container font size updated', async () => {
    const fontSize1 = 12;
    await emitFontSize(fontSize1);
    assertEquals(fontSize1, containerFontSize());

    const fontSize2 = 16;
    await emitFontSize(fontSize2);
    assertEquals(fontSize2, containerFontSize());

    const fontSize3 = 9;
    await emitFontSize(fontSize3);
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

  test('on font change font updates container font', async () => {
    const font1 = 'Andika';
    await emitFont(font1);
    assertFontsEqual(containerFont(), font1);

    const font2 = 'Comic Neue';
    await emitFont(font2);
    assertFontsEqual(containerFont(), font2);
  });

  suite('on language toggle', () => {
    function emitLanguageToggle(lang: string) {
      emitEvent(app, ToolbarEvent.LANGUAGE_TOGGLE, {detail: {language: lang}});
      return microtasksFinished();
    }

    test('enabled languages are added', async () => {
      const firstLanguage = 'English';
      await emitLanguageToggle(firstLanguage);
      assertTrue(app.enabledLangs.includes(firstLanguage));
      assertTrue(chrome.readingMode.getLanguagesEnabledInPref()
        .includes(firstLanguage));

      const secondLanguage = 'French';
      await emitLanguageToggle(secondLanguage);
      assertTrue(app.enabledLangs.includes(secondLanguage));
      assertTrue(chrome.readingMode.getLanguagesEnabledInPref()
        .includes(secondLanguage));
    });

    test('disabled languages are removed', async () => {
      const firstLanguage = 'English';
      await emitLanguageToggle(firstLanguage);
      assertTrue(app.enabledLangs.includes(firstLanguage));
      assertTrue(chrome.readingMode.getLanguagesEnabledInPref()
        .includes(firstLanguage));

      await emitLanguageToggle(firstLanguage);
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
          async () => {
            const lang = 'en-us';
            app.updateVoicePackStatus(lang, 'kOther');
            await emitLanguageToggle(lang);

            assertEquals(lang, sentInstallRequestFor);
            assertEquals(
                app.getVoicePackStatusForTesting(lang).client,
                VoiceClientSideStatusCode.SENT_INSTALL_REQUEST_ERROR_RETRY);
          });

      test(
          'when there is no status for lang, directly sends install request',
          async () => {
            await emitLanguageToggle('en-us');

            assertEquals('en-us', sentInstallRequestFor);
          });


      test(
          'when language status is uninstalled, does not directly install lang',
          async () => {
            const lang = 'en-us';
            app.updateVoicePackStatus(lang, 'kNotInstalled');
            await emitLanguageToggle(lang);

            assertEquals('', sentInstallRequestFor);
          });
      });
  });

  suite('on speech rate change', () => {
    function emitRate() {
      emitEvent(app, ToolbarEvent.RATE);
      return microtasksFinished();
    }

    test('speech rate updated', async () => {
      const speechSynthesis = new FakeSpeechSynthesis();
      app.synth = speechSynthesis;
      app.playSpeech();

      const speechRate1 = 2;
      chrome.readingMode.speechRate = speechRate1;
      await emitRate();
      assertTrue(speechSynthesis.spokenUtterances.every(
          utterance => utterance.rate === speechRate1));

      const speechRate2 = 0.5;
      chrome.readingMode.speechRate = speechRate2;
      await emitRate();
      assertTrue(speechSynthesis.spokenUtterances.every(
          utterance => utterance.rate === speechRate2));

      const speechRate3 = 4;
      chrome.readingMode.speechRate = speechRate3;
      await emitRate();
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
      return microtasksFinished();
    });

    function emitPlayPause() {
      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      return microtasksFinished();
    }

    test('by default is paused', () => {
      assertFalse(app.speechPlayingState.isSpeechActive);
      assertFalse(propagatedActiveState);
      assertFalse(app.speechPlayingState.hasSpeechBeenTriggered);

      // isSpeechTreeInitialized is set in updateContent
      assertTrue(app.speechPlayingState.isSpeechTreeInitialized);
    });


    test('on first click starts speech', async () => {
      await emitPlayPause();
      assertTrue(app.speechPlayingState.isSpeechActive);
      assertTrue(app.speechPlayingState.isSpeechTreeInitialized);
      assertTrue(app.speechPlayingState.hasSpeechBeenTriggered);
      assertTrue(propagatedActiveState);
    });

    test('on second click stops speech', async () => {
      await emitPlayPause();
      await emitPlayPause();

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

      test('first press plays', async () => {
        app.$.appFlexParent!.dispatchEvent(kPress);
        await microtasksFinished();

        assertTrue(app.speechPlayingState.isSpeechActive);
        assertTrue(propagatedActiveState);
      });

      test('second press pauses', async () => {
        app.$.appFlexParent!.dispatchEvent(kPress);
        app.$.appFlexParent!.dispatchEvent(kPress);
        await microtasksFinished();

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

    function emitHighlight(highlightOn: boolean) {
      const highlightValue = highlightOn ? chrome.readingMode.autoHighlighting :
                                           chrome.readingMode.noHighlighting;
      chrome.readingMode.onHighlightGranularityChanged(highlightValue);
      emitEvent(app, ToolbarEvent.HIGHLIGHT_CHANGE, {
        detail: {data: highlightValue},
      });
      return microtasksFinished();
    }

    setup(() => {
      emitColorTheme(chrome.readingMode.defaultTheme);
      app.updateContent();
      app.playSpeech();
    });

    test('on hide, uses transparent highlight', async () => {
      await emitHighlight(false);
      assertEquals('transparent', highlightColor());
    });

    test('on show, uses colored highlight', async () => {
      await emitHighlight(true);
      assertNotEquals('transparent', highlightColor());
    });

    test('new theme uses colored highlight with highlights on', async () => {
      await emitHighlight(true);
      emitColorTheme(chrome.readingMode.blueTheme);
      assertNotEquals('transparent', highlightColor());
    });

    test(
        'new theme uses transparent highlight with highlights off',
        async () => {
          await emitHighlight(false);
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
