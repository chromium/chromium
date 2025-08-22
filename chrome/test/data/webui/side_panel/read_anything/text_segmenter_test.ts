// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {TextSegmenter} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

suite('TextSegmenter', () => {
  test('getWordCount returns expected word count', () => {
    const segmenter = TextSegmenter.getInstance();
    assertEquals(0, segmenter.getWordCount(''));
    assertEquals(0, segmenter.getWordCount(' '));
    assertEquals(0, segmenter.getWordCount('.'));
    assertEquals(0, segmenter.getWordCount(', .'));
    assertEquals(4, segmenter.getWordCount(', heels, nails, blade , mascara'));
    assertEquals(5, segmenter.getWordCount('ready for my napalm era'));
    assertEquals(8, segmenter.getWordCount('do-re-mi-fa-so-la-ti-do'));
  });
});
