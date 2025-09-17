// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {ContentController, getReadAloudModel, ReadAloudHighlighter, SelectionController, SpeechBrowserProxyImpl, SpeechController, ToolbarEvent, VoiceLanguageController, WordBoundaries} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertLE, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createApp, createSpeechSynthesisVoice, emitEvent, playFromSelectionWithMockTimer, setSimpleAxTreeWithText} from './common.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';

suite('WordHighlighting', () => {
  let app: AppElement;
  let speech: TestSpeechBrowserProxy;
  let wordBoundaries: WordBoundaries;
  let speechController: SpeechController;
  let selectionController: SelectionController;

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
    speech = new TestSpeechBrowserProxy();
    SpeechBrowserProxyImpl.setInstance(speech);
    VoiceLanguageController.setInstance(new VoiceLanguageController());
    wordBoundaries = new WordBoundaries();
    WordBoundaries.setInstance(wordBoundaries);
    ReadAloudHighlighter.setInstance(new ReadAloudHighlighter());
    selectionController = new SelectionController();
    SelectionController.setInstance(selectionController);
    speechController = new SpeechController();
    SpeechController.setInstance(speechController);
    ContentController.setInstance(new ContentController());

    // Reset the browser proxy.
    getReadAloudModel().resetModel?.();

    app = await createApp();
    chrome.readingMode.setContentForTesting(axTree, [2, 4]);
    chrome.readingMode.onSpeechRateChange(1);
  });

  test('word highlight used', () => {
    wordBoundaries.updateBoundary(10);
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);

    const currentHighlight =
        app.$.container.querySelector('.current-read-highlight');
    assertTrue(!!currentHighlight);
    // Sometimes the word returned can be "link", "link.", or "link. " which
    // can create flaky tests. Therefore, just check that the highlighted
    // text starts with "link" and isn't longer than the string would be if it
    // were "link. "
    // TODO(crbug.com/301131238): Investigate why there's a discrepancy here.
    assertTrue(currentHighlight.textContent!.startsWith('link'));
    assertTrue(currentHighlight.textContent!.length < 6);
  });

  test('with rate over 1 sentence highlight used', () => {
    wordBoundaries.updateBoundary(10);
    chrome.readingMode.onSpeechRateChange(2);
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);

    const currentHighlight =
        app.$.container.querySelector('.current-read-highlight');
    assertTrue(!!currentHighlight);
    assertEquals('This is a link.', currentHighlight.textContent);
  });

  test('with no word boundary sentence highlight used', () => {
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);

    const currentHighlight =
        app.$.container.querySelector('.current-read-highlight');
    assertTrue(!!currentHighlight);
    assertEquals('This is a link.', currentHighlight.textContent);
  });

  test('word highlighting with only punctuation skips highlight', () => {
    setSimpleAxTreeWithText('.?!\'\",(){}[]');
    wordBoundaries.updateBoundary(10);
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);

    const currentHighlight =
        app.$.container.querySelector('.current-read-highlight');
    assertFalse(!!currentHighlight);
  });

  test('word highlighting time with charLength uses charLength', () => {
    const text = '4:00pm';
    setSimpleAxTreeWithText(text);
    wordBoundaries.updateBoundary(0, text.length);
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);

    const currentHighlight =
        app.$.container.querySelector('.current-read-highlight');
    assertTrue(!!currentHighlight);
    assertEquals(text, currentHighlight.textContent);
  });

  test('word highlighting time without charLength uses ax pos', () => {
    const text = '4:00pm';
    setSimpleAxTreeWithText(text);
    wordBoundaries.updateBoundary(0);
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);

    const currentHighlight =
        app.$.container.querySelector('.current-read-highlight');
    assertTrue(!!currentHighlight);
    let expectedHighlight = '4';
    if (!chrome.readingMode.isTsTextSegmentationEnabled) {
      // The V8 model appends the colon to the highlight.
      expectedHighlight += ':';
    }
    assertEquals(expectedHighlight, currentHighlight.textContent);
  });

  test('word highlighting date across nodes with charLength', () => {
    const axTree = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          htmlTag: '#document',
          childIds: [2, 4, 5],
        },
        {
          id: 2,
          htmlTag: 'b',
          childIds: [3],
        },
        {
          id: 3,
          role: 'staticText',
          name: 'April',
        },
        {
          id: 4,
          role: 'staticText',
          name: ' 18,',
        },
        {
          id: 5,
          role: 'link',
          htmlTag: 'a',
          url: 'http://www.google.com',
          childIds: [6],
        },
        {
          id: 6,
          role: 'staticText',
          name: ' 2025',
        },
      ],
    };
    chrome.readingMode.setContentForTesting(axTree, [3, 4, 6]);
    wordBoundaries.updateBoundary(0, 14);
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);

    const currentHighlight =
        app.$.container.querySelector('.current-read-highlight');
    assertTrue(!!currentHighlight);
    assertEquals('April', currentHighlight.textContent);
  });

  test('word highlighting date across nodes without charLength', () => {
    const axTree = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          htmlTag: '#document',
          childIds: [2, 4, 5],
        },
        {
          id: 2,
          htmlTag: 'b',
          childIds: [3],
        },
        {
          id: 3,
          role: 'staticText',
          name: 'April',
        },
        {
          id: 4,
          role: 'staticText',
          name: '18',
        },
        {
          id: 5,
          role: 'link',
          htmlTag: 'a',
          url: 'http://www.google.com',
          childIds: [6],
        },
        {
          id: 6,
          role: 'staticText',
          name: '2025',
        },
      ],
    };
    chrome.readingMode.setContentForTesting(axTree, [3, 4, 6]);
    wordBoundaries.updateBoundary(0);
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);

    const currentHighlight =
        app.$.container.querySelector('.current-read-highlight');
    assertTrue(!!currentHighlight);
    assertEquals('April', currentHighlight.textContent);
  });

  test('word highlighting with single alphabet character has highlight', () => {
    setSimpleAxTreeWithText('a');
    wordBoundaries.updateBoundary(0);
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);

    const currentHighlight =
        app.$.container.querySelector('.current-read-highlight');
    assertTrue(!!currentHighlight);
    assertEquals('a', currentHighlight.textContent);
  });

  test('word highlighting skipping', () => {
    const toTest =
        ['[', ']', '(', ')', '.', ',', '?', '!', '{', '}', '\'', '\"'];

    for (const char of toTest) {
      setSimpleAxTreeWithText(char);
      wordBoundaries.updateBoundary(0);
      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      const currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      assertFalse(!!currentHighlight);
    }
  });

  test('on speaking from selection, word boundary state reset', () => {
    const anchorIndex = 1;
    const focusIndex = 2;
    const anchorOffset = 0;
    const focusOffset = 1;
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);
    wordBoundaries.updateBoundary(2);
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);

    // Update the selection directly on the document.
    const spans = app.$.container.querySelectorAll('span');
    assertLE(2, spans.length);
    const anchor = spans[anchorIndex]!;
    const focus = spans[focusIndex]!;
    const range = document.createRange();
    range.setStart(anchor, anchorOffset);
    range.setEnd(focus, focusOffset);

    const selection = document.getSelection();
    assertTrue(!!selection);
    selection.addRange(range);
    selectionController.onSelectionChange(selection);

    playFromSelectionWithMockTimer(app);

    const currentHighlight =
        app.$.container.querySelector('.current-read-highlight');

    // Verify that we're highlighting from the selected point.
    assertTrue(!!currentHighlight);
    assertTrue(!!currentHighlight.textContent);
    let expectedHighlight = 'This';
    if (!chrome.readingMode.isTsTextSegmentationEnabled) {
      // The V8 model appends a space in the word highlight.
      expectedHighlight += ' ';
    }
    assertEquals(expectedHighlight, currentHighlight.textContent);
    // Verify that the word boundary state has been reset.
    assertFalse(wordBoundaries.hasBoundaries());
  });

  test('sentence highlight used with espeak voice', () => {
    const selectedVoice =
        createSpeechSynthesisVoice({lang: 'en', name: 'Kristi eSpeak'});
    emitEvent(app, ToolbarEvent.VOICE, {detail: {selectedVoice}});
    const sentence = 'Hello, how are you!';
    setSimpleAxTreeWithText(sentence);
    wordBoundaries.updateBoundary(0);
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);

    const currentHighlight =
        app.$.container.querySelector('.current-read-highlight');
    assertTrue(currentHighlight !== undefined);
    assertEquals(sentence, currentHighlight!.textContent);
  });
});
