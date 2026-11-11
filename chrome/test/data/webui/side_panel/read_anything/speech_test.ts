// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AppElement, ContentController, NodeStore, SpeechController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {ContentType, SelectionController, setInstance, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {emitEvent, getSpokenText, playFromSelectionWithMockTimer, setupAppTestEnvironment, setupBasicSpeech} from './common.js';
import type {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';

// Integration tests for Read Aloud playback over real DOM content. Uses the
// real ReadAloud model so text is segmented from the DOM. Unit tests for the
// model and SpeechController are in read_aloud_model_test.ts and
// speech_controller_test.ts.
suite('Speech', () => {
  let app: AppElement;
  let speech: TestSpeechBrowserProxy;
  let speechController: SpeechController;
  let contentController: ContentController;
  let nodeStore: NodeStore;

  const paragraph1: string[] = [
    'Something has changed within me, something is not the same.',
    'I\'m through with playing by the rules of someone else\'s game.',
    'Too late for second guessing; too late to go back to sleep.',
    'It\'s time to trust my instincts, close my eyes, and leap!',
  ];
  const paragraph2: string[] = [
    'It\'s time to try defying gravity.',
    'I think I\'ll try defying gravity.',
    'Kiss me goodbye, I\'m defying gravity.',
    'And you won\'t bring me down.',
  ];

  function buildDOMTree() {
    app.$.container.replaceChildren();

    const p1 = document.createElement('p');
    const node3 = document.createTextNode(paragraph1.join(' '));
    nodeStore.setDomNode(p1, 2);
    nodeStore.setDomNode(node3, 3);
    p1.appendChild(node3);
    app.$.container.appendChild(p1);

    const p2 = document.createElement('p');
    const node5 = document.createTextNode(paragraph2.join(' '));
    nodeStore.setDomNode(p2, 4);
    nodeStore.setDomNode(node5, 5);
    p2.appendChild(node5);
    app.$.container.appendChild(p2);

    nodeStore.setDomNode(app.$.container, 1);
    contentController.setState(ContentType.HAS_CONTENT);
  }

  setup(async () => {
    const result = await setupAppTestEnvironment({useRealReadAloudModel: true});
    app = result.app;
    nodeStore = result.nodeStore;
    speech = result.speech;
    speechController = result.speechController;
    contentController = result.contentController;
    setupBasicSpeech(speech);
    buildDOMTree();
    await microtasksFinished();
    // Text in hidden containers is skipped by Read Aloud.
    assertTrue(app.$.container.checkVisibility());
    speech.reset();
  });

  test('speaks all text by sentences', async () => {
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);
    await microtasksFinished();
    const sentences = [...paragraph1, ...paragraph2];
    sentences.forEach((sentence, i) => {
      assertEquals(sentence, getSpokenText(speech, i));
      speech.getArgs('speak')[i].onend();
    });
    assertEquals(sentences.length, speech.getCallCount('speak'));
  });

  test('play after speech finishes plays again from the top', () => {
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);
    const spoken1 = speech.getArgs('speak')[0];
    spoken1.onend();
    const spoken2 = speech.getArgs('speak')[1];
    spoken2.onend();
    const spoken3 = speech.getArgs('speak')[2];
    spoken3.onend();
    const spoken4 = speech.getArgs('speak')[3];
    spoken4.onend();
    const spoken5 = speech.getArgs('speak')[4];
    spoken5.onend();
    const spoken6 = speech.getArgs('speak')[5];
    spoken6.onend();
    const spoken7 = speech.getArgs('speak')[6];
    spoken7.onend();
    const spoken8 = speech.getArgs('speak')[7];
    spoken8.onend();

    emitEvent(app, ToolbarEvent.PLAY_PAUSE);

    assertEquals(9, speech.getCallCount('speak'));
    const spoken9 = speech.getArgs('speak')[0];
    assertEquals(paragraph1[0], spoken9.text.trim());
  });

  suite('with text selected', () => {
    const fragment1 = ' This is a sentence';
    const fragment2 = ' that ends in the next node. ';
    const fragment3 =
        'And a following sentence in the same node to be selected.';

    function buildSplitNodeDOM() {
      app.$.container.replaceChildren();

      const p = document.createElement('p');
      nodeStore.setDomNode(p, 2);

      const a = document.createElement('a');
      nodeStore.setDomNode(a, 3);

      const node4 = document.createTextNode(fragment1);
      nodeStore.setDomNode(node4, 4);
      a.appendChild(node4);
      p.appendChild(a);

      const node5 = document.createTextNode(fragment2 + fragment3);
      nodeStore.setDomNode(node5, 5);
      p.appendChild(node5);

      app.$.container.appendChild(p);
      nodeStore.setDomNode(app.$.container, 1);
      contentController.setState(ContentType.HAS_CONTENT);
    }

    function selectAndPlay(
        anchorId: number, anchorOffset: number, focusId: number,
        focusOffset: number, isBackward: boolean = false,
        isSplitNode: boolean = false): void {
      select(
          anchorId, anchorOffset, focusId, focusOffset, isBackward,
          isSplitNode);
      playFromSelectionWithMockTimer(app);
    }

    function select(
        anchorId: number, anchorOffset: number, focusId: number,
        focusOffset: number, isBackward: boolean = false,
        isSplitNode: boolean = false): void {
      if (isSplitNode) {
        buildSplitNodeDOM();
      } else {
        buildDOMTree();
      }
      setInstance(null);
      speechController.resetForNewContent();
      speechController.initializeSpeechTree(app.$.container);

      const anchorNode = nodeStore.getDomNode(anchorId);
      const focusNode = nodeStore.getDomNode(focusId);
      if (anchorNode && focusNode) {
        const range = document.createRange();
        if (isBackward) {
          range.setStart(focusNode, focusOffset);
          range.setEnd(anchorNode, anchorOffset);
        } else {
          range.setStart(anchorNode, anchorOffset);
          range.setEnd(focusNode, focusOffset);
        }
        const selection = app.getSelection();
        assertTrue(!!selection);
        selection.removeAllRanges();
        selection.addRange(range);
      }

      const selectionController = SelectionController.getInstance();
      selectionController.onSelectionChange(app.getSelection());
      speechController.onSelectionChange(
          selectionController.getCurrentSelectionStart());
    }

    setup(() => microtasksFinished());

    test('first play starts from selected node', () => {
      selectAndPlay(5, 0, 5, 7);
      assertEquals(paragraph2[0], getSpokenText(speech));
    });

    test('selection is cleared after play', () => {
      selectAndPlay(5, 0, 5, 10);
      const selection = app.getSelection();
      assertTrue(!!selection);
      assertEquals('None', selection.type);
    });

    test('in middle of node, play from beginning of node', async () => {
      selectAndPlay(5, 10, 5, 20);
      await microtasksFinished();
      assertEquals(paragraph2[0], getSpokenText(speech));
    });

    test('when selection crosses nodes, play from earlier node', () => {
      selectAndPlay(3, 10, 5, 10);
      assertEquals(paragraph1[0], getSpokenText(speech));
    });

    test('when selection is backward, play from earlier node', () => {
      selectAndPlay(5, 10, 3, 10, /*isBackward=*/ true);
      assertEquals(paragraph1[0], getSpokenText(speech));
    });

    test('after speech started, cancels and plays from selection', () => {
      select(5, 0, 5, 10);
      const domNode = nodeStore.getDomNode(1);
      speechController.initializeSpeechTree(domNode);
      speechController.setHasSpeechBeenTriggered(true);
      speech.reset();

      playFromSelectionWithMockTimer(app);

      assertEquals(1, speech.getCallCount('cancel'));
      assertEquals(paragraph2[0], getSpokenText(speech));
    });

    test('after two selections, plays from most recent selection', () => {
      select(5, 0, 5, 10);
      let domNode = nodeStore.getDomNode(1);
      speechController.initializeSpeechTree(domNode);
      speechController.setHasSpeechBeenTriggered(true);
      speech.reset();

      playFromSelectionWithMockTimer(app);

      assertEquals(1, speech.getCallCount('cancel'));
      assertEquals(paragraph2[0], getSpokenText(speech));

      select(3, 10, 5, 10);
      domNode = nodeStore.getDomNode(1);
      speechController.clearReadAloudState();
      speechController.initializeSpeechTree(domNode);
      speechController.setHasSpeechBeenTriggered(true);
      speech.reset();

      playFromSelectionWithMockTimer(app);

      assertEquals(1, speech.getCallCount('cancel'));
      assertEquals(paragraph1[0], getSpokenText(speech));
    });

    test('play from selection when node split across sentences', () => {
      selectAndPlay(
          5, fragment2.length + 1, 5, fragment2.length + fragment3.length,
          /*isBackward=*/ false, /*isSplitNode=*/ true);

      // We shouldn't speak fragment2 even though it's in the same node
      // because the selection only covers fragment 3.
      assertEquals(fragment3, getSpokenText(speech));
    });
  });

  test('next granularity plays from there', () => {
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);
    speech.reset();
    emitEvent(app, ToolbarEvent.NEXT_GRANULARITY);
    assertEquals(paragraph1[1], getSpokenText(speech));
  });

  test('previous granularity plays from there', () => {
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);
    emitEvent(app, ToolbarEvent.NEXT_GRANULARITY);
    speech.reset();

    emitEvent(app, ToolbarEvent.PREVIOUS_GRANULARITY);

    assertEquals(paragraph1[0], getSpokenText(speech));
  });
});
