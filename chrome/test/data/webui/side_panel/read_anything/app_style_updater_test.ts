// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {AppStyleUpdater, LineFocusType} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertGT, assertNotEquals, assertStringContains} from 'chrome-untrusted://webui-test/chai_assert.js';

import {setupAppTestEnvironment} from './common.js';
import type {TestAudioBrowserProxy} from './test_audio_browser_proxy.js';
import type {TestVisualBrowserProxy} from './test_visual_browser_proxy.js';

suite('AppStyleUpdater', () => {
  let app: AppElement;
  let updater: AppStyleUpdater;
  let visualBrowserProxy: TestVisualBrowserProxy;
  let audioBrowserProxy: TestAudioBrowserProxy;

  function computeStyle(style: string) {
    return window.getComputedStyle(app.$.container).getPropertyValue(style);
  }

  function setAppFontSize(size: number) {
    app.style.fontSize = size + 'px';
  }

  function updateStyles(styles: {[attribute: string]: string}) {
    for (const [key, val] of Object.entries(styles)) {
      app.style.setProperty(key, val);
    }
  }

  setup(async () => {
    const result = await setupAppTestEnvironment();
    app = result.app;
    visualBrowserProxy = result.visualBrowserProxy;
    audioBrowserProxy = result.audioBrowserProxy;
    updater = new AppStyleUpdater(app);
  });

  test('max line width is max chars', () => {
    visualBrowserProxy.maxLineWidth = 100;
    updater.setMaxLineWidth();
    assertEquals('100ch', app.style.getPropertyValue('--max-width'));

    visualBrowserProxy.maxLineWidth = 40;
    updater.setMaxLineWidth();
    assertEquals('40ch', app.style.getPropertyValue('--max-width'));
  });

  test('setPaddingForLineFocus sets top and bottom padding', () => {
    visualBrowserProxy.lineFocusEnabled = true;
    const padding = 50;

    updater.setPaddingForLineFocus(padding);

    assertEquals(`${padding}px`, computeStyle('padding-top'));
    assertEquals(`${padding}px`, computeStyle('padding-bottom'));
    assertEquals(padding, updater.getPaddingForLineFocus());
  });

  test('line focus height depends on font scale', () => {
    visualBrowserProxy.fontSize = 1;
    updater.setLineFocusHeight();
    assertEquals('2px', app.style.getPropertyValue('--line-focus-height'));

    visualBrowserProxy.fontSize = 2;
    updater.setLineFocusHeight();
    assertEquals('4px', app.style.getPropertyValue('--line-focus-height'));
  });

  test('setLineFocusStyle with no line focus hides view', () => {
    visualBrowserProxy.lineFocusEnabled = true;

    updater.setLineFocusStyle(LineFocusType.NONE);

    assertEquals('none', app.style.getPropertyValue('--line-focus-display'));
    assertEquals('', app.style.getPropertyValue('--line-focus-shadow'));
    assertEquals('', app.style.getPropertyValue('--line-focus-bg'));
    assertEquals('', app.style.getPropertyValue('--line-focus-height'));
  });

  test('setLineFocusStyle with line focus off hides view', () => {
    visualBrowserProxy.lineFocusEnabled = true;
    visualBrowserProxy.colorTheme = visualBrowserProxy.lowContrastDarkTheme;

    updater.setLineFocusStyle(LineFocusType.NONE);

    assertEquals('none', app.style.getPropertyValue('--line-focus-display'));
    assertEquals('', app.style.getPropertyValue('--line-focus-shadow'));
    assertEquals('', app.style.getPropertyValue('--line-focus-bg'));
    assertEquals('', app.style.getPropertyValue('--line-focus-height'));
  });

  test('setLineFocusStyle with line focus line shows view', () => {
    visualBrowserProxy.lineFocusEnabled = true;
    visualBrowserProxy.colorTheme = visualBrowserProxy.lowContrastDarkTheme;

    updater.setLineFocusStyle(LineFocusType.LINE);

    assertNotEquals('none', app.style.getPropertyValue('--line-focus-display'));
    assertNotEquals('', app.style.getPropertyValue('--line-focus-shadow'));
    assertEquals(
        'var(--color-read-anything-line-focus-low-contrast-dark)',
        app.style.getPropertyValue('--line-focus-bg'));
    assertNotEquals('', app.style.getPropertyValue('--line-focus-height'));
  });

  test('setLineFocusStyle with line focus window shows view', () => {
    visualBrowserProxy.lineFocusEnabled = true;

    updater.setLineFocusStyle(LineFocusType.WINDOW);

    assertNotEquals('none', app.style.getPropertyValue('--line-focus-display'));
    assertNotEquals('', app.style.getPropertyValue('--line-focus-shadow'));
    assertNotEquals('', app.style.getPropertyValue('--line-focus-bg'));
  });

  test('setLineFocusStyle with line focus window does not set height', () => {
    visualBrowserProxy.lineFocusEnabled = true;
    updater.setLineFocusStyle(LineFocusType.WINDOW);
    assertEquals('', app.style.getPropertyValue('--line-focus-height'));
  });

  test(
      'setLineFocusStyle sets different background and shadow for different types',
      () => {
        visualBrowserProxy.lineFocusEnabled = true;
        updater.setLineFocusStyle(LineFocusType.WINDOW);
        const windowShadow = app.style.getPropertyValue('--line-focus-shadow');
        const windowBg = app.style.getPropertyValue('--line-focus-bg');

        updater.setLineFocusStyle(LineFocusType.LINE);
        const lineShadow = app.style.getPropertyValue('--line-focus-shadow');
        const lineBg = app.style.getPropertyValue('--line-focus-bg');

        assertNotEquals(windowShadow, lineShadow);
        assertNotEquals(windowBg, lineBg);
      });

  test(
      'setLineFocusStyle does not update toolbar colors if line focus is ' +
          'disabled',
      () => {
        visualBrowserProxy.lineFocusEnabled = false;
        updater.setLineFocusStyle(LineFocusType.WINDOW);
        assertEquals('', app.style.getPropertyValue('--toolbar-icon-color'));
        assertEquals(
            '', app.style.getPropertyValue('--legacy-toolbar-icon-color'));
        assertEquals(
            '', app.style.getPropertyValue('--legacy-audio-player-icon-color'));
      });

  test(
      'setLineFocusStyle sets dark toolbar icon color in immersive mode for ' +
          'window line focus',
      () => {
        visualBrowserProxy.lineFocusEnabled = true;
        updater.setLineFocusStyle(LineFocusType.WINDOW);
        assertEquals(
            'var(--color-read-anything-toolbar-icon-dark)',
            app.style.getPropertyValue('--toolbar-icon-color'));
      });

  test(
      'setLineFocusStyle sets themed toolbar icon color in immersive mode ' +
          'for non-window line focus',
      () => {
        visualBrowserProxy.lineFocusEnabled = true;
        visualBrowserProxy.colorTheme = visualBrowserProxy.yellowTheme;
        updater.setLineFocusStyle(LineFocusType.LINE);
        assertEquals(
            'var(--color-read-anything-toolbar-icon-yellow)',
            app.style.getPropertyValue('--toolbar-icon-color'));
      });

  test('setLineFocusPos sets y position', () => {
    const pos = 123;

    updater.setLineFocusPos(pos, 0);

    assertEquals(`${pos}px`, app.style.getPropertyValue('--line-focus-y'));
    assertEquals('', app.style.getPropertyValue('--line-focus-height'));
  });

  test('setLineFocusPos sets height', () => {
    const height = 456;

    updater.setLineFocusPos(0, height);

    assertEquals(
        `${height}px`, app.style.getPropertyValue('--line-focus-height'));
  });

  test('line spacing depends on font size', () => {
    visualBrowserProxy.lineSpacing = 10;

    setAppFontSize(10);
    updater.setLineSpacing();
    const lineHeight1 = parseInt(computeStyle('line-height'));

    setAppFontSize(12);
    updater.setLineSpacing();
    const lineHeight2 = parseInt(computeStyle('line-height'));

    assertGT(lineHeight2, lineHeight1);
  });

  test('paragraph spacing depends on line spacing', () => {
    setAppFontSize(10);

    visualBrowserProxy.lineSpacing = 10;
    updater.setLineSpacing();
    const lineSpacing1 = parseInt(computeStyle('line-height'));
    const pSpacing1 = parseInt(computeStyle('--paragraph-spacing'));

    visualBrowserProxy.lineSpacing = 16;
    updater.setLineSpacing();
    const lineSpacing2 = parseInt(computeStyle('line-height'));
    const pSpacing2 = parseInt(computeStyle('--paragraph-spacing'));

    assertGT(lineSpacing2, lineSpacing1);
    assertGT(pSpacing2, pSpacing1);
  });

  test('letter spacing depends on font size', () => {
    setAppFontSize(10);
    visualBrowserProxy.letterSpacing = 10;
    updater.setLetterSpacing();
    assertEquals('100px', computeStyle('letter-spacing'));

    setAppFontSize(12);
    visualBrowserProxy.letterSpacing = 16;
    updater.setLetterSpacing();
    assertEquals('192px', computeStyle('letter-spacing'));
  });

  test('word spacing depends on letter spacing', () => {
    setAppFontSize(10);

    visualBrowserProxy.letterSpacing = 10;
    updater.setLetterSpacing();
    const letterSpacing1 = +computeStyle('letter-spacing').replace('px', '');
    const wordSpacing1 = +computeStyle('word-spacing').replace('px', '');

    visualBrowserProxy.letterSpacing = 16;
    updater.setLetterSpacing();
    const letterSpacing2 = +computeStyle('letter-spacing').replace('px', '');
    const wordSpacing2 = +computeStyle('word-spacing').replace('px', '');

    assertGT(letterSpacing2, letterSpacing1);
    assertGT(wordSpacing2, wordSpacing1);
  });

  test('font size scales', () => {
    setAppFontSize(10);
    visualBrowserProxy.fontSize = 1;
    updater.setFontSize();
    assertEquals('10px', computeStyle('font-size'));

    visualBrowserProxy.fontSize = 2.5;
    updater.setFontSize();
    assertEquals('25px', computeStyle('font-size'));

    visualBrowserProxy.fontSize = 0.5;
    updater.setFontSize();
    assertEquals('5px', computeStyle('font-size'));
  });

  test('font name', () => {
    visualBrowserProxy.fontName = 'Poppins';
    updater.setFont();
    assertStringContains(
        computeStyle('font-family'), visualBrowserProxy.fontName);

    visualBrowserProxy.fontName = 'Lexend Deca';
    updater.setFont();
    assertStringContains(
        computeStyle('font-family'), visualBrowserProxy.fontName);
  });

  test('current highlight', () => {
    const expectedYellowColor = 'yellow';
    const expectedDarkColor = 'black';
    updateStyles({
      '--color-read-anything-current-read-aloud-highlight-yellow':
          expectedYellowColor,
      '--color-read-anything-current-read-aloud-highlight-dark':
          expectedDarkColor,
    });
    audioBrowserProxy.onHighlightGranularityChanged(
        audioBrowserProxy.autoHighlighting);
    visualBrowserProxy.colorTheme = visualBrowserProxy.yellowTheme;
    updater.setHighlight();
    assertEquals(
        expectedYellowColor, computeStyle('--current-highlight-bg-color'));

    visualBrowserProxy.colorTheme = visualBrowserProxy.darkTheme;
    updater.setHighlight();
    assertEquals(
        expectedDarkColor, computeStyle('--current-highlight-bg-color'));

    audioBrowserProxy.onHighlightGranularityChanged(
        audioBrowserProxy.noHighlighting);
    visualBrowserProxy.colorTheme = visualBrowserProxy.lightTheme;
    updater.setHighlight();
    assertEquals('transparent', computeStyle('--current-highlight-bg-color'));
  });

  test('color theme', () => {
    // Make each expected color distinct so we can verify each color is changed
    // with each update.
    const expectedDefaultBackground = 'rgb(0, 0, 255)';
    const expectedYellowBackground = 'rgb(0, 255, 0)';
    const expectedDarkBackground = 'rgb(0, 255, 255)';
    const expectedHighContrastBackground = 'rgb(255, 255, 0)';
    const expectedLowContrastLightBackground = 'rgb(255, 255, 255)';
    const expectedLowContrastDarkBackground = 'rgb(0, 0, 255)';
    const expectedDefaultForeground = 'rgb(255, 0, 0)';
    const expectedYellowForeground = 'rgb(255, 0, 255)';
    const expectedDarkForeground = 'rgb(255, 255, 0)';
    const expectedHighContrastForeground = 'rgb(0, 0, 0)';
    const expectedLowContrastLightForeground = 'rgb(0, 255, 0)';
    const expectedLowContrastDarkForeground = 'rgb(0, 0, 255)';
    const expectedDefaultSelectionBackground = 'rgb(255, 255, 255)';
    const expectedYellowCurrentHighlight = 'rgb(0, 0, 0)';
    const expectedDarkCurrentHighlight = 'rgb(5, 5, 100)';
    const expectedHighContrastCurrentHighlight = 'rgb(5, 100, 5)';
    const expectedLowContrastLightCurrentHighlight = 'rgb(100, 100, 5)';
    const expectedLowContrastDarkCurrentHighlight = 'rgb(100, 5, 100)';
    const expectedDefaultPreviousHighlight = 'rgb(5, 100, 5)';
    const expectedYellowPreviousHighlight = 'rgb(5, 100, 100)';
    const expectedDarkPreviousHighlight = 'rgb(100, 100, 100)';
    const expectedHighContrastPreviousHighlight = 'rgb(100, 255, 255)';
    const expectedLowContrastLightPreviousHighlight = 'rgb(255, 255, 100)';
    const expectedLowContrastDarkPreviousHighlight = 'rgb(100, 100, 255)';
    const expectedDefaultEmptyHeading = 'rgb(100, 5, 100)';
    const expectedDefaultEmptyBody = 'rgb(100, 100, 100)';
    const expectedYellowEmptyBody = 'rgb(255, 0, 255)';
    const expectedDarkEmptyBody = 'rgb(255, 255, 0)';
    const expectedHighContrastEmptyBody = 'rgb(0, 0, 0)';
    const expectedLowContrastLightEmptyBody = 'rgb(0, 255, 0)';
    const expectedLowContrastDarkEmptyBody = 'rgb(0, 0, 255)';
    const expectedDefaultLink = 'rgb(6, 37, 37)';
    const expectedYellowLink = 'rgb(37, 6, 6)';
    const expectedDarkLink = 'rgb(37, 6, 37)';
    const expectedHighContrastLink = 'rgb(6, 37, 6)';
    const expectedLowContrastLightLink = 'rgb(6, 37, 6)';
    const expectedLowContrastDarkLink = 'rgb(37, 6, 37)';
    const expectedDefaultLinkVisited = 'rgb(37, 37, 6)';
    const expectedYellowLinkVisited = 'rgb(37, 37, 37)';
    const expectedDarkLinkVisited = 'rgb(14, 14, 28)';
    const expectedHighContrastLinkVisited = 'rgb(14, 28, 14)';
    const expectedLowContrastLightLinkVisited = 'rgb(14, 28, 28)';
    const expectedLowContrastDarkLinkVisited = 'rgb(28, 14, 28)';
    const expectedDefaultLineFocus = 'rgb(100, 100, 0)';
    const expectedDarkLineFocus = 'rgb(200, 200, 0)';
    const expectedLightLineFocus = 'rgb(50, 50, 0)';
    updateStyles({
      '--color-sys-base-container-elevated': expectedDefaultBackground,
      '--color-read-anything-background-yellow': expectedYellowBackground,
      '--color-read-anything-background-dark': expectedDarkBackground,
      '--color-read-anything-background-high-contrast':
          expectedHighContrastBackground,
      '--color-read-anything-background-low-contrast-light':
          expectedLowContrastLightBackground,
      '--color-read-anything-background-low-contrast-dark':
          expectedLowContrastDarkBackground,
      '--color-sys-on-surface': expectedDefaultForeground,
      '--color-read-anything-foreground': expectedDefaultEmptyHeading,
      '--color-read-anything-foreground-yellow': expectedYellowForeground,
      '--color-read-anything-foreground-dark': expectedDarkForeground,
      '--color-read-anything-foreground-high-contrast':
          expectedHighContrastForeground,
      '--color-read-anything-foreground-low-contrast-light':
          expectedLowContrastLightForeground,
      '--color-read-anything-foreground-low-contrast-dark':
          expectedLowContrastDarkForeground,
      '--color-sys-state-focus-ring': expectedDefaultLineFocus,
      '--color-read-anything-line-focus': expectedDarkLineFocus,
      '--color-read-anything-line-focus-yellow': expectedLightLineFocus,
      '--color-read-anything-line-focus-dark': expectedDarkLineFocus,
      '--color-read-anything-line-focus-light': expectedLightLineFocus,
      '--color-read-anything-line-focus-high-contrast': expectedDarkLineFocus,
      '--color-read-anything-line-focus-low-contrast-light':
          expectedLightLineFocus,
      '--color-read-anything-line-focus-low-contrast-dark':
          expectedDarkLineFocus,
      '--color-text-selection-background': expectedDefaultSelectionBackground,
      '--color-read-anything-current-read-aloud-highlight-yellow':
          expectedYellowCurrentHighlight,
      '--color-read-anything-current-read-aloud-highlight-dark':
          expectedDarkCurrentHighlight,
      '--color-read-anything-current-read-aloud-highlight-high-contrast':
          expectedHighContrastCurrentHighlight,
      '--color-read-anything-current-read-aloud-highlight-low-contrast-light':
          expectedLowContrastLightCurrentHighlight,
      '--color-read-anything-current-read-aloud-highlight-low-contrast-dark':
          expectedLowContrastDarkCurrentHighlight,
      '--color-sys-on-surface-subtle': expectedDefaultPreviousHighlight,
      '--color-read-anything-previous-read-aloud-highlight-yellow':
          expectedYellowPreviousHighlight,
      '--color-read-anything-previous-read-aloud-highlight-dark':
          expectedDarkPreviousHighlight,
      '--color-read-anything-previous-read-aloud-highlight-high-contrast':
          expectedHighContrastPreviousHighlight,
      '--color-read-anything-previous-read-aloud-highlight-low-contrast-light':
          expectedLowContrastLightPreviousHighlight,
      '--color-read-anything-previous-read-aloud-highlight-low-contrast-dark':
          expectedLowContrastDarkPreviousHighlight,
      '--color-side-panel-card-secondary-foreground': expectedDefaultEmptyBody,
      '--google-grey-700': expectedYellowEmptyBody,
      '--google-grey-500': expectedDarkEmptyBody,
      '--color-read-anything-link-default': expectedDefaultLink,
      '--color-read-anything-link-default-yellow': expectedYellowLink,
      '--color-read-anything-link-default-dark': expectedDarkLink,
      '--color-read-anything-link-default-high-contrast':
          expectedHighContrastLink,
      '--color-read-anything-link-default-low-contrast-light':
          expectedLowContrastLightLink,
      '--color-read-anything-link-default-low-contrast-dark':
          expectedLowContrastDarkLink,
      '--color-read-anything-link-visited': expectedDefaultLinkVisited,
      '--color-read-anything-link-visited-yellow': expectedYellowLinkVisited,
      '--color-read-anything-link-visited-dark': expectedDarkLinkVisited,
      '--color-read-anything-link-visited-high-contrast':
          expectedHighContrastLinkVisited,
      '--color-read-anything-link-visited-low-contrast-light':
          expectedLowContrastLightLinkVisited,
      '--color-read-anything-link-visited-low-contrast-dark':
          expectedLowContrastDarkLinkVisited,
      '--line-focus-bg': expectedLightLineFocus,
    });
    audioBrowserProxy.onHighlightGranularityChanged(
        audioBrowserProxy.autoHighlighting);

    // Verify default theme colors.
    visualBrowserProxy.colorTheme = visualBrowserProxy.defaultTheme;
    updater.setTheme();
    assertStringContains(computeStyle('background'), expectedDefaultBackground);
    assertStringContains(computeStyle('color'), expectedDefaultForeground);
    assertEquals(
        expectedDefaultSelectionBackground,
        computeStyle('--current-highlight-bg-color'));
    assertEquals(
        expectedDefaultPreviousHighlight,
        computeStyle('--previous-highlight-color'));
    assertEquals(
        expectedDefaultEmptyHeading,
        computeStyle('--sp-empty-state-heading-color'));
    assertEquals(
        expectedDefaultEmptyBody, computeStyle('--sp-empty-state-body-color'));
    assertEquals(expectedDefaultLink, computeStyle('--link-color'));
    assertEquals(
        expectedDefaultLinkVisited, computeStyle('--visited-link-color'));
    assertEquals(expectedDefaultLineFocus, computeStyle('--line-focus-bg'));

    // Verify yellow theme colors.
    visualBrowserProxy.colorTheme = visualBrowserProxy.yellowTheme;
    updater.setTheme();
    assertStringContains(computeStyle('background'), expectedYellowBackground);
    assertStringContains(computeStyle('color'), expectedYellowForeground);
    assertEquals(
        expectedYellowCurrentHighlight,
        computeStyle('--current-highlight-bg-color'));
    assertEquals(
        expectedYellowPreviousHighlight,
        computeStyle('--previous-highlight-color'));
    assertEquals(
        expectedYellowForeground,
        computeStyle('--sp-empty-state-heading-color'));
    assertEquals(
        expectedYellowEmptyBody, computeStyle('--sp-empty-state-body-color'));
    assertEquals(expectedYellowLink, computeStyle('--link-color'));
    assertEquals(
        expectedYellowLinkVisited, computeStyle('--visited-link-color'));
    assertEquals(expectedLightLineFocus, computeStyle('--line-focus-bg'));

    // Verify light theme colors.
    visualBrowserProxy.colorTheme = visualBrowserProxy.lightTheme;
    updater.setTheme();
    assertEquals(expectedLightLineFocus, computeStyle('--line-focus-bg'));

    // Verify dark theme colors.
    visualBrowserProxy.colorTheme = visualBrowserProxy.darkTheme;
    updater.setTheme();
    assertStringContains(computeStyle('background'), expectedDarkBackground);
    assertStringContains(computeStyle('color'), expectedDarkForeground);
    assertEquals(
        expectedDarkCurrentHighlight,
        computeStyle('--current-highlight-bg-color'));
    assertEquals(
        expectedDarkPreviousHighlight,
        computeStyle('--previous-highlight-color'));
    assertEquals(
        expectedDarkForeground, computeStyle('--sp-empty-state-heading-color'));
    assertEquals(
        expectedDarkEmptyBody, computeStyle('--sp-empty-state-body-color'));
    assertEquals(expectedDarkLink, computeStyle('--link-color'));
    assertEquals(expectedDarkLinkVisited, computeStyle('--visited-link-color'));
    assertEquals(expectedDarkLineFocus, computeStyle('--line-focus-bg'));

    // Verify high contrast theme colors.
    updateStyles({'--google-grey-700': expectedHighContrastEmptyBody});
    visualBrowserProxy.colorTheme = visualBrowserProxy.highContrastTheme;
    updater.setTheme();
    assertStringContains(
        computeStyle('background'), expectedHighContrastBackground);
    assertStringContains(computeStyle('color'), expectedHighContrastForeground);
    assertEquals(
        expectedHighContrastCurrentHighlight,
        computeStyle('--current-highlight-bg-color'));
    assertEquals(
        expectedHighContrastPreviousHighlight,
        computeStyle('--previous-highlight-color'));
    assertEquals(
        expectedHighContrastForeground,
        computeStyle('--sp-empty-state-heading-color'));
    assertEquals(
        expectedHighContrastEmptyBody,
        computeStyle('--sp-empty-state-body-color'));
    assertEquals(expectedHighContrastLink, computeStyle('--link-color'));
    assertEquals(
        expectedHighContrastLinkVisited, computeStyle('--visited-link-color'));
    assertEquals(expectedDarkLineFocus, computeStyle('--line-focus-bg'));


    // Verify lowContrast light theme colors.
    updateStyles({'--google-grey-700': expectedLowContrastLightEmptyBody});
    visualBrowserProxy.colorTheme = visualBrowserProxy.lowContrastLightTheme;
    updater.setTheme();
    assertStringContains(
        computeStyle('background'), expectedLowContrastLightBackground);
    assertStringContains(
        computeStyle('color'), expectedLowContrastLightForeground);
    assertEquals(
        expectedLowContrastLightCurrentHighlight,
        computeStyle('--current-highlight-bg-color'));
    assertEquals(
        expectedLowContrastLightPreviousHighlight,
        computeStyle('--previous-highlight-color'));
    assertEquals(
        expectedLowContrastLightForeground,
        computeStyle('--sp-empty-state-heading-color'));
    assertEquals(
        expectedLowContrastLightEmptyBody,
        computeStyle('--sp-empty-state-body-color'));
    assertEquals(expectedLowContrastLightLink, computeStyle('--link-color'));
    assertEquals(
        expectedLowContrastLightLinkVisited,
        computeStyle('--visited-link-color'));
    assertEquals(expectedLightLineFocus, computeStyle('--line-focus-bg'));

    // Verify lowContrast dark theme colors.
    updateStyles({'--google-grey-700': expectedLowContrastDarkEmptyBody});
    visualBrowserProxy.colorTheme = visualBrowserProxy.lowContrastDarkTheme;
    updater.setTheme();
    assertStringContains(
        computeStyle('background'), expectedLowContrastDarkBackground);
    assertStringContains(
        computeStyle('color'), expectedLowContrastDarkForeground);
    assertEquals(
        expectedLowContrastDarkCurrentHighlight,
        computeStyle('--current-highlight-bg-color'));
    assertEquals(
        expectedLowContrastDarkPreviousHighlight,
        computeStyle('--previous-highlight-color'));
    assertEquals(
        expectedLowContrastDarkForeground,
        computeStyle('--sp-empty-state-heading-color'));
    assertEquals(
        expectedLowContrastDarkEmptyBody,
        computeStyle('--sp-empty-state-body-color'));
    assertEquals(expectedLowContrastDarkLink, computeStyle('--link-color'));
    assertEquals(
        expectedLowContrastDarkLinkVisited,
        computeStyle('--visited-link-color'));
    assertEquals(expectedDarkLineFocus, computeStyle('--line-focus-bg'));
  });

  test('audio player colors change with theme', () => {
    const expectedDefaultBg = 'rgb(1, 1, 1)';
    const expectedDefaultIcon = 'rgb(2, 2, 2)';
    const expectedLightBg = 'rgb(3, 3, 3)';
    const expectedLightIcon = 'rgb(4, 4, 4)';
    const expectedDarkBg = 'rgb(5, 5, 5)';
    const expectedDarkIcon = 'rgb(6, 6, 6)';
    const expectedYellowBg = 'rgb(7, 7, 7)';
    const expectedYellowIcon = 'rgb(8, 8, 8)';
    const expectedBlueBg = 'rgb(9, 9, 9)';
    const expectedBlueIcon = 'rgb(10, 10, 10)';
    const expectedHighContrastBg = 'rgb(11, 11, 11)';
    const expectedHighContrastIcon = 'rgb(12, 12, 12)';
    const expectedLowContrastLightBg = 'rgb(15, 15, 15)';
    const expectedLowContrastLightIcon = 'rgb(16, 16, 16)';
    const expectedLowContrastDarkBg = 'rgb(17, 17, 17)';
    const expectedLowContrastDarkIcon = 'rgb(18, 18, 18)';
    const expectedDefaultControlsIcon = 'rgb(19, 19, 19)';
    const expectedLightControlsIcon = 'rgb(20, 20, 20)';
    const expectedDarkControlsIcon = 'rgb(21, 21, 21)';
    const expectedYellowControlsIcon = 'rgb(22, 22, 22)';
    const expectedBlueControlsIcon = 'rgb(23, 23, 23)';
    const expectedHighContrastControlsIcon = 'rgb(24, 24, 24)';
    const expectedLowContrastLightControlsIcon = 'rgb(26, 26, 26)';
    const expectedLowContrastDarkControlsIcon = 'rgb(27, 27, 27)';
    updateStyles({
      '--color-read-anything-audio-player-background': expectedDefaultBg,
      '--color-read-anything-audio-player-icon': expectedDefaultIcon,
      '--color-read-anything-audio-player-background-light': expectedLightBg,
      '--color-read-anything-audio-player-icon-light': expectedLightIcon,
      '--color-read-anything-audio-player-background-dark': expectedDarkBg,
      '--color-read-anything-audio-player-icon-dark': expectedDarkIcon,
      '--color-read-anything-audio-player-background-yellow': expectedYellowBg,
      '--color-read-anything-audio-player-icon-yellow': expectedYellowIcon,
      '--color-read-anything-audio-player-background-blue': expectedBlueBg,
      '--color-read-anything-audio-player-icon-blue': expectedBlueIcon,
      '--color-read-anything-audio-player-background-high-contrast':
          expectedHighContrastBg,
      '--color-read-anything-audio-player-icon-high-contrast':
          expectedHighContrastIcon,
      '--color-read-anything-audio-player-background-low-contrast-light':
          expectedLowContrastLightBg,
      '--color-read-anything-audio-player-icon-low-contrast-light':
          expectedLowContrastLightIcon,
      '--color-read-anything-audio-player-background-low-contrast-dark':
          expectedLowContrastDarkBg,
      '--color-read-anything-audio-player-icon-low-contrast-dark':
          expectedLowContrastDarkIcon,
      '--color-read-anything-audio-controls-icon': expectedDefaultControlsIcon,
      '--color-read-anything-audio-controls-icon-light':
          expectedLightControlsIcon,
      '--color-read-anything-audio-controls-icon-dark':
          expectedDarkControlsIcon,
      '--color-read-anything-audio-controls-icon-yellow':
          expectedYellowControlsIcon,
      '--color-read-anything-audio-controls-icon-blue':
          expectedBlueControlsIcon,
      '--color-read-anything-audio-controls-icon-high-contrast':
          expectedHighContrastControlsIcon,
      '--color-read-anything-audio-controls-icon-low-contrast-light':
          expectedLowContrastLightControlsIcon,
      '--color-read-anything-audio-controls-icon-low-contrast-dark':
          expectedLowContrastDarkControlsIcon,
    });

    // Default theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.defaultTheme;
    updater.setTheme();
    assertEquals(
        expectedDefaultBg, computeStyle('--audio-player-background-color'));
    assertEquals(
        expectedDefaultIcon, computeStyle('--audio-player-icon-color'));
    assertEquals(
        expectedDefaultControlsIcon,
        computeStyle('--audio-controls-icon-color'));

    // Light theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.lightTheme;
    updater.setTheme();
    assertEquals(
        expectedLightBg, computeStyle('--audio-player-background-color'));
    assertEquals(expectedLightIcon, computeStyle('--audio-player-icon-color'));
    assertEquals(
        expectedLightControlsIcon, computeStyle('--audio-controls-icon-color'));

    // Dark theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.darkTheme;
    updater.setTheme();
    assertEquals(
        expectedDarkBg, computeStyle('--audio-player-background-color'));
    assertEquals(expectedDarkIcon, computeStyle('--audio-player-icon-color'));
    assertEquals(
        expectedDarkControlsIcon, computeStyle('--audio-controls-icon-color'));

    // Yellow theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.yellowTheme;
    updater.setTheme();
    assertEquals(
        expectedYellowBg, computeStyle('--audio-player-background-color'));
    assertEquals(expectedYellowIcon, computeStyle('--audio-player-icon-color'));
    assertEquals(
        expectedYellowControlsIcon,
        computeStyle('--audio-controls-icon-color'));

    // Blue theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.blueTheme;
    updater.setTheme();
    assertEquals(
        expectedBlueBg, computeStyle('--audio-player-background-color'));
    assertEquals(expectedBlueIcon, computeStyle('--audio-player-icon-color'));
    assertEquals(
        expectedBlueControlsIcon, computeStyle('--audio-controls-icon-color'));

    // High contrast theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.highContrastTheme;
    updater.setTheme();
    assertEquals(
        expectedHighContrastBg,
        computeStyle('--audio-player-background-color'));
    assertEquals(
        expectedHighContrastIcon, computeStyle('--audio-player-icon-color'));
    assertEquals(
        expectedHighContrastControlsIcon,
        computeStyle('--audio-controls-icon-color'));


    // LowContrast light theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.lowContrastLightTheme;
    updater.setTheme();
    assertEquals(
        expectedLowContrastLightBg,
        computeStyle('--audio-player-background-color'));
    assertEquals(
        expectedLowContrastLightIcon,
        computeStyle('--audio-player-icon-color'));
    assertEquals(
        expectedLowContrastLightControlsIcon,
        computeStyle('--audio-controls-icon-color'));

    // LowContrast dark theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.lowContrastDarkTheme;
    updater.setTheme();
    assertEquals(
        expectedLowContrastDarkBg,
        computeStyle('--audio-player-background-color'));
    assertEquals(
        expectedLowContrastDarkIcon, computeStyle('--audio-player-icon-color'));
    assertEquals(
        expectedLowContrastDarkControlsIcon,
        computeStyle('--audio-controls-icon-color'));
  });

  test('toolbar icon colors change with theme', () => {
    const expectedDefaultToolbarIcon = 'rgb(1, 1, 1)';
    const expectedLightToolbarIcon = 'rgb(2, 2, 2)';
    const expectedDarkToolbarIcon = 'rgb(3, 3, 3)';
    const expectedYellowToolbarIcon = 'rgb(4, 4, 4)';
    const expectedBlueToolbarIcon = 'rgb(5, 5, 5)';
    const expectedHighContrastToolbarIcon = 'rgb(6, 6, 6)';
    const expectedLowContrastLightToolbarIcon = 'rgb(8, 8, 8)';
    const expectedLowContrastDarkToolbarIcon = 'rgb(9, 9, 9)';
    updateStyles({
      '--color-read-anything-toolbar-icon': expectedDefaultToolbarIcon,
      '--color-read-anything-toolbar-icon-light': expectedLightToolbarIcon,
      '--color-read-anything-toolbar-icon-dark': expectedDarkToolbarIcon,
      '--color-read-anything-toolbar-icon-yellow': expectedYellowToolbarIcon,
      '--color-read-anything-toolbar-icon-blue': expectedBlueToolbarIcon,
      '--color-read-anything-toolbar-icon-high-contrast':
          expectedHighContrastToolbarIcon,
      '--color-read-anything-toolbar-icon-low-contrast-light':
          expectedLowContrastLightToolbarIcon,
      '--color-read-anything-toolbar-icon-low-contrast-dark':
          expectedLowContrastDarkToolbarIcon,
    });

    // Default theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.defaultTheme;
    updater.setTheme();
    assertEquals(
        expectedDefaultToolbarIcon, computeStyle('--toolbar-icon-color'));

    // Light theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.lightTheme;
    updater.setTheme();
    assertEquals(
        expectedLightToolbarIcon, computeStyle('--toolbar-icon-color'));

    // Dark theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.darkTheme;
    updater.setTheme();
    assertEquals(expectedDarkToolbarIcon, computeStyle('--toolbar-icon-color'));

    // Yellow theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.yellowTheme;
    updater.setTheme();
    assertEquals(
        expectedYellowToolbarIcon, computeStyle('--toolbar-icon-color'));

    // Blue theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.blueTheme;
    updater.setTheme();
    assertEquals(expectedBlueToolbarIcon, computeStyle('--toolbar-icon-color'));

    // High contrast theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.highContrastTheme;
    updater.setTheme();
    assertEquals(
        expectedHighContrastToolbarIcon, computeStyle('--toolbar-icon-color'));

    // LowContrast light theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.lowContrastLightTheme;
    updater.setTheme();
    assertEquals(
        expectedLowContrastLightToolbarIcon,
        computeStyle('--toolbar-icon-color'));

    // LowContrast dark theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.lowContrastDarkTheme;
    updater.setTheme();
    assertEquals(
        expectedLowContrastDarkToolbarIcon,
        computeStyle('--toolbar-icon-color'));
  });

  test(
      'setTheme does not update toolbar icon color if line focus is enabled ' +
          'and a visible window',
      () => {
        visualBrowserProxy.lineFocusEnabled = true;
        app.style.setProperty('--line-focus-display', 'block');
        app.style.setProperty('--line-focus-bg', 'none');
        const initialColor = 'rgb(255, 0, 0)';
        app.style.setProperty('--toolbar-icon-color', initialColor);

        visualBrowserProxy.colorTheme = visualBrowserProxy.darkTheme;
        updater.setTheme();

        assertEquals(initialColor, computeStyle('--toolbar-icon-color'));
      });

  test(
      'setTheme updates toolbar icon color if line focus is enabled but ' +
          'display is none',
      () => {
        visualBrowserProxy.lineFocusEnabled = true;
        app.style.setProperty('--line-focus-display', 'none');
        app.style.setProperty('--line-focus-bg', 'none');
        const initialColor = 'rgb(255, 0, 0)';
        app.style.setProperty('--toolbar-icon-color', initialColor);
        const expectedDarkToolbarIcon = 'rgb(3, 3, 3)';
        updateStyles({
          '--color-read-anything-toolbar-icon-dark': expectedDarkToolbarIcon,
        });

        visualBrowserProxy.colorTheme = visualBrowserProxy.darkTheme;
        updater.setTheme();

        assertEquals(
            expectedDarkToolbarIcon, computeStyle('--toolbar-icon-color'));
      });

  test(
      'setTheme updates toolbar icon color if line focus is enabled and a ' +
          'a visible line',
      () => {
        visualBrowserProxy.lineFocusEnabled = true;
        app.style.setProperty('--line-focus-display', 'none');
        app.style.setProperty(
            '--line-focus-bg', 'var(--color-read-anything-line-focus-dark)');
        const initialColor = 'rgb(255, 0, 0)';
        app.style.setProperty('--toolbar-icon-color', initialColor);
        const expectedDarkToolbarIcon = 'rgb(3, 3, 3)';
        updateStyles({
          '--color-read-anything-toolbar-icon-dark': expectedDarkToolbarIcon,
        });

        visualBrowserProxy.colorTheme = visualBrowserProxy.darkTheme;
        updater.setTheme();

        assertEquals(
            expectedDarkToolbarIcon, computeStyle('--toolbar-icon-color'));
      });

  test('on player focus outline colors change with theme', () => {
    const expectedDefault = 'rgb(1, 1, 1)';
    const expectedLight = 'rgb(2, 2, 2)';
    const expectedDark = 'rgb(3, 3, 3)';
    const expectedYellow = 'rgb(4, 4, 4)';
    const expectedBlue = 'rgb(5, 5, 5)';
    const expectedHighContrast = 'rgb(6, 6, 6)';
    const expectedLowContrastLight = 'rgb(8, 8, 8)';
    const expectedLowContrastDark = 'rgb(9, 9, 9)';
    updateStyles({
      '--color-read-anything-on-audio-player-focus-outline': expectedDefault,
      '--color-read-anything-on-audio-player-focus-outline-light':
          expectedLight,
      '--color-read-anything-on-audio-player-focus-outline-dark': expectedDark,
      '--color-read-anything-on-audio-player-focus-outline-yellow':
          expectedYellow,
      '--color-read-anything-on-audio-player-focus-outline-blue': expectedBlue,
      '--color-read-anything-on-audio-player-focus-outline-high-contrast':
          expectedHighContrast,
      '--color-read-anything-on-audio-player-focus-outline-low-contrast-light':
          expectedLowContrastLight,
      '--color-read-anything-on-audio-player-focus-outline-low-contrast-dark':
          expectedLowContrastDark,
    });

    // Default theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.defaultTheme;
    updater.setTheme();
    assertEquals(
        expectedDefault, computeStyle('--on-audio-player-focus-outline-color'));

    // Light theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.lightTheme;
    updater.setTheme();
    assertEquals(
        expectedLight, computeStyle('--on-audio-player-focus-outline-color'));

    // Dark theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.darkTheme;
    updater.setTheme();
    assertEquals(
        expectedDark, computeStyle('--on-audio-player-focus-outline-color'));

    // Yellow theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.yellowTheme;
    updater.setTheme();
    assertEquals(
        expectedYellow, computeStyle('--on-audio-player-focus-outline-color'));

    // Blue theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.blueTheme;
    updater.setTheme();
    assertEquals(
        expectedBlue, computeStyle('--on-audio-player-focus-outline-color'));

    // High contrast theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.highContrastTheme;
    updater.setTheme();
    assertEquals(
        expectedHighContrast,
        computeStyle('--on-audio-player-focus-outline-color'));

    // LowContrast light theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.lowContrastLightTheme;
    updater.setTheme();
    assertEquals(
        expectedLowContrastLight,
        computeStyle('--on-audio-player-focus-outline-color'));

    // LowContrast dark theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.lowContrastDarkTheme;
    updater.setTheme();
    assertEquals(
        expectedLowContrastDark,
        computeStyle('--on-audio-player-focus-outline-color'));
  });

  test('setTheme with line focus window does not update color', () => {
    const lineFocusColor = 'rgb(50, 21, 0)';
    const expectedLineFocusBg = 'none';
    updateStyles({
      '--color-read-anything-line-focus': lineFocusColor,
      '--color-read-anything-line-focus-yellow': lineFocusColor,
      '--color-read-anything-line-focus-dark': lineFocusColor,
      '--color-read-anything-line-focus-light': lineFocusColor,
      '--color-read-anything-line-focus-high-contrast': lineFocusColor,
      '--color-read-anything-line-focus-low-contrast-light': lineFocusColor,
      '--color-read-anything-line-focus-low-contrast-dark': lineFocusColor,
      '--line-focus-bg': expectedLineFocusBg,
    });

    visualBrowserProxy.colorTheme = visualBrowserProxy.lowContrastDarkTheme;
    updater.setTheme();
    assertEquals(expectedLineFocusBg, computeStyle('--line-focus-bg'));

    visualBrowserProxy.colorTheme = visualBrowserProxy.blueTheme;
    updater.setTheme();
    assertEquals(expectedLineFocusBg, computeStyle('--line-focus-bg'));

    visualBrowserProxy.colorTheme = visualBrowserProxy.defaultTheme;
    updater.setTheme();
    assertEquals(expectedLineFocusBg, computeStyle('--line-focus-bg'));
  });

  test('setAllTextStyles updates all text styles', () => {
    setAppFontSize(10);
    visualBrowserProxy.fontSize = 2;
    visualBrowserProxy.lineSpacing = 4;
    visualBrowserProxy.letterSpacing = 3;
    visualBrowserProxy.fontName = 'Andika';
    visualBrowserProxy.colorTheme = visualBrowserProxy.blueTheme;
    audioBrowserProxy.onHighlightGranularityChanged(
        audioBrowserProxy.autoHighlighting);
    const expectedBlueBackground = 'rgb(1, 2, 3)';
    const expectedBlueForeground = 'rgb(4, 5, 6)';
    const expectedBlueCurrentHighlight = 'rgb(7, 8, 9)';
    const expectedBluePreviousHighlight = 'rgb(10, 11, 12)';
    const expectedBlueEmptyBody = 'rgb(4, 5, 6)';
    const expectedBlueLink = 'rgb(16, 17, 18)';
    const expectedBlueLinkVisited = 'rgb(19, 20, 21)';
    const expectedBlueLineFocus = 'rgb(22, 23, 24)';
    updateStyles({
      '--color-read-anything-background-blue': expectedBlueBackground,
      '--color-read-anything-foreground-blue': expectedBlueForeground,
      '--color-read-anything-current-read-aloud-highlight-blue':
          expectedBlueCurrentHighlight,
      '--color-read-anything-previous-read-aloud-highlight-blue':
          expectedBluePreviousHighlight,
      '--google-grey-700': expectedBlueEmptyBody,
      '--color-read-anything-link-default-blue': expectedBlueLink,
      '--color-read-anything-link-visited-blue': expectedBlueLinkVisited,
      '--color-read-anything-line-focus-blue': expectedBlueLineFocus,
    });

    updater.setAllTextStyles();

    assertEquals('20px', computeStyle('font-size'));
    assertEquals('100px', computeStyle('line-height'));
    assertEquals('60px', computeStyle('letter-spacing'));
    assertStringContains(
        computeStyle('font-family'), visualBrowserProxy.fontName);
    assertStringContains(computeStyle('background'), expectedBlueBackground);
    assertStringContains(computeStyle('color'), expectedBlueForeground);
    assertEquals(
        expectedBlueCurrentHighlight,
        computeStyle('--current-highlight-bg-color'));
    assertEquals(
        expectedBluePreviousHighlight,
        computeStyle('--previous-highlight-color'));
    assertEquals(
        expectedBlueForeground, computeStyle('--sp-empty-state-heading-color'));
    assertEquals(
        expectedBlueEmptyBody, computeStyle('--sp-empty-state-body-color'));
    assertEquals(expectedBlueLink, computeStyle('--link-color'));
    assertEquals(expectedBlueLinkVisited, computeStyle('--visited-link-color'));
    assertEquals(expectedBlueLineFocus, computeStyle('--line-focus-bg'));
  });

  test('scrollbar color changes with theme', () => {
    const expectedLightFullPageScrollbar = 'rgb(1, 1, 1)';
    const expectedDarkFullPageScrollbar = 'rgb(2, 2, 2)';
    const expectedYellowFullPageScrollbar = 'rgb(3, 3, 3)';
    const expectedBlueFullPageScrollbar = 'rgb(4, 4, 4)';
    const expectedHighContrastFullPageScrollbar = 'rgb(5, 5, 5)';
    const expectedLowContrastLightFullPageScrollbar = 'rgb(6, 6, 6)';
    const expectedLowContrastDarkFullPageScrollbar = 'rgb(7, 7, 7)';
    updateStyles({
      '--color-read-anything-full-page-scrollbar-light':
          expectedLightFullPageScrollbar,
      '--color-read-anything-full-page-scrollbar-dark':
          expectedDarkFullPageScrollbar,
      '--color-read-anything-full-page-scrollbar-yellow':
          expectedYellowFullPageScrollbar,
      '--color-read-anything-full-page-scrollbar-blue':
          expectedBlueFullPageScrollbar,
      '--color-read-anything-full-page-scrollbar-high-contrast':
          expectedHighContrastFullPageScrollbar,
      '--color-read-anything-full-page-scrollbar-low-contrast-light':
          expectedLowContrastLightFullPageScrollbar,
      '--color-read-anything-full-page-scrollbar-low-contrast-dark':
          expectedLowContrastDarkFullPageScrollbar,
    });

    // Light theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.lightTheme;
    updater.setTheme();
    assertEquals(
        expectedLightFullPageScrollbar,
        computeStyle('--color-read-anything-full-page-scrollbar'));

    // Dark theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.darkTheme;
    updater.setTheme();
    assertEquals(
        expectedDarkFullPageScrollbar,
        computeStyle('--color-read-anything-full-page-scrollbar'));

    // Yellow theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.yellowTheme;
    updater.setTheme();
    assertEquals(
        expectedYellowFullPageScrollbar,
        computeStyle('--color-read-anything-full-page-scrollbar'));

    // Blue theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.blueTheme;
    updater.setTheme();
    assertEquals(
        expectedBlueFullPageScrollbar,
        computeStyle('--color-read-anything-full-page-scrollbar'));

    // High contrast theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.highContrastTheme;
    updater.setTheme();
    assertEquals(
        expectedHighContrastFullPageScrollbar,
        computeStyle('--color-read-anything-full-page-scrollbar'));

    // LowContrast light theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.lowContrastLightTheme;
    updater.setTheme();
    assertEquals(
        expectedLowContrastLightFullPageScrollbar,
        computeStyle('--color-read-anything-full-page-scrollbar'));

    // LowContrast dark theme
    visualBrowserProxy.colorTheme = visualBrowserProxy.lowContrastDarkTheme;
    updater.setTheme();
    assertEquals(
        expectedLowContrastDarkFullPageScrollbar,
        computeStyle('--color-read-anything-full-page-scrollbar'));
  });
});
