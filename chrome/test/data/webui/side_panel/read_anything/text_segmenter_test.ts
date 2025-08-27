// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {TextSegmenter} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

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

  // TODO: crbug.com/440400392- Once sentence segmentation is added, add
  // tests to verify updateLanguage. It was difficult to find any examples
  // of the language making a difference for word count because the segmenter
  // is good enough at recognizing the input language to handle word breakdown.

  test('getAccessibleBoundary when max length cuts off sentence', () => {
    const firstSentence = 'This is a normal sentence. ';
    const secondSentence = 'This is a second sentence.';
    const combinedSentence = firstSentence + secondSentence;
    const index = TextSegmenter.getInstance().getAccessibleBoundary(
        combinedSentence, firstSentence.length - 3);
    assertTrue(index < firstSentence.length);
    assertEquals('This is a normal ', combinedSentence.slice(0, index));
  });

  test('getAccessibleBoundary when text longer than max length', () => {
    const firstSentence = 'This is a normal sentence. ';
    const secondSentence = 'This is a second sentence.';

    const combinedSentence = firstSentence + secondSentence;
    const index = TextSegmenter.getInstance().getAccessibleBoundary(
        combinedSentence, firstSentence.length + secondSentence.length - 5);
    assertEquals(index, firstSentence.length);
    assertEquals(firstSentence, combinedSentence.slice(0, index));
  });

  test(
      'getAccessibleBoundary with one sentence when max length cuts off sentence',
      () => {
        const sentence = 'Hello, this is a normal sentence.';

        const index =
            TextSegmenter.getInstance().getAccessibleBoundary(sentence, 12);
        assertTrue(index < sentence.length);
        assertEquals('Hello, ', sentence.slice(0, index));
      });

  test('getAccessibleBoundary for word', () => {
    const text = 'Hello there.This/is\ntesting.';
    const newLineLocation = text.indexOf('\n');
    const index = TextSegmenter.getInstance().getAccessibleBoundary(text, 1000);
    assertEquals(newLineLocation + 1, index);
    assertEquals('Hello there.This/is\n', text.slice(0, index));
  });
});
