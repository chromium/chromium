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

  // The purpose of this test is intended to verify that
  // textSegmenter.updateLanguage is being used correctly, rather than
  // testing the lower level segmentation rules.
  test('updateLanguage', () => {
    // In Greek, ';' is interpreted as a question mark.
    const text = 'Πού είσαι; Είμαι στο σπίτι.';
    const textSegmenter = TextSegmenter.getInstance();

    textSegmenter.updateLanguage('en-us');
    assertEquals(1, textSegmenter.getSentences(text).length);

    textSegmenter.updateLanguage('el');
    assertEquals(2, textSegmenter.getSentences(text).length);

    textSegmenter.updateLanguage('fr-fr');
    assertEquals(1, textSegmenter.getSentences(text).length);
  });

  test('getSentences', () => {
    const textSegmenter = TextSegmenter.getInstance();

    const sentence1 = 'I don\'t think you\'re ready for the takedown. ';
    const sentence2 =
        'Break you into peices in the world of pain \'cause you\'re all ' +
        'the same. ';
    const sentence3 = 'Yeah, it\'s a takedown.';
    const text = sentence1 + sentence2 + sentence3;
    const sentences = textSegmenter.getSentences(text);

    assertEquals(3, sentences.length);
    assertEquals(sentence1, sentences[0]!.text);
    assertEquals(0, sentences[0]!.index);

    assertEquals(sentence2, sentences[1]!.text);
    assertEquals(sentence1.length, sentences[1]!.index);

    assertEquals(sentence3, sentences[2]!.text);
    assertEquals(sentence1.length + sentence2.length, sentences[2]!.index);
  });

  test('getSentences in scriptio continua language', () => {
    const textSegmenter = TextSegmenter.getInstance();
    textSegmenter.updateLanguage('jp');

    const sentence1 = '市場に行ってフルーツをたくさん買ったよ？';
    const sentence2 = 'すごく美味しいよ。';
    const text = sentence1 + sentence2;
    const sentences = textSegmenter.getSentences(text);

    assertEquals(2, sentences.length);
    assertEquals(sentence1, sentences[0]!.text);
    assertEquals(0, sentences[0]!.index);

    assertEquals(sentence2, sentences[1]!.text);
    assertEquals(sentence1.length, sentences[1]!.index);
  });

  test('getNextWordEnd', () => {
    const textSegmenter = TextSegmenter.getInstance();
    const word1 = 'onomatopoeia';
    const word2 = 'party';
    const text = word1 + ' ' + word2;
    const endIndex = textSegmenter.getNextWordEnd(text);
    assertEquals(word1.length, endIndex);
    assertEquals(word1, text.slice(0, endIndex));
  });

  test('getNextWordEnd only one word', () => {
    const textSegmenter = TextSegmenter.getInstance();
    const text = 'happiness';
    const endIndex = textSegmenter.getNextWordEnd(text);
    assertEquals(text.length, endIndex);
    assertEquals(text, text.slice(0, endIndex));
  });

  test('getNextWordEnd with text beginning with non-words', () => {
    const textSegmenter = TextSegmenter.getInstance();
    const nonWordText = '... \ ? ';
    const word = 'axolotl';
    const text = nonWordText + word;
    const endIndex = textSegmenter.getNextWordEnd(text);
    assertEquals(text.length, endIndex);
    assertEquals(text, text.slice(0, endIndex));
  });

  test(
      'getSentences groups trailing opening punctuation with next sentence',
      () => {
        const textSegmenter = TextSegmenter.getInstance();
        const text = 'hello.[2]';
        const sentences = textSegmenter.getSentences(text);
        assertEquals(2, sentences.length);
        assertEquals('hello.', sentences[0]!.text);
        assertEquals('[2]', sentences[1]!.text);
      });

  test(
      'getSentences groups trailing duplicate opening punctuation with next sentence',
      () => {
        const textSegmenter = TextSegmenter.getInstance();
        const text = 'hello.[[[2]';
        const sentences = textSegmenter.getSentences(text);
        assertEquals(2, sentences.length);
        assertEquals('hello.', sentences[0]!.text);
        assertEquals('[[[2]', sentences[1]!.text);
      });

  test('getSentences does not adjust mid-sentence punctuation', () => {
    const textSegmenter = TextSegmenter.getInstance();
    const text = 'hello, (goodbye)- how are you?';
    const sentences = textSegmenter.getSentences(text);
    assertEquals(1, sentences.length);
    assertEquals('hello, (goodbye)- how are you?', sentences[0]!.text);
  });

  test(
      'getSentences does not adjust mid-sentence punctuation with ending opening punctuation',
      () => {
        const textSegmenter = TextSegmenter.getInstance();
        const text = 'hello, [goodbye]- what\'s up?[2]';
        const sentences = textSegmenter.getSentences(text);
        assertEquals(2, sentences.length);
        assertEquals('hello, [goodbye]- what\'s up?', sentences[0]!.text);
        assertEquals('[2]', sentences[1]!.text);
      });
});
