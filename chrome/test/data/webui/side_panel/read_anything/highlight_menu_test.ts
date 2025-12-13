// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {ReadAloudSettingsChange, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {HighlightMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, mockMetrics} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('HighlightMenuElement', () => {
  let highlightMenu: HighlightMenuElement;
  let metrics: TestMetricsBrowserProxy;

  function createHighlightMenu() {
    highlightMenu = document.createElement('highlight-menu');
    document.body.appendChild(highlightMenu);
  }

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    metrics = mockMetrics();
  });

  test('has checkmarks', () => {
    createHighlightMenu();
    assertCheckMarksForDropdown(highlightMenu);
  });

  test('highlight change is propagated', async () => {
    createHighlightMenu();

    const highlight1 = chrome.readingMode.noHighlighting;
    highlightMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.HIGHLIGHT_CHANGE, {detail: {data: highlight1}}));
    assertEquals(highlight1, chrome.readingMode.highlightGranularity);

    const highlight2 = chrome.readingMode.autoHighlighting;
    highlightMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.HIGHLIGHT_CHANGE, {detail: {data: highlight2}}));
    assertEquals(highlight2, chrome.readingMode.highlightGranularity);

    const highlight3 = chrome.readingMode.sentenceHighlighting;
    highlightMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.HIGHLIGHT_CHANGE, {detail: {data: highlight3}}));
    assertEquals(highlight3, chrome.readingMode.highlightGranularity);

    assertEquals(
        ReadAloudSettingsChange.HIGHLIGHT_CHANGE,
        await metrics.whenCalled('recordSpeechSettingsChange'));
    assertEquals(3, metrics.getCallCount('recordSpeechSettingsChange'));
  });

  test('highlight change logs new granularity', async () => {
    createHighlightMenu();

    const highlight = chrome.readingMode.noHighlighting;
    highlightMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.HIGHLIGHT_CHANGE, {detail: {data: highlight}}));

    assertEquals(
        highlight, await metrics.whenCalled('recordHighlightGranularity'));
  });

  test('has phrase highlighting option if flag enabled', () => {
    chrome.readingMode.isPhraseHighlightingEnabled = true;

    createHighlightMenu();

    const menu = highlightMenu.$.menu.$.lazyMenu.get();
    const options =
        Array.from(menu.querySelectorAll<HTMLButtonElement>('.dropdown-item'));
    const titles = options.map(button => button.textContent?.trim());
    assertEquals(5, titles.length);
    assertTrue(titles.includes('Phrase'));
  });

  test('does not have phrase highlighting option if flag disabled', () => {
    chrome.readingMode.isPhraseHighlightingEnabled = false;

    createHighlightMenu();

    const menu = highlightMenu.$.menu.$.lazyMenu.get();
    const options =
        Array.from(menu.querySelectorAll<HTMLButtonElement>('.dropdown-item'));
    const titles = options.map(button => button.textContent?.trim());
    assertEquals(4, titles.length);
    assertFalse(titles.includes('Phrase'));
  });

  test('restores saved highlight option', async () => {
    createHighlightMenu();
    const granularity = chrome.readingMode.wordHighlighting;
    const startingIndex = highlightMenu.$.menu.currentSelectedIndex;
    assertNotEquals(granularity, startingIndex);

    highlightMenu.settingsPrefs = {
      letterSpacing: 0,
      lineSpacing: 0,
      theme: 0,
      speechRate: 0,
      font: '',
      highlightGranularity: granularity,
    };
    await microtasksFinished();

    assertNotEquals(startingIndex, highlightMenu.$.menu.currentSelectedIndex);
  });

  test('does nothing if saved highlight is the same', async () => {
    createHighlightMenu();
    const startingIndex = highlightMenu.$.menu.currentSelectedIndex;

    highlightMenu.settingsPrefs = {
      letterSpacing: 100,
      lineSpacing: 101,
      theme: 102,
      speechRate: 103,
      font: 'font',
      highlightGranularity: 0,
    };
    await microtasksFinished();

    assertEquals(startingIndex, highlightMenu.$.menu.currentSelectedIndex);
  });
});
