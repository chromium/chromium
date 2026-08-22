// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {AudioBrowserProxyImpl, ContentBrowserProxyImpl, ContentController, ContentType, NodeStore, playFromSelectionTimeout, SelectionController, setInstance, SpeechBrowserProxyImpl, SpeechController, ToolbarEvent, VisualBrowserProxyImpl, VoiceLanguageController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {MockTimer} from 'chrome-untrusted://webui-test/mock_timer.js';

import {createApp, emitEvent} from './common.js';
import {TestAudioBrowserProxy} from './test_audio_browser_proxy.js';
import {TestContentBrowserProxy} from './test_content_browser_proxy.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';
import {TestVisualBrowserProxy} from './test_visual_browser_proxy.js';

suite('ReadAloudHighlight', () => {
  let app: AppElement;
  let speechController: SpeechController;
  let selectionController: SelectionController;
  let visualBrowserProxy: TestVisualBrowserProxy;
  let contentController: ContentController;
  let nodeStore: NodeStore;
  const sentence1 = 'Only need the light when it\'s burning low.\n';
  const sentence2 = 'Only miss the sun when it starts to snow.\n';
  const sentenceSegment1 = 'Only know you love her when you let her go';
  const sentenceSegment2 = ', and you let her go.';

  function buildDOMTree() {
    app.$.container.replaceChildren();

    const node2 = document.createTextNode(sentence1);
    nodeStore.setDomNode(node2, 2);
    app.$.container.appendChild(node2);

    const node3 = document.createTextNode(sentence2);
    nodeStore.setDomNode(node3, 3);
    app.$.container.appendChild(node3);

    const node4 = document.createTextNode(sentenceSegment1);
    nodeStore.setDomNode(node4, 4);
    app.$.container.appendChild(node4);

    const node5 = document.createTextNode(sentenceSegment2);
    nodeStore.setDomNode(node5, 5);
    app.$.container.appendChild(node5);

    contentController.setState(ContentType.HAS_CONTENT);
  }

  function emitNextGranularity() {
    emitEvent(app, ToolbarEvent.NEXT_GRANULARITY);
  }

  function emitPreviousGranularity() {
    emitEvent(app, ToolbarEvent.PREVIOUS_GRANULARITY);
  }

  setup(async () => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    nodeStore = new NodeStore();
    NodeStore.setInstance(nodeStore);
    ContentBrowserProxyImpl.setInstance(new TestContentBrowserProxy());
    AudioBrowserProxyImpl.setInstance(new TestAudioBrowserProxy());
    visualBrowserProxy = new TestVisualBrowserProxy();
    VisualBrowserProxyImpl.setInstance(visualBrowserProxy);

    SpeechBrowserProxyImpl.setInstance(new TestSpeechBrowserProxy());
    selectionController = new SelectionController();
    SelectionController.setInstance(selectionController);
    speechController = new SpeechController();
    SpeechController.setInstance(speechController);
    VoiceLanguageController.setInstance(new VoiceLanguageController());
    contentController = new ContentController();
    ContentController.setInstance(contentController);
    // Ensure the ReadAloudModel is not shared between tests.
    setInstance(null);

    app = await createApp();
    buildDOMTree();
    selectionController.onSelectionChange(app.getSelection());
  });

  test('on speak first sentence highlights are correct', () => {
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);
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
      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      emitNextGranularity();
      emitNextGranularity();
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

  test('on speak next sentence highlights are correct', () => {
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);
    emitNextGranularity();
    const currentHighlight =
        app.$.container.querySelector('.current-read-highlight');
    const previousHighlight =
        app.$.container.querySelector('.previous-read-highlight');

    assertEquals(sentence2, currentHighlight!.textContent);
    assertEquals(sentence1, previousHighlight!.textContent);
  });

  // TODO: crbug.com/411198154- After refactoring is complete, ensure
  // there are proper unit tests for keeping the reading position. Until the
  // refactoring is complete, there isn't a great way to test this due to how
  // distillation is managed in tests.

  suite('on finish speaking', () => {
    let currentHighlight: HTMLElement|null;
    let previousHighlights: NodeListOf<Element>;

    setup(() => {
      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      emitNextGranularity();
      emitNextGranularity();
      emitNextGranularity();

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

    setup(() => {
      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      emitNextGranularity();
      emitPreviousGranularity();

      currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      previousHighlights =
          app.$.container.querySelectorAll('.previous-read-highlight');
    });

    test('previous sentence is now current and nothing marked previous', () => {
      assertEquals(sentence1, currentHighlight!.textContent);
      assertEquals(0, previousHighlights.length);
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

      assertEquals(sentence1, currentHighlight!.textContent);
    });

    test('going forward after going back shows correct highlights', () => {
      emitNextGranularity();
      currentHighlight =
          app.$.container.querySelector('.current-read-highlight');
      previousHighlights =
          app.$.container.querySelectorAll('.previous-read-highlight');

      assertEquals(sentence2, currentHighlight!.textContent);
      assertEquals(1, previousHighlights.length);
      assertEquals(sentence1, previousHighlights[0]!.textContent);

      emitNextGranularity();
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

  suite('on speaking from selection', () => {
    let currentHighlight: HTMLElement|null;
    let previousHighlights: NodeListOf<Element>;
    let mockTimer: MockTimer;
    let initialLineFocusEnabled: boolean;

    function selectAndPlay(
        anchorId: number, anchorOffset: number, focusId: number,
        focusOffset: number): void {
      mockTimer.install();
      buildDOMTree();

      const anchorNode = nodeStore.getDomNode(anchorId);
      const focusNode = nodeStore.getDomNode(focusId);
      if (anchorNode && focusNode) {
        const range = document.createRange();
        range.setStart(anchorNode, anchorOffset);
        range.setEnd(focusNode, focusOffset);
        const selection = app.getSelection();
        assertTrue(!!selection);
        selection.removeAllRanges();
        selection.addRange(range);
      }

      selectionController.updateSelection(app.getSelection(), app.$.container);
      selectionController.onSelectionChange(app.getSelection());
      speechController.onSelectionChange(
          selectionController.getCurrentSelectionStart());

      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      mockTimer.tick(playFromSelectionTimeout);
      mockTimer.uninstall();
    }

    setup(() => {
      mockTimer = new MockTimer();
      initialLineFocusEnabled = visualBrowserProxy.lineFocusEnabled;
    });

    teardown(() => {
      visualBrowserProxy.lineFocusEnabled = initialLineFocusEnabled;
    });

    suite('with line focus disabled', () => {
      setup(() => {
        visualBrowserProxy.lineFocusEnabled = false;
        selectAndPlay(3, 1, 3, 5);
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

      test('next granularity shows correct highlights', () => {
        emitNextGranularity();

        currentHighlight =
            app.$.container.querySelector('.current-read-highlight');
        previousHighlights =
            app.$.container.querySelectorAll('.previous-read-highlight');
        assertEquals(sentenceSegment1, currentHighlight!.textContent);
        assertEquals(2, previousHighlights!.length);
        assertEquals(sentence1, previousHighlights![0]!.textContent);
        assertEquals(sentence2, previousHighlights![1]!.textContent);
      });

      test('previous granularity shows correct highlights', () => {
        emitPreviousGranularity();

        currentHighlight =
            app.$.container.querySelector('.current-read-highlight');
        previousHighlights =
            app.$.container.querySelectorAll('.previous-read-highlight');
        assertEquals(sentence1, currentHighlight!.textContent);
        assertEquals(0, previousHighlights!.length);
      });
    });

    suite('with line focus enabled', () => {
      setup(() => {
        visualBrowserProxy.lineFocusEnabled = true;
        selectAndPlay(3, 1, 3, 5);
      });

      test('shows correct highlights', () => {
        currentHighlight =
            app.$.container.querySelector('.current-read-highlight');
        previousHighlights =
            app.$.container.querySelectorAll('.previous-read-highlight');

        assertEquals(sentence2, currentHighlight!.textContent);
        assertEquals(0, previousHighlights!.length);
      });

      test('next granularity shows correct highlights', () => {
        emitNextGranularity();

        currentHighlight =
            app.$.container.querySelector('.current-read-highlight');
        previousHighlights =
            app.$.container.querySelectorAll('.previous-read-highlight');
        assertEquals(sentenceSegment1, currentHighlight!.textContent);
        assertEquals(1, previousHighlights!.length);
        assertEquals(sentence2, previousHighlights![0]!.textContent);
      });

      test('previous granularity shows correct highlights', () => {
        emitPreviousGranularity();

        currentHighlight =
            app.$.container.querySelector('.current-read-highlight');
        previousHighlights =
            app.$.container.querySelectorAll('.previous-read-highlight');
        assertEquals(sentence1, currentHighlight!.textContent);
        assertEquals(0, previousHighlights!.length);
      });
    });
  });
});
