// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {AppStyleUpdater, BrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertNotEquals, assertStringContains} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors} from './common.js';
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

  setup(() => {
    suppressInnocuousErrors();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;

    app = document.createElement('read-anything-app');
    document.body.appendChild(app);
    updater = new AppStyleUpdater(app);
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
    const expectedDefaultForeground = 'rgb(255, 0, 0)';
    const expectedYellowForeground = 'rgb(255, 0, 255)';
    const expectedDarkForeground = 'rgb(255, 255, 0)';
    const expectedDefaultSelectionBackground = 'rgb(255, 255, 255)';
    const expectedYellowCurrentHighlight = 'rgb(0, 0, 0)';
    const expectedDarkCurrentHighlight = 'rgb(5, 5, 100)';
    const expectedDefaultPreviousHighlight = 'rgb(5, 100, 5)';
    const expectedYellowPreviousHighlight = 'rgb(5, 100, 100)';
    const expectedDarkPreviousHighlight = 'rgb(100, 5, 5)';
    const expectedDefaultEmptyHeading = 'rgb(100, 5, 100)';
    const expectedDefaultEmptyBody = 'rgb(100, 100, 100)';
    const expectedYellowEmptyBody = 'rgb(6, 6, 37)';
    const expectedDarkEmptyBody = 'rgb(6, 37, 6)';
    const expectedDefaultLink = 'rgb(6, 37, 37)';
    const expectedYellowLink = 'rgb(37, 6, 6)';
    const expectedDarkLink = 'rgb(37, 6, 37)';
    const expectedDefaultLinkVisited = 'rgb(37, 37, 6)';
    const expectedYellowLinkVisited = 'rgb(37, 37, 37)';
    const expectedDarkLinkVisited = 'rgb(14, 14, 28)';
    updateStyles({
      '--color-sys-base-container-elevated': expectedDefaultBackground,
      '--color-read-anything-background-yellow': expectedYellowBackground,
      '--color-read-anything-background-dark': expectedDarkBackground,
      '--color-sys-on-surface': expectedDefaultForeground,
      '--color-read-anything-foreground': expectedDefaultEmptyHeading,
      '--color-read-anything-foreground-yellow': expectedYellowForeground,
      '--color-read-anything-foreground-dark': expectedDarkForeground,
      '--color-text-selection-background': expectedDefaultSelectionBackground,
      '--color-read-anything-current-read-aloud-highlight-yellow':
          expectedYellowCurrentHighlight,
      '--color-read-anything-current-read-aloud-highlight-dark':
          expectedDarkCurrentHighlight,
      '--color-sys-on-surface-subtle': expectedDefaultPreviousHighlight,
      '--color-read-anything-previous-read-aloud-highlight-yellow':
          expectedYellowPreviousHighlight,
      '--color-read-anything-previous-read-aloud-highlight-dark':
          expectedDarkPreviousHighlight,
      '--color-side-panel-card-secondary-foreground': expectedDefaultEmptyBody,
      '--google-grey-700': expectedYellowEmptyBody,
      '--google-grey-500': expectedDarkEmptyBody,
      '--color-read-anything-link-default': expectedDefaultLink,
      '--color-read-anything-link-default-yellow': expectedYellowLink,
      '--color-read-anything-link-default-dark': expectedDarkLink,
      '--color-read-anything-link-visited': expectedDefaultLinkVisited,
      '--color-read-anything-link-visited-yellow': expectedYellowLinkVisited,
      '--color-read-anything-link-visited-dark': expectedDarkLinkVisited,
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
    const expectedBlueEmptyBody = 'rgb(13, 14, 15)';
    const expectedBlueLink = 'rgb(16, 17, 18)';
    const expectedBlueLinkVisited = 'rgb(19, 20, 21)';
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
  });
});
