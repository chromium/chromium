// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {emitEvent, suppressInnocuousErrors, waitForPlayFromSelection} from './common.js';

suite('ReadAloudHighlight', () => {
  let app: AppElement;
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

  function emitNextGranularity() {
    emitEvent(app, ToolbarEvent.NEXT_GRANULARITY);
    return microtasksFinished();
  }

  function emitPreviousGranularity() {
    emitEvent(app, ToolbarEvent.PREVIOUS_GRANULARITY);
    return microtasksFinished();
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

  test('on speak first sentence highlights are correct', () => {
    app.playSpeech();
    const currentHighlight =
        app.$.container.querySelector('.current-read-highlight');
    const previousHighlight =
        app.$.container.querySelector('.previous-read-highlight');

    assertEquals(sentence1, currentHighlight!.textContent);
    assertFalse(!!previousHighlight);
  });

  suite('on sentence spread across multiple segments', () => {
    let currentHighlights: NodeListOf<Element>;
    let previousHighlights: NodeListOf<Element>;

    setup(() => {
      app.playSpeech();
      emitNextGranularity();
      emitNextGranularity();
      return microtasksFinished();
    });

    test('all segments highlighted', () => {
      currentHighlights =
          app.$.container.querySelectorAll('.current-read-highlight');
      previousHighlights =
          app.$.container.querySelectorAll('.previous-read-highlight');

      assertEquals(2, previousHighlights.length);
      assertEquals(sentence1, previousHighlights[0]!.textContent);
      assertEquals(sentence2, previousHighlights[1]!.textContent);
      assertEquals(2, currentHighlights.length);
      assertEquals(sentenceSegment1, currentHighlights[0]!.textContent);
      assertEquals(sentenceSegment2, currentHighlights[1]!.textContent);
    });

    test('going back after multiple segments resets all segments', () => {
      emitPreviousGranularity();

      currentHighlights =
          app.$.container.querySelectorAll('.current-read-highlight');
      previousHighlights =
          app.$.container.querySelectorAll('.previous-read-highlight');

      assertEquals(1, previousHighlights.length);
      assertEquals(sentence1, previousHighlights[0]!.textContent);
      assertEquals(1, currentHighlights.length);
      assertEquals(sentence2, currentHighlights[0]!.textContent);
    });
  });

  test('on speak next sentence highlights are correct', async () => {
    app.playSpeech();
    await emitNextGranularity();
    const currentHighlight =
        app.$.container.querySelector('.current-read-highlight');
    const previousHighlight =
        app.$.container.querySelector('.previous-read-highlight');

    assertEquals(sentence2, currentHighlight!.textContent);
    assertEquals(sentence1, previousHighlight!.textContent);
  });

  suite('on finish speaking', () => {
    let currentHighlight: HTMLElement|null;
    let previousHighlights: NodeListOf<Element>;

    setup(async () => {
      app.playSpeech();
      await emitNextGranularity();
      await emitNextGranularity();
      await emitNextGranularity();

      currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      previousHighlights =
          app.$.container.querySelectorAll('.previous-read-highlight');
    });

    test('no highlights and keeps content', () => {
      assertFalse(!!currentHighlight);
      assertEquals(0, previousHighlights.length);

      const expectedText =
          sentence1 + sentence2 + sentenceSegment1 + sentenceSegment2;
      assertEquals(expectedText, app.$.container.textContent);
    });
  });

  suite('on speak previous sentence', () => {
    let currentHighlight: HTMLElement|null;
    let previousHighlights: NodeListOf<Element>;

    setup(async () => {
      app.playSpeech();
      await emitNextGranularity();
      await emitPreviousGranularity();

      currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      previousHighlights =
          app.$.container.querySelectorAll('.previous-read-highlight');
    });

    test('previous sentence is now current and nothing marked previous', () => {
      assertEquals(sentence1, currentHighlight!.textContent);
      assertEquals(0, previousHighlights.length);
    });

    test('going back before first sentence does not crash', async () => {
      await emitPreviousGranularity();
      await emitPreviousGranularity();
      await emitPreviousGranularity();
      await emitPreviousGranularity();

      currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      previousHighlights =
          app.$.container.querySelectorAll('.previous-read-highlight');

      assertEquals(sentence1, currentHighlight!.textContent);
    });

    test(
        'going forward after going back shows correct highlights', async () => {
          await emitNextGranularity();
          currentHighlight =
              app.$.container.querySelector('.current-read-highlight');
          previousHighlights =
              app.$.container.querySelectorAll('.previous-read-highlight');

          assertEquals(sentence2, currentHighlight!.textContent);
          assertEquals(1, previousHighlights.length);
          assertEquals(sentence1, previousHighlights[0]!.textContent);

          await emitNextGranularity();
          const currentHighlights =
              app.$.container.querySelectorAll('.current-read-highlight');
          previousHighlights =
              app.$.container.querySelectorAll('.previous-read-highlight');

          assertEquals(2, currentHighlights.length);
          assertEquals(sentenceSegment1, currentHighlights[0]!.textContent);
          assertEquals(sentenceSegment2, currentHighlights[1]!.textContent);
          assertEquals(2, previousHighlights.length);
          assertEquals(sentence1, previousHighlights[0]!.textContent);
          assertEquals(sentence2, previousHighlights[1]!.textContent);
        });
  });

  suite('on speaking from selection', async () => {
    let currentHighlight: HTMLElement|null;
    let previousHighlights: NodeListOf<Element>;

    async function selectAndPlay(
        anchorId: number, anchorOffset: number, focusId: number,
        focusOffset: number): Promise<void> {
      const selectedTree = Object.assign(
          {
            selection: {
              anchor_object_id: anchorId,
              focus_object_id: focusId,
              anchor_offset: anchorOffset,
              focus_offset: focusOffset,
              is_backward: false,
            },
          },
          axTree);
      chrome.readingMode.setContentForTesting(selectedTree, leafIds);
      await microtasksFinished();
      app.updateSelection();
      await microtasksFinished();
      app.playSpeech();
      return waitForPlayFromSelection();
    }

    setup(() => {
      return selectAndPlay(3, 1, 3, 5);
    });

    test('shows correct highlights', () => {
      currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      previousHighlights =
          app.$.container.querySelectorAll('.previous-read-highlight');

      assertEquals(sentence2, currentHighlight!.textContent);
      assertEquals(1, previousHighlights!.length);
      assertEquals(sentence1, previousHighlights![0]!.textContent);
    });

    test('next granularity shows correct highlights', async () => {
      await emitNextGranularity();

      currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      previousHighlights =
          app.$.container.querySelectorAll('.previous-read-highlight');
      assertEquals(sentenceSegment1, currentHighlight!.textContent);
      assertEquals(2, previousHighlights!.length);
      assertEquals(sentence1, previousHighlights![0]!.textContent);
      assertEquals(sentence2, previousHighlights![1]!.textContent);
    });

    test('previous granularity shows correct highlights', async () => {
      await emitPreviousGranularity();

      currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      previousHighlights =
          app.$.container.querySelectorAll('.previous-read-highlight');
      assertEquals(sentence1, currentHighlight!.textContent);
      assertEquals(0, previousHighlights!.length);
    });
  });
});
