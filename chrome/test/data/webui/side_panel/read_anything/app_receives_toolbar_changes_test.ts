// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {BrowserProxy, SpeechBrowserProxyImpl, SpeechController, ToolbarEvent, VoiceLanguageController, WordBoundaries} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertArrayEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {hasStyle, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {createApp, createSpeechSynthesisVoice, emitEvent, mockMetrics, setSimpleAxTreeWithText, setupBasicSpeech} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';

suite('AppReceivesToolbarChanges', () => {
  let app: AppElement;
  let speech: TestSpeechBrowserProxy;
  let metrics: TestMetricsBrowserProxy;
  let voiceLanguageController: VoiceLanguageController;
  let speechController: SpeechController;

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

  function emitPlayPause(): Promise<void> {
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);
    return microtasksFinished();
  }

  setup(async () => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    speech = new TestSpeechBrowserProxy();
    SpeechBrowserProxyImpl.setInstance(speech);
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    metrics = mockMetrics();
    voiceLanguageController = new VoiceLanguageController();
    VoiceLanguageController.setInstance(voiceLanguageController);
    speechController = new SpeechController();
    SpeechController.setInstance(speechController);
    app = await createApp();
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
    function emitLanguageToggle(lang: string) {
      emitEvent(app, ToolbarEvent.LANGUAGE_TOGGLE, {detail: {language: lang}});
    }

    test('enabled languages are added', () => {
      const firstLanguage = 'en-us';
      emitLanguageToggle(firstLanguage);
      assertTrue(voiceLanguageController.isLangEnabled(firstLanguage));
      assertTrue(chrome.readingMode.getLanguagesEnabledInPref().includes(
          firstLanguage));

      const secondLanguage = 'fr';
      emitLanguageToggle(secondLanguage);
      assertTrue(voiceLanguageController.isLangEnabled(secondLanguage));
      assertTrue(chrome.readingMode.getLanguagesEnabledInPref().includes(
          secondLanguage));
    });

    test('disabled languages are removed', () => {
      const firstLanguage = 'en-us';
      emitLanguageToggle(firstLanguage);
      assertTrue(voiceLanguageController.isLangEnabled(firstLanguage));
      assertTrue(chrome.readingMode.getLanguagesEnabledInPref().includes(
          firstLanguage));

      emitLanguageToggle(firstLanguage);
      assertFalse(voiceLanguageController.isLangEnabled(firstLanguage));
      assertFalse(chrome.readingMode.getLanguagesEnabledInPref().includes(
          firstLanguage));
    });
  });

  test('on speech rate change speech rate updated', async () => {
    setupBasicSpeech(speech);
    setSimpleAxTreeWithText('we mean no harm');
    app.updateContent();
    await emitPlayPause();

    const speechRate1 = 2;
    chrome.readingMode.speechRate = speechRate1;
    emitEvent(app, ToolbarEvent.RATE);
    assertEquals(2, speech.getCallCount('speak'));

    const speechRate2 = 0.5;
    chrome.readingMode.speechRate = speechRate2;
    emitEvent(app, ToolbarEvent.RATE);
    assertEquals(3, speech.getCallCount('speak'));

    const speechRate3 = 4;
    chrome.readingMode.speechRate = speechRate3;
    emitEvent(app, ToolbarEvent.RATE);
    assertEquals(4, speech.getCallCount('speak'));

    const speechRates =
        speech.getArgs('speak').map(utterance => utterance.rate);
    assertArrayEquals([1, 2, 0.5, 4], speechRates);
  });

  test('on voice selected, current voice updated', () => {
    const voice = createSpeechSynthesisVoice({lang: 'es-us', name: 'Poodle'});
    emitEvent(app, ToolbarEvent.VOICE, {detail: {selectedVoice: voice}});
    assertEquals(voice, voiceLanguageController.getCurrentVoice());
  });

  suite('play/pause', () => {
    setup(() => {
      app.updateContent();
      return microtasksFinished();
    });

    function emitPlayPause(): Promise<void> {
      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      return microtasksFinished();
    }

    test('on first click starts speech', async () => {
      setSimpleAxTreeWithText('We come in peace');
      await emitPlayPause();
      assertTrue(speechController.isSpeechActive());
      assertTrue(speechController.isSpeechTreeInitialized());
      assertTrue(speechController.hasSpeechBeenTriggered());
    });

    test('on second click stops speech', async () => {
      setSimpleAxTreeWithText('Don\'t be alarmed!');
      await emitPlayPause();
      await emitPlayPause();

      assertFalse(speechController.isSpeechActive());
      assertTrue(speechController.isSpeechTreeInitialized());
      assertTrue(speechController.hasSpeechBeenTriggered());
    });

    suite('on keyboard k pressed', () => {
      let kPress: KeyboardEvent;

      setup(() => {
        kPress = new KeyboardEvent('keydown', {key: 'k'});
      });

      test('first press plays', async () => {
        app.$.appFlexParent.dispatchEvent(kPress);
        await microtasksFinished();

        assertTrue(speechController.isSpeechActive());
        assertEquals(0, metrics.getCallCount('recordSpeechStopSource'));
      });

      test('second press pauses', async () => {
        app.$.appFlexParent.dispatchEvent(kPress);
        app.$.appFlexParent.dispatchEvent(kPress);
        await microtasksFinished();

        assertFalse(speechController.isSpeechActive());
        assertEquals(
            chrome.readingMode.keyboardShortcutStopSource,
            await metrics.whenCalled('recordSpeechStopSource'));
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
    }

    setup(() => {
      emitColorTheme(chrome.readingMode.defaultTheme);
      app.updateContent();
      emitPlayPause();
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

  suite('with highlight granularity menu', () => {
    function highlightColor(): string {
      return window.getComputedStyle(app.$.container)
          .getPropertyValue('--current-highlight-bg-color');
    }

    function emitHighlight(granularity: number) {
      emitEvent(app, ToolbarEvent.HIGHLIGHT_CHANGE, {
        detail: {data: granularity},
      });
    }

    setup(() => {
      chrome.readingMode.isPhraseHighlightingEnabled = true;
      WordBoundaries.getInstance().updateBoundary(7);
      app.updateContent();
    });

    test('updates highlight', () => {
      emitHighlight(chrome.readingMode.wordHighlighting);
      emitPlayPause();

      assertEquals(
          chrome.readingMode.wordHighlighting,
          chrome.readingMode.highlightGranularity);

      emitHighlight(chrome.readingMode.phraseHighlighting);
      assertEquals(
          chrome.readingMode.phraseHighlighting,
          chrome.readingMode.highlightGranularity);

      emitHighlight(chrome.readingMode.noHighlighting);
      assertEquals(
          chrome.readingMode.noHighlighting,
          chrome.readingMode.highlightGranularity);
    });

    test('new theme uses colored highlight with highlights on', () => {
      emitHighlight(chrome.readingMode.phraseHighlighting);
      emitColorTheme(chrome.readingMode.blueTheme);
      assertNotEquals('transparent', highlightColor());
    });

    test('new theme uses transparent highlight with highlights off', () => {
      emitHighlight(chrome.readingMode.noHighlighting);
      emitColorTheme(chrome.readingMode.yellowTheme);
      assertEquals('transparent', highlightColor());
    });
  });

  suite('on granularity change', () => {
    setup(() => {
      app.updateContent();
    });

    test('next highlights text', () => {
      emitEvent(app, ToolbarEvent.NEXT_GRANULARITY);
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertTrue(!!currentHighlight!.textContent);
    });

    test('previous highlights text', () => {
      emitEvent(app, ToolbarEvent.PREVIOUS_GRANULARITY);
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertTrue(!!currentHighlight!.textContent);
    });
  });
});
