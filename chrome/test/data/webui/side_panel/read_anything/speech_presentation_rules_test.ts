// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getCurrentSpeechRate, isInvalidHighlightForWordHighlighting, textEndsWithOpeningPunctuation} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {FakeReadingMode} from './fake_reading_mode.js';

suite('SpeechPresentationRules', () => {
  setup(() => {
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
  });

  test('getCurrentSpeechRate rounds value to 1 decimal', () => {
    chrome.readingMode.speechRate = 1.1234567890;
    assertEquals(1.1, getCurrentSpeechRate());

    chrome.readingMode.speechRate = 0.912345678;
    assertEquals(0.9, getCurrentSpeechRate());

    chrome.readingMode.speechRate = 1.199999999;
    assertEquals(1.2, getCurrentSpeechRate());
  });

  test('isInvalidHighlightForWordHighlighting', () => {
    assertTrue(isInvalidHighlightForWordHighlighting());
    assertTrue(isInvalidHighlightForWordHighlighting(''));
    assertTrue(isInvalidHighlightForWordHighlighting(' '));
    assertTrue(isInvalidHighlightForWordHighlighting('  '));
    assertTrue(isInvalidHighlightForWordHighlighting('!'));
    assertTrue(isInvalidHighlightForWordHighlighting('()?!?'));
    assertFalse(isInvalidHighlightForWordHighlighting('hello !!!'));
    assertFalse(isInvalidHighlightForWordHighlighting('(psst);'));
  });

  test('speechEndsWithOpeningPunctuation', () => {
    assertNull(textEndsWithOpeningPunctuation(' '));
    assertNull(textEndsWithOpeningPunctuation('how()'));
    assertEquals('[', textEndsWithOpeningPunctuation('[')![0]);
    assertEquals('[', textEndsWithOpeningPunctuation('hello[')![0]);
    assertEquals('{', textEndsWithOpeningPunctuation('goodbye{')![0]);
    assertEquals('<', textEndsWithOpeningPunctuation('where?<')![0]);
    assertEquals('(', textEndsWithOpeningPunctuation('why(')![0]);
    assertEquals('((', textEndsWithOpeningPunctuation('when((')![0]);
  });
});
