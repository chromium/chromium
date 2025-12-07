// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {ReadAnythingSettingsChange, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {LetterSpacingMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertNotEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, mockMetrics} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('LetterSpacing', () => {
  let letterSpacingMenu: LetterSpacingMenuElement;
  let metrics: TestMetricsBrowserProxy;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    metrics = mockMetrics();

    letterSpacingMenu = document.createElement('letter-spacing-menu');
    document.body.appendChild(letterSpacingMenu);
  });

  test('has checkmarks', () => {
    assertCheckMarksForDropdown(letterSpacingMenu);
  });

  test('spacing change', async () => {
    const veryWide = chrome.readingMode.veryWideLetterSpacing;
    letterSpacingMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.LETTER_SPACING, {detail: {data: veryWide}}));
    assertEquals(veryWide, chrome.readingMode.letterSpacing);

    const wide = chrome.readingMode.wideLetterSpacing;
    letterSpacingMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.LETTER_SPACING, {detail: {data: wide}}));
    assertEquals(wide, chrome.readingMode.letterSpacing);

    const standard = chrome.readingMode.standardLetterSpacing;
    letterSpacingMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.LETTER_SPACING, {detail: {data: standard}}));
    assertEquals(standard, chrome.readingMode.letterSpacing);

    assertEquals(
        ReadAnythingSettingsChange.LETTER_SPACING_CHANGE,
        await metrics.whenCalled('recordTextSettingsChange'));
    assertEquals(3, metrics.getCallCount('recordTextSettingsChange'));
  });

  test('restores saved spacing option', async () => {
    const spacing = chrome.readingMode.veryWideLetterSpacing;
    const startingIndex = letterSpacingMenu.$.menu.currentSelectedIndex;
    assertNotEquals(spacing, startingIndex);

    letterSpacingMenu.settingsPrefs = {
      letterSpacing: spacing,
      lineSpacing: 0,
      theme: 0,
      speechRate: 0,
      font: '',
      highlightGranularity: 0,
    };
    await microtasksFinished();

    assertNotEquals(
        startingIndex, letterSpacingMenu.$.menu.currentSelectedIndex);
  });

  test('does nothing if saved spacing is the same', async () => {
    const startingIndex = letterSpacingMenu.$.menu.currentSelectedIndex;

    letterSpacingMenu.settingsPrefs = {
      letterSpacing: 0,
      lineSpacing: 101,
      theme: 102,
      speechRate: 103,
      font: 'font',
      highlightGranularity: 103,
    };
    await microtasksFinished();

    assertEquals(startingIndex, letterSpacingMenu.$.menu.currentSelectedIndex);
  });
});
