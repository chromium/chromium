// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getCurrentSpeechRate} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

import {suppressInnocuousErrors} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';

suite('Common', () => {
  setup(() => {
    suppressInnocuousErrors();
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
  });

  suite('getCurrentSpeechRate', () => {
    test('rounds value to 1 decimal', () => {
      chrome.readingMode.speechRate = 1.1234567890;
      assertEquals(1.1, getCurrentSpeechRate());

      chrome.readingMode.speechRate = 0.912345678;
      assertEquals(0.9, getCurrentSpeechRate());

      chrome.readingMode.speechRate = 1.199999999;
      assertEquals(1.2, getCurrentSpeechRate());
    });
  });
});
