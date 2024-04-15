// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';

import type {ReadAnythingElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/app.js';
import {NEXT_GRANULARITY_EVENT, PREVIOUS_GRANULARITY_EVENT} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything_toolbar.js';
import {assertEquals, assertFalse} from 'chrome-untrusted://webui-test/chai_assert.js';

import {emitEvent, suppressInnocuousErrors} from './common.js';

suite('ReadAloudHighlight', () => {
  let app: ReadAnythingElement;
  const sentence1 = 'Only need the light when it\'s burning low.\n';
  const sentence2 = 'Only miss the sun when it starts to snow.\n';
  const sentenceSegment1 = 'Only know you love her when you let her go';
  const sentenceSegment2 = ', and you let her go.';
  const leafIds = [2, 3, 4, 5];
  const axTree = {
    rootId: 1,
    nodes: [
      {
        id: 1,
        role: 'rootWebArea',
        htmlTag: '#document',
        childIds: leafIds,
      },
      {
        id: 2,
        role: 'staticText',
        name: sentence1,
      },
      {
        id: 3,
        role: 'staticText',
        name: sentence2,
      },
      {
        id: 4,
        role: 'staticText',
        name: sentenceSegment1,
      },
      {
        id: 5,
        role: 'staticText',
        name: sentenceSegment2,
      },
    ],
  };

  function emitNextGranularity(): void {
    emitEvent(app, NEXT_GRANULARITY_EVENT);
  }

  function emitPreviousGranularity(): void {
    emitEvent(app, PREVIOUS_GRANULARITY_EVENT);
  }

  setup(() => {
    suppressInnocuousErrors();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Do not call the real `onConnected()`. As defined in
    // ReadAnythingAppController, onConnected creates mojo pipes to connect to
    // the rest of the Read Anything feature, which we are not testing here.
    chrome.readingMode.onConnected = () => {};

    app = document.createElement('read-anything-app');
    document.body.appendChild(app);
    chrome.readingMode.setContentForTesting(axTree, leafIds);
  });

  suite('on speak first sentence', () => {
    let currentHighlight: HTMLElement|null;
    let previousHighlight: HTMLElement|null;

    setup(() => {
      app.playSpeech();
      currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      previousHighlight =
          app.$.container.querySelector('.previous-read-highlight');
    });

    test('sentence is highlighted', () => {
      assertEquals(currentHighlight!.textContent, sentence1);
    });

    test('no previous highlight', () => {
      assertFalse(!!previousHighlight);
    });
  });

  suite('on sentence spread across multiple segments', () => {
    let currentHighlights: NodeListOf<Element>;
    let previousHighlights: NodeListOf<Element>;

    setup(() => {
      app.playSpeech();
      emitNextGranularity();
      emitNextGranularity();
    });

    test('all segments highlighted', () => {
      currentHighlights =
          app.$.container.querySelectorAll('.current-read-highlight');
      previousHighlights =
          app.$.container.querySelectorAll('.previous-read-highlight');

      assertEquals(previousHighlights.length, 2);
      assertEquals(previousHighlights[0]!.textContent, sentence1);
      assertEquals(previousHighlights[1]!.textContent, sentence2);
      assertEquals(currentHighlights.length, 2);
      assertEquals(currentHighlights[0]!.textContent, sentenceSegment1);
      assertEquals(currentHighlights[1]!.textContent, sentenceSegment2);
    });

    test('going back after multiple segments resets all segments', () => {
      emitPreviousGranularity();

      currentHighlights =
          app.$.container.querySelectorAll('.current-read-highlight');
      previousHighlights =
          app.$.container.querySelectorAll('.previous-read-highlight');

      assertEquals(previousHighlights.length, 1);
      assertEquals(previousHighlights[0]!.textContent, sentence1);
      assertEquals(currentHighlights.length, 1);
      assertEquals(currentHighlights[0]!.textContent, sentence2);
    });
  });

  suite('on speak next sentence', () => {
    let currentHighlight: HTMLElement|null;
    let previousHighlight: HTMLElement|null;

    setup(() => {
      app.playSpeech();
      emitNextGranularity();
      currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      previousHighlight =
          app.$.container.querySelector('.previous-read-highlight');
    });

    test('sentence is highlighted', () => {
      assertEquals(currentHighlight!.textContent, sentence2);
    });

    test('previous sentence has highlight', () => {
      assertEquals(previousHighlight!.textContent, sentence1);
    });
  });

  suite('on finish speaking', () => {
    let currentHighlight: HTMLElement|null;
    let previousHighlight: NodeListOf<Element>;

    setup(() => {
      app.playSpeech();
      emitNextGranularity();
      emitNextGranularity();
      emitNextGranularity();

      currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      previousHighlight =
          app.$.container.querySelectorAll('.previous-read-highlight');
    });

    test('no current highlight', () => {
      assertFalse(!!currentHighlight);
    });

    test('all sentences are marked previous', () => {
      assertEquals(previousHighlight.length, leafIds.length);
      assertEquals(previousHighlight[0]!.textContent, sentence1);
      assertEquals(previousHighlight[1]!.textContent, sentence2);
      assertEquals(previousHighlight[2]!.textContent, sentenceSegment1);
      assertEquals(previousHighlight[3]!.textContent, sentenceSegment2);
    });

    test('playing next granularity does not crash', () => {
      emitNextGranularity();
      emitNextGranularity();
    });
  });

  suite('on speak previous sentence', () => {
    let currentHighlight: HTMLElement|null;
    let previousHighlights: NodeListOf<Element>;

    setup(() => {
      app.playSpeech();
      emitNextGranularity();
      emitPreviousGranularity();

      currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      previousHighlights =
          app.$.container.querySelectorAll('.previous-read-highlight');
    });

    test('previous sentence is now current', () => {
      assertEquals(currentHighlight!.textContent, sentence1);
    });

    test('nothing marked previous', () => {
      assertEquals(previousHighlights.length, 0);
    });

    test('going back before first sentence does not crash', () => {
      emitPreviousGranularity();
      emitPreviousGranularity();
      emitPreviousGranularity();
      emitPreviousGranularity();

      currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      previousHighlights =
          app.$.container.querySelectorAll('.previous-read-highlight');

      assertEquals(currentHighlight!.textContent, sentence1);
    });

    test('going forward after going back shows correct highlights', () => {
      emitNextGranularity();
      currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      previousHighlights =
          app.$.container.querySelectorAll('.previous-read-highlight');

      assertEquals(currentHighlight!.textContent, sentence2);
      assertEquals(previousHighlights.length, 1);
      assertEquals(previousHighlights[0]!.textContent, sentence1);

      emitNextGranularity();
      const currentHighlights =
          app.$.container.querySelectorAll('.current-read-highlight');
      previousHighlights =
          app.$.container.querySelectorAll('.previous-read-highlight');

      assertEquals(currentHighlights.length, 2);
      assertEquals(currentHighlights[0]!.textContent, sentenceSegment1);
      assertEquals(currentHighlights[1]!.textContent, sentenceSegment2);
      assertEquals(previousHighlights.length, 2);
      assertEquals(previousHighlights[0]!.textContent, sentence1);
      assertEquals(previousHighlights[1]!.textContent, sentence2);
    });
  });
});
