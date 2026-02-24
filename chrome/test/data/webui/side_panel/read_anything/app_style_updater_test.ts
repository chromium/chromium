// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {AppStyleUpdater, BrowserProxy, LineFocusType} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertNotEquals, assertStringContains} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {createApp} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('AppStyleUpdater', () => {
  let app: AppElement;
  let updater: AppStyleUpdater;

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
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;

    app = await createApp();
    updater = new AppStyleUpdater(app);
  });

  test('max line width is max chars', () => {
    chrome.readingMode.maxLineWidth = 100;
    updater.setMaxLineWidth();
    assertEquals('100ch', app.style.getPropertyValue('--max-width'));

    chrome.readingMode.maxLineWidth = 40;
    updater.setMaxLineWidth();
    assertEquals('40ch', app.style.getPropertyValue('--max-width'));
  });

  test('setPaddingForLineFocus sets top and bottom padding', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    const padding = 50;

    updater.setPaddingForLineFocus(padding);

    assertEquals(`${padding}px`, computeStyle('padding-top'));
    assertEquals(`${padding}px`, computeStyle('padding-bottom'));
    assertEquals(padding, updater.getPaddingForLineFocus());
  });

  test('line focus height depends on font scale', () => {
    chrome.readingMode.fontSize = 1;
    updater.setLineFocusHeight();
    assertEquals('2px', app.style.getPropertyValue('--line-focus-height'));

    chrome.readingMode.fontSize = 2;
    updater.setLineFocusHeight();
    assertEquals('4px', app.style.getPropertyValue('--line-focus-height'));
  });

  test('setLineFocusStyle with no line focus hides view', () => {
    chrome.readingMode.isLineFocusEnabled = true;

    updater.setLineFocusStyle();

    assertEquals('none', app.style.getPropertyValue('--line-focus-display'));
    assertEquals('', app.style.getPropertyValue('--line-focus-shadow'));
    assertEquals('', app.style.getPropertyValue('--line-focus-bg'));
    assertEquals('', app.style.getPropertyValue('--line-focus-height'));
  });

  test('setLineFocusStyle with line focus off hides view', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    chrome.readingMode.colorTheme = chrome.readingMode.lowContrastDarkTheme;

    updater.setLineFocusStyle(LineFocusType.NONE);

    assertEquals('none', app.style.getPropertyValue('--line-focus-display'));
    assertEquals('', app.style.getPropertyValue('--line-focus-shadow'));
    assertEquals('', app.style.getPropertyValue('--line-focus-bg'));
    assertEquals('', app.style.getPropertyValue('--line-focus-height'));
  });

  test('setLineFocusStyle with line focus line shows view', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    chrome.readingMode.colorTheme = chrome.readingMode.lowContrastDarkTheme;

    updater.setLineFocusStyle(LineFocusType.LINE);

    assertNotEquals('none', app.style.getPropertyValue('--line-focus-display'));
    assertNotEquals('', app.style.getPropertyValue('--line-focus-shadow'));
    assertEquals(
        'var(--color-read-anything-line-focus-low-contrast-dark)',
        app.style.getPropertyValue('--line-focus-bg'));
    assertNotEquals('', app.style.getPropertyValue('--line-focus-height'));
  });

  test('setLineFocusStyle with line focus window shows view', () => {
    chrome.readingMode.isLineFocusEnabled = true;

    updater.setLineFocusStyle(LineFocusType.WINDOW);

    assertNotEquals('none', app.style.getPropertyValue('--line-focus-display'));
    assertNotEquals('', app.style.getPropertyValue('--line-focus-shadow'));
    assertNotEquals('', app.style.getPropertyValue('--line-focus-bg'));
  });

  test('setLineFocusStyle with line focus window does not set height', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    updater.setLineFocusStyle(LineFocusType.WINDOW);
    assertEquals('', app.style.getPropertyValue('--line-focus-height'));
  });

  test(
      'setLineFocusStyle sets different background and shadow for different types',
      () => {
        chrome.readingMode.isLineFocusEnabled = true;
        updater.setLineFocusStyle(LineFocusType.WINDOW);
        const windowShadow = app.style.getPropertyValue('--line-focus-shadow');
        const windowBg = app.style.getPropertyValue('--line-focus-bg');

        updater.setLineFocusStyle(LineFocusType.LINE);
        const lineShadow = app.style.getPropertyValue('--line-focus-shadow');
        const lineBg = app.style.getPropertyValue('--line-focus-bg');

        assertNotEquals(windowShadow, lineShadow);
        assertNotEquals(windowBg, lineBg);
      });

  test('setLineFocusPos sets y position', () => {
    const pos = 123;

    updater.setLineFocusPos(pos, null, app.$.containerParent);

    assertEquals(`${pos}px`, app.style.getPropertyValue('--line-focus-y'));
    assertEquals(
        `-${pos}px`, app.style.getPropertyValue('--line-focus-clip-top'));
    assertEquals('', app.style.getPropertyValue('--line-focus-height'));
    assertEquals('', app.style.getPropertyValue('--line-focus-clip-bottom'));
  });

  test('setLineFocusPos offsets top', async () => {
    const pos = 123;
    // Ensure there's content so there is an offset.
    app.updateContent();
    await microtasksFinished();

    updater.setLineFocusPos(pos, null, app.$.containerParent);

    assertEquals(`${pos}px`, app.style.getPropertyValue('--line-focus-y'));
    assertEquals(
        `-${pos - app.$.containerParent.offsetTop}px`,
        app.style.getPropertyValue('--line-focus-clip-top'));
    assertEquals('', app.style.getPropertyValue('--line-focus-height'));
    assertEquals('', app.style.getPropertyValue('--line-focus-clip-bottom'));
  });

  test('setLineFocusPos sets height', () => {
    const height = 456;

    updater.setLineFocusPos(0, height, app.$.containerParent);

    assertEquals(
        `${height}px`, app.style.getPropertyValue('--line-focus-height'));
    assertEquals(
        `${height}px`, app.style.getPropertyValue('--line-focus-clip-bottom'));
  });

  test('setLineFocusPos offsets bottom when height given', async () => {
    const pos = 123;
    // Ensure there's content so there is an offset.
    app.updateContent();
    await microtasksFinished();
    const containerHeight = app.$.containerParent.offsetHeight;
    const containerTop = app.$.containerParent.offsetTop;
    const windowHeight = containerHeight / 10;
    console.error('height', app.$.containerParent.offsetHeight);

    updater.setLineFocusPos(pos, windowHeight, app.$.containerParent);

    assertEquals(
        `${windowHeight}px`, app.style.getPropertyValue('--line-focus-height'));
    assertEquals(
        `-${containerHeight - pos - windowHeight + containerTop}px`,
        app.style.getPropertyValue('--line-focus-clip-bottom'));
  });

  test('line spacing depends on font size', () => {
    setAppFontSize(10);
    chrome.readingMode.lineSpacing = 10;
    updater.setLineSpacing();
    assertEquals('100px', computeStyle('line-height'));

    setAppFontSize(12);
    chrome.readingMode.lineSpacing = 16;
    updater.setLineSpacing();
    assertEquals('192px', computeStyle('line-height'));
  });

  test('letter spacing depends on font size', () => {
    setAppFontSize(10);
    chrome.readingMode.letterSpacing = 10;
    updater.setLetterSpacing();
    assertEquals('100px', computeStyle('letter-spacing'));

    setAppFontSize(12);
    chrome.readingMode.letterSpacing = 16;
    updater.setLetterSpacing();
    assertEquals('192px', computeStyle('letter-spacing'));
  });

  test('font size scales', () => {
    setAppFontSize(10);
    chrome.readingMode.fontSize = 1;
    updater.setFontSize();
    assertEquals('10px', computeStyle('font-size'));

    chrome.readingMode.fontSize = 2.5;
    updater.setFontSize();
    assertEquals('25px', computeStyle('font-size'));

    chrome.readingMode.fontSize = 0.5;
    updater.setFontSize();
    assertEquals('5px', computeStyle('font-size'));
  });

  test('font name', () => {
    chrome.readingMode.fontName = 'Poppins';
    updater.setFont();
    assertStringContains(
        computeStyle('font-family'), chrome.readingMode.fontName);

    chrome.readingMode.fontName = 'Lexend Deca';
    updater.setFont();
    assertStringContains(
        computeStyle('font-family'), chrome.readingMode.fontName);
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
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.autoHighlighting);
    chrome.readingMode.colorTheme = chrome.readingMode.yellowTheme;
    updater.setHighlight();
    assertEquals(
        expectedYellowColor, computeStyle('--current-highlight-bg-color'));

    chrome.readingMode.colorTheme = chrome.readingMode.darkTheme;
    updater.setHighlight();
    assertEquals(
        expectedDarkColor, computeStyle('--current-highlight-bg-color'));

    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.noHighlighting);
    chrome.readingMode.colorTheme = chrome.readingMode.lightTheme;
    updater.setHighlight();
    assertEquals('transparent', computeStyle('--current-highlight-bg-color'));
  });

  test('overflow toolbar changes style based on input', () => {
    updater.overflowToolbar(true);
    const scrollOverflow = computeStyle('--app-overflow-x');
    const scrollMinWidth = computeStyle('--container-min-width');

    updater.overflowToolbar(false);
    const noScrollOverflow = computeStyle('--app-overflow-x');
    const noScrollMinWidth = computeStyle('--container-min-width');

    assertNotEquals(scrollOverflow, noScrollOverflow);
    assertNotEquals(scrollMinWidth, noScrollMinWidth);
  });

  test('overflow toolbar without scrolling is same as resetting', () => {
    updater.overflowToolbar(false);
    const noScrollOverflow = computeStyle('--app-overflow-x');
    const noScrollMinWidth = computeStyle('--container-min-width');

    updater.resetToolbar();
    const resetOverflow = computeStyle('--app-overflow-x');
    const resetMinWidth = computeStyle('--container-min-width');

    assertEquals(resetOverflow, noScrollOverflow);
    assertEquals(resetMinWidth, noScrollMinWidth);
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
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.autoHighlighting);

    // Verify default theme colors.
    chrome.readingMode.colorTheme = chrome.readingMode.defaultTheme;
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
    chrome.readingMode.colorTheme = chrome.readingMode.yellowTheme;
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
    chrome.readingMode.colorTheme = chrome.readingMode.lightTheme;
    updater.setTheme();
    assertEquals(expectedLightLineFocus, computeStyle('--line-focus-bg'));

    // Verify dark theme colors.
    chrome.readingMode.colorTheme = chrome.readingMode.darkTheme;
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
    chrome.readingMode.colorTheme = chrome.readingMode.highContrastTheme;
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
    chrome.readingMode.colorTheme = chrome.readingMode.lowContrastLightTheme;
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
    chrome.readingMode.colorTheme = chrome.readingMode.lowContrastDarkTheme;
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
    chrome.readingMode.colorTheme = chrome.readingMode.defaultTheme;
    updater.setTheme();
    assertEquals(
        expectedDefaultBg, computeStyle('--audio-player-background-color'));
    assertEquals(
        expectedDefaultIcon, computeStyle('--audio-player-icon-color'));
    assertEquals(
        expectedDefaultControlsIcon,
        computeStyle('--audio-controls-icon-color'));

    // Light theme
    chrome.readingMode.colorTheme = chrome.readingMode.lightTheme;
    updater.setTheme();
    assertEquals(
        expectedLightBg, computeStyle('--audio-player-background-color'));
    assertEquals(expectedLightIcon, computeStyle('--audio-player-icon-color'));
    assertEquals(
        expectedLightControlsIcon, computeStyle('--audio-controls-icon-color'));

    // Dark theme
    chrome.readingMode.colorTheme = chrome.readingMode.darkTheme;
    updater.setTheme();
    assertEquals(
        expectedDarkBg, computeStyle('--audio-player-background-color'));
    assertEquals(expectedDarkIcon, computeStyle('--audio-player-icon-color'));
    assertEquals(
        expectedDarkControlsIcon, computeStyle('--audio-controls-icon-color'));

    // Yellow theme
    chrome.readingMode.colorTheme = chrome.readingMode.yellowTheme;
    updater.setTheme();
    assertEquals(
        expectedYellowBg, computeStyle('--audio-player-background-color'));
    assertEquals(expectedYellowIcon, computeStyle('--audio-player-icon-color'));
    assertEquals(
        expectedYellowControlsIcon,
        computeStyle('--audio-controls-icon-color'));

    // Blue theme
    chrome.readingMode.colorTheme = chrome.readingMode.blueTheme;
    updater.setTheme();
    assertEquals(
        expectedBlueBg, computeStyle('--audio-player-background-color'));
    assertEquals(expectedBlueIcon, computeStyle('--audio-player-icon-color'));
    assertEquals(
        expectedBlueControlsIcon, computeStyle('--audio-controls-icon-color'));

    // High contrast theme
    chrome.readingMode.colorTheme = chrome.readingMode.highContrastTheme;
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
    chrome.readingMode.colorTheme = chrome.readingMode.lowContrastLightTheme;
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
    chrome.readingMode.colorTheme = chrome.readingMode.lowContrastDarkTheme;
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
    chrome.readingMode.colorTheme = chrome.readingMode.defaultTheme;
    updater.setTheme();
    assertEquals(
        expectedDefaultToolbarIcon, computeStyle('--toolbar-icon-color'));

    // Light theme
    chrome.readingMode.colorTheme = chrome.readingMode.lightTheme;
    updater.setTheme();
    assertEquals(
        expectedLightToolbarIcon, computeStyle('--toolbar-icon-color'));

    // Dark theme
    chrome.readingMode.colorTheme = chrome.readingMode.darkTheme;
    updater.setTheme();
    assertEquals(expectedDarkToolbarIcon, computeStyle('--toolbar-icon-color'));

    // Yellow theme
    chrome.readingMode.colorTheme = chrome.readingMode.yellowTheme;
    updater.setTheme();
    assertEquals(
        expectedYellowToolbarIcon, computeStyle('--toolbar-icon-color'));

    // Blue theme
    chrome.readingMode.colorTheme = chrome.readingMode.blueTheme;
    updater.setTheme();
    assertEquals(expectedBlueToolbarIcon, computeStyle('--toolbar-icon-color'));

    // High contrast theme
    chrome.readingMode.colorTheme = chrome.readingMode.highContrastTheme;
    updater.setTheme();
    assertEquals(
        expectedHighContrastToolbarIcon, computeStyle('--toolbar-icon-color'));

    // LowContrast light theme
    chrome.readingMode.colorTheme = chrome.readingMode.lowContrastLightTheme;
    updater.setTheme();
    assertEquals(
        expectedLowContrastLightToolbarIcon,
        computeStyle('--toolbar-icon-color'));

    // LowContrast dark theme
    chrome.readingMode.colorTheme = chrome.readingMode.lowContrastDarkTheme;
    updater.setTheme();
    assertEquals(
        expectedLowContrastDarkToolbarIcon,
        computeStyle('--toolbar-icon-color'));
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

    chrome.readingMode.colorTheme = chrome.readingMode.lowContrastDarkTheme;
    updater.setTheme();
    assertEquals(expectedLineFocusBg, computeStyle('--line-focus-bg'));

    chrome.readingMode.colorTheme = chrome.readingMode.blueTheme;
    updater.setTheme();
    assertEquals(expectedLineFocusBg, computeStyle('--line-focus-bg'));

    chrome.readingMode.colorTheme = chrome.readingMode.defaultTheme;
    updater.setTheme();
    assertEquals(expectedLineFocusBg, computeStyle('--line-focus-bg'));
  });

  test('setAllTextStyles updates all text styles', () => {
    setAppFontSize(10);
    chrome.readingMode.fontSize = 2;
    chrome.readingMode.lineSpacing = 4;
    chrome.readingMode.letterSpacing = 3;
    chrome.readingMode.fontName = 'Andika';
    chrome.readingMode.colorTheme = chrome.readingMode.blueTheme;
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.autoHighlighting);
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
    assertEquals('80px', computeStyle('line-height'));
    assertEquals('60px', computeStyle('letter-spacing'));
    assertStringContains(
        computeStyle('font-family'), chrome.readingMode.fontName);
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
});
