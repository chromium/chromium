// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AudioMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {DEFAULT_SETTINGS, ReadAloudSettingsChange, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {assertCheckMarksForDropdown, assertTestSettingsAreNotDefaultSettings, setupTestEnvironment, stubAnimationFrame, TEST_RANDOM_VALUE_SETTINGS} from './common.js';
import type {TestAudioBrowserProxy} from './test_audio_browser_proxy.js';
import type {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('AudioMenuElement', () => {
  let audioMenu: AudioMenuElement;
  let metrics: TestMetricsBrowserProxy;
  let audioBrowserProxy: TestAudioBrowserProxy;

  function createAudioMenu() {
    audioMenu = document.createElement('audio-menu');
    document.body.appendChild(audioMenu);
  }

  suiteSetup(() => {
    assertTestSettingsAreNotDefaultSettings();
  });

  setup(() => {
    const result = setupTestEnvironment();
    metrics = result.metrics;
    audioBrowserProxy = result.audioBrowserProxy;
  });

  test('has checkmarks', () => {
    createAudioMenu();
    assertCheckMarksForDropdown(audioMenu);
  });

  test('highlight granularity prop update changes selected item', async () => {
    createAudioMenu();
    const wordHighlight = audioBrowserProxy.getWordHighlighting();
    audioMenu.settingsPrefs = {
      ...audioMenu.settingsPrefs,
      highlightGranularity: wordHighlight,
    };
    await microtasksFinished();

    const selectedItems =
        audioMenu.$.menu.menuGroups[0]!.items.filter(item => item.selected);
    assertEquals(1, selectedItems.length);
    assertEquals(wordHighlight, selectedItems[0]!.data);
  });

  test('on highlight change', async () => {
    createAudioMenu();
    let closeAllMenusCount = 0;
    document.addEventListener(
        ToolbarEvent.CLOSE_ALL_MENUS, () => closeAllMenusCount += 1);

    const highlightsToTest = [
      audioBrowserProxy.getAutoHighlighting(),
      audioBrowserProxy.getWordHighlighting(),
      audioBrowserProxy.getSentenceHighlighting(),
      audioBrowserProxy.getNoHighlighting(),
    ];

    for (const testHighlight of highlightsToTest) {
      audioBrowserProxy.resetResolver('onHighlightGranularityChanged');
      audioMenu.$.menu.dispatchEvent(new CustomEvent(
          ToolbarEvent.HIGHLIGHT_CHANGE, {detail: {data: testHighlight}}));
      await microtasksFinished();

      assertEquals(
          testHighlight,
          await audioBrowserProxy.whenCalled('onHighlightGranularityChanged'));
      const selectedItems =
          audioMenu.$.menu.menuGroups[0]!.items.filter(item => item.selected);
      assertEquals(1, selectedItems.length);
      assertEquals(testHighlight, selectedItems[0]!.data);
    }

    assertEquals(
        ReadAloudSettingsChange.HIGHLIGHT_CHANGE,
        await metrics.whenCalled('recordSpeechSettingsChange'));
    assertEquals(
        highlightsToTest.length,
        metrics.getCallCount('recordSpeechSettingsChange'));
    // Close onClick is false in Improved UI submenus.
    assertEquals(0, closeAllMenusCount);
  });

  test('highlight change logs new granularity', async () => {
    createAudioMenu();
    const highlight = audioBrowserProxy.getSentenceHighlighting();
    audioMenu.$.menu.dispatchEvent(new CustomEvent(
        ToolbarEvent.HIGHLIGHT_CHANGE, {detail: {data: highlight}}));
    await microtasksFinished();

    assertEquals(
        highlight, await metrics.whenCalled('recordHighlightGranularity'));
  });

  test('has phrase highlighting option if flag enabled', () => {
    audioBrowserProxy.isPhraseHighlightingEnabledFlag = true;
    createAudioMenu();

    const menu = audioMenu.$.menu.$.lazyMenu.get();
    const options =
        Array.from(menu.querySelectorAll<HTMLButtonElement>('.dropdown-item'));
    const titles = options.map(button => button.textContent?.trim());
    assertEquals(5, titles.length);
    assertTrue(titles.includes('Phrase'));
  });

  test('does not have phrase highlighting option if flag disabled', () => {
    audioBrowserProxy.isPhraseHighlightingEnabledFlag = false;
    createAudioMenu();

    const menu = audioMenu.$.menu.$.lazyMenu.get();
    const options =
        Array.from(menu.querySelectorAll<HTMLButtonElement>('.dropdown-item'));
    const titles = options.map(button => button.textContent?.trim());
    assertEquals(4, titles.length);
    assertFalse(titles.includes('Phrase'));
  });

  test('restores saved highlight option', async () => {
    createAudioMenu();
    const granularity = audioBrowserProxy.getSentenceHighlighting();
    const startingSelected =
        audioMenu.$.menu.menuGroups[0]!.items.find(item => item.selected);
    assertNotEquals(granularity, startingSelected?.data);

    audioMenu.settingsPrefs = {
      ...DEFAULT_SETTINGS,
      highlightGranularity: granularity,
    };
    await microtasksFinished();

    const newSelected =
        audioMenu.$.menu.menuGroups[0]!.items.find(item => item.selected);
    assertEquals(granularity, newSelected?.data);
  });

  test('does nothing if saved highlight is the same', async () => {
    createAudioMenu();
    const startingSelected =
        audioMenu.$.menu.menuGroups[0]!.items.find(item => item.selected);

    audioMenu.settingsPrefs = {
      ...TEST_RANDOM_VALUE_SETTINGS,
      highlightGranularity: 0,
    };
    await microtasksFinished();

    const newSelected =
        audioMenu.$.menu.menuGroups[0]!.items.find(item => item.selected);
    assertEquals(startingSelected?.data, newSelected?.data);
  });

  test('can be closed programmatically', () => {
    createAudioMenu();
    stubAnimationFrame();
    audioMenu.open(document.body);
    assertTrue(audioMenu.$.menu.$.lazyMenu.get().open);
    audioMenu.close();
    assertFalse(audioMenu.$.menu.$.lazyMenu.get().open);
  });
});
