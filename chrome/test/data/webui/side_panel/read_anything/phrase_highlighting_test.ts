// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {ReadAloudHighlighter, SpeechController, ToolbarEvent, VoiceLanguageController, WordBoundaries} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createApp, emitEvent} from './common.js';

suite('PhraseHighlighting', () => {
  let app: AppElement;
  let wordBoundaries: WordBoundaries;

  // root htmlTag='#document' id=1
  // ++link htmlTag='a' url='http://www.google.com' id=2
  // ++++staticText name='This is a link.' id=3
  // ++link htmlTag='a' url='http://www.youtube.com' id=4
  // ++++staticText name='This is another link.' id=5
  const axTree = {
    rootId: 1,
    nodes: [
      {
        id: 1,
        role: 'rootWebArea',
        htmlTag: '#document',
        childIds: [2, 4],
      },
      {
        id: 2,
        role: 'link',
        htmlTag: 'a',
        url: 'http://www.google.com',
        childIds: [3],
      },
      {
        id: 3,
        role: 'staticText',
        name: 'This is a link.',
      },
      {
        id: 4,
        role: 'link',
        htmlTag: 'a',
        url: 'http://www.youtube.com',
        childIds: [5],
      },
      {
        id: 5,
        role: 'staticText',
        name: 'This is another link.',
      },
    ],
  };

  setup(async () => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Do not call the real `onConnected()`. As defined in
    // ReadAnythingAppController, onConnected creates mojo pipes to connect to
    // the rest of the Read Anything feature, which we are not testing here.
    chrome.readingMode.onConnected = () => {};

    VoiceLanguageController.setInstance(new VoiceLanguageController());
    wordBoundaries = new WordBoundaries();
    WordBoundaries.setInstance(wordBoundaries);
    ReadAloudHighlighter.setInstance(new ReadAloudHighlighter());
    SpeechController.setInstance(new SpeechController());
    app = await createApp();

    // Use a tree with just one sentence. For the actual implementation of
    // phrase segmentation, a more realistic example would be to use
    // setSimpleAxTreeWithText instead.
    chrome.readingMode.setContentForTesting(axTree, [2, 4]);
  });

  test('with word highlighting on, word is highlighted', () => {
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.wordHighlighting);

    wordBoundaries.updateBoundary(0);
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);
    const currentHighlight =
        app.$.container.querySelector('.current-read-highlight');
    assertTrue(currentHighlight !== undefined);
    assertEquals(currentHighlight!.textContent!, 'This ');
  });

  test('with phrase highlighting on, phrase is highlighted', () => {
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.phraseHighlighting);

    wordBoundaries.updateBoundary(0);
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);

    const currentHighlight =
        app.$.container.querySelector('.current-read-highlight');
    assertTrue(currentHighlight !== undefined);
    assertEquals(currentHighlight!.textContent!, 'This is a ');
  });

  test('with sentence highlighting on, sentence is highlighted', () => {
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.sentenceHighlighting);

    wordBoundaries.updateBoundary(0);
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);

    const currentHighlight =
        app.$.container.querySelector('.current-read-highlight');
    assertTrue(currentHighlight !== undefined);
    assertEquals(currentHighlight!.textContent!, 'This is a link.');
  });

  test('with highlighting off, sentence is highlighted', () => {
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.noHighlighting);

    wordBoundaries.updateBoundary(0);
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);

    const currentHighlight =
        app.$.container.querySelector('.current-read-highlight');
    assertTrue(currentHighlight !== undefined);
    assertEquals(currentHighlight!.textContent!, 'This is a link.');
  });

  suite('after a word boundary', () => {
    setup(() => {
      wordBoundaries.updateBoundary(0);
    });

    test('initially, phrase is highlighted', () => {
      chrome.readingMode.onHighlightGranularityChanged(
          chrome.readingMode.phraseHighlighting);
      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertTrue(currentHighlight !== undefined);
      assertEquals(currentHighlight!.textContent!, 'This is a ');
    });

    test('phrase highlight same after second word boundary', () => {
      chrome.readingMode.onHighlightGranularityChanged(
          chrome.readingMode.phraseHighlighting);
      wordBoundaries.updateBoundary(5);
      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertTrue(currentHighlight !== undefined);
      assertEquals(currentHighlight!.textContent!, 'This is a ');
    });

    test('phrase highlighting highlights second phrase', () => {
      chrome.readingMode.onHighlightGranularityChanged(
          chrome.readingMode.phraseHighlighting);
      wordBoundaries.updateBoundary(10);
      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertTrue(currentHighlight !== undefined);
      assertEquals(currentHighlight!.textContent!, 'link.');
    });

    // TODO(b/364327601): Add tests for unsupported language handling.
  });
});
