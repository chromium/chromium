// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getWordCount} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

import {FakeReadingMode} from './fake_reading_mode.js';

suite('Common', () => {
  setup(() => {
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
  });


  test('getWordCount', () => {
    assertEquals(5, getWordCount('Nothing but the truth now'));
    assertEquals(8, getWordCount('Nothing but the\nproof of\n what I am'));
    assertEquals(1, getWordCount('TheworstofwhatIcamefrom'));
    assertEquals(
        4, getWordCount('patterns           I\'m ashamed\n\n\n\n\n\n   of'));
  });
});
