// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {DEFAULT_SETTINGS, ReadAnythingSettingsChange, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {LetterSpacingMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, assertTestSettingsAreNotDefaultSettings, mockMetrics, stubAnimationFrame, TEST_RANDOM_VALUE_SETTINGS} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('LetterSpacing', () => {
  let letterSpacingMenu: LetterSpacingMenuElement;
  let metrics: TestMetricsBrowserProxy;

  suiteSetup(() => {
    assertTestSettingsAreNotDefaultSettings();
  });

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
    const numberOfSpacings = 3;

    const veryWide = chrome.readingMode.veryWideLetterSpacing;
    const closePromise1 =
        eventToPromise(ToolbarEvent.CLOSE_ALL_MENUS, document);
    letterSpacingMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.LETTER_SPACING, {detail: {data: veryWide}}));
    await closePromise1;
    assertEquals(veryWide, chrome.readingMode.letterSpacing);

    const wide = chrome.readingMode.wideLetterSpacing;
    const closePromise2 =
        eventToPromise(ToolbarEvent.CLOSE_ALL_MENUS, document);
    letterSpacingMenu.$.menu.dispatchEvent(
        new CustomEvent(ToolbarEvent.LETTER_SPACING, {detail: {data: wide}}));
    await closePromise2;
    assertEquals(wide, chrome.readingMode.letterSpacing);

    const standard = chrome.readingMode.standardLetterSpacing;
    const closePromise3 =
        eventToPromise(ToolbarEvent.CLOSE_ALL_MENUS, document);
    letterSpacingMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.LETTER_SPACING, {detail: {data: standard}}));
    await closePromise3;
    assertEquals(standard, chrome.readingMode.letterSpacing);

    assertEquals(
        ReadAnythingSettingsChange.LETTER_SPACING_CHANGE,
        await metrics.whenCalled('recordTextSettingsChange'));
    assertEquals(
        numberOfSpacings, metrics.getCallCount('recordTextSettingsChange'));
  });

  test('restores saved spacing option', async () => {
    const spacing = chrome.readingMode.veryWideLetterSpacing;
    const startingIndex = letterSpacingMenu.$.menu.currentSelectedIndex;
    assertNotEquals(spacing, startingIndex);

    letterSpacingMenu.settingsPrefs = {
      ...DEFAULT_SETTINGS,
      letterSpacing: spacing,
    };
    await microtasksFinished();

    assertNotEquals(
        startingIndex, letterSpacingMenu.$.menu.currentSelectedIndex);
  });

  test('does nothing if saved spacing is the same', async () => {
    const startingIndex = letterSpacingMenu.$.menu.currentSelectedIndex;

    letterSpacingMenu.settingsPrefs = {
      ...TEST_RANDOM_VALUE_SETTINGS,
      letterSpacing: 0,
    };
    await microtasksFinished();

    assertEquals(startingIndex, letterSpacingMenu.$.menu.currentSelectedIndex);
  });

  test('can be closed programatically', () => {
    stubAnimationFrame();
    letterSpacingMenu.open(document.body);
    assertTrue(letterSpacingMenu.$.menu.$.lazyMenu.get().open);
    letterSpacingMenu.close();
    assertFalse(letterSpacingMenu.$.menu.$.lazyMenu.get().open);
  });
});
