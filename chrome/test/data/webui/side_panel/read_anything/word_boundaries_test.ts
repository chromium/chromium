// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy, WordBoundaries} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {WordBoundaryState} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('WordBoundaries', () => {
  let wordBoundaries: WordBoundaries;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;

    wordBoundaries = new WordBoundaries();
  });

  test('default state', () => {
    const state: WordBoundaryState = wordBoundaries.state;
    assertEquals(0, state.previouslySpokenIndex);
    assertEquals(0, state.speechUtteranceStartIndex);
    assertEquals(0, state.speechUtteranceLength);
    assertTrue(wordBoundaries.notSupported());
    assertFalse(wordBoundaries.hasBoundaries());
  });

  test('has boundaries after update boundary', () => {
    wordBoundaries.updateBoundary(15);

    assertEquals(15, wordBoundaries.state.previouslySpokenIndex);
    assertEquals(0, wordBoundaries.state.speechUtteranceLength);
    assertTrue(wordBoundaries.hasBoundaries());
    assertFalse(wordBoundaries.notSupported());
  });

  test('updates charLength', () => {
    const charLength = 6;
    const charIndex = 12;

    wordBoundaries.updateBoundary(charIndex, charLength);

    assertEquals(charIndex, wordBoundaries.state.previouslySpokenIndex);
    assertEquals(charLength, wordBoundaries.state.speechUtteranceLength);
    assertTrue(wordBoundaries.hasBoundaries());
    assertFalse(wordBoundaries.notSupported());
  });

  test('reset to default state after update boundary', () => {
    wordBoundaries.updateBoundary(15, 10);

    wordBoundaries.resetToDefaultState();

    const state: WordBoundaryState = wordBoundaries.state;
    assertEquals(0, state.previouslySpokenIndex);
    assertEquals(0, state.speechUtteranceStartIndex);
    assertEquals(0, state.speechUtteranceLength);
    assertFalse(wordBoundaries.notSupported());
    assertFalse(wordBoundaries.hasBoundaries());
  });

  test('getResumeBoundary', () => {
    wordBoundaries.updateBoundary(5);
    assertEquals(5, wordBoundaries.getResumeBoundary());

    wordBoundaries.updateBoundary(5);
    wordBoundaries.updateBoundary(13);
    assertEquals(18, wordBoundaries.getResumeBoundary());

    wordBoundaries.updateBoundary(5);
    wordBoundaries.updateBoundary(13);
    wordBoundaries.updateBoundary(2);
    assertEquals(20, wordBoundaries.getResumeBoundary());
  });
});
