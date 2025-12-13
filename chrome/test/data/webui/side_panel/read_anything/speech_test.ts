// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {ContentController, NodeStore, playFromSelectionTimeout, SelectionController, setInstance, SpeechBrowserProxyImpl, SpeechController, ToolbarEvent, VoiceLanguageController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {MockTimer} from 'chrome-untrusted://webui-test/mock_timer.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {createAndSetVoices, emitEvent, mockMetrics, setupBasicSpeech, stubAnimationFrame} from './common.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';

suite('Speech', () => {
  let app: AppElement;
  let speech: TestSpeechBrowserProxy;
  let voiceLanguageController: VoiceLanguageController;
  let speechController: SpeechController;

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

  const leafIds = [3, 5];
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
        role: 'paragraph',
        htmlTag: 'p',
        childIds: [3],
      },
      {
        id: 3,
        role: 'staticText',
        name: paragraph1.join(' '),
      },
      {
        id: 4,
        role: 'paragraph',
        htmlTag: 'p',
        childIds: [5],
      },
      {
        id: 5,
        role: 'staticText',
        name: paragraph2.join(' '),
      },
    ],
  };

  function getSpokenText(): string {
    assertEquals(1, speech.getCallCount('speak'));
    return speech.getArgs('speak')[0].text.trim();
  }

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Do not call the real `onConnected()`. As defined in
    // ReadAnythingAppController, onConnected creates mojo pipes to connect to
    // the rest of the Read Anything feature, which we are not testing here.
    chrome.readingMode.onConnected = () => {};
    if (chrome.readingMode.isTsTextSegmentationEnabled) {
      stubAnimationFrame();
    }
    // Ensure the ReadAloudModel is not shared between tests.
    setInstance(null);
    speech = new TestSpeechBrowserProxy();
    SpeechBrowserProxyImpl.setInstance(speech);
    chrome.readingMode.shouldShowUi = () => true;
    chrome.readingMode.showLoading = () => {};
    chrome.readingMode.restoreSettingsFromPrefs = () => {};
    chrome.readingMode.languageChanged = () => {};
    chrome.readingMode.onTtsEngineInstalled = () => {};
    mockMetrics();
    voiceLanguageController = new VoiceLanguageController();
    VoiceLanguageController.setInstance(voiceLanguageController);
    speechController = new SpeechController();
    SpeechController.setInstance(speechController);
    ContentController.setInstance(new ContentController());

    app = document.createElement('read-anything-app');
    document.body.appendChild(app);
    setupBasicSpeech(speech);
    chrome.readingMode.setContentForTesting(axTree, leafIds);
    speech.reset();
  });

  test('speaks all text by sentences', () => {
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);
    assertEquals(1, speech.getCallCount('speak'));
    const spoken1 = speech.getArgs('speak')[0];
    assertEquals(paragraph1[0], spoken1.text.trim());

    spoken1.onend();
    assertEquals(2, speech.getCallCount('speak'));
    const spoken2 = speech.getArgs('speak')[1];
    assertEquals(paragraph1[1], spoken2.text.trim());

    spoken2.onend();
    assertEquals(3, speech.getCallCount('speak'));
    const spoken3 = speech.getArgs('speak')[2];
    assertEquals(paragraph1[2], spoken3.text.trim());

    spoken3.onend();
    assertEquals(4, speech.getCallCount('speak'));
    const spoken4 = speech.getArgs('speak')[3];
    assertEquals(paragraph1[3], spoken4.text.trim());

    spoken4.onend();
    assertEquals(5, speech.getCallCount('speak'));
    const spoken5 = speech.getArgs('speak')[4];
    assertEquals(paragraph2[0], spoken5.text.trim());

    spoken5.onend();
    assertEquals(6, speech.getCallCount('speak'));
    const spoken6 = speech.getArgs('speak')[5];
    assertEquals(paragraph2[1], spoken6.text.trim());

    spoken6.onend();
    assertEquals(7, speech.getCallCount('speak'));
    const spoken7 = speech.getArgs('speak')[6];
    assertEquals(paragraph2[2], spoken7.text.trim());

    spoken7.onend();
    assertEquals(8, speech.getCallCount('speak'));
    const spoken8 = speech.getArgs('speak')[7];
    assertEquals(paragraph2[3], spoken8.text.trim());

    spoken8.onend();
    assertEquals(8, speech.getCallCount('speak'));
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

  test('uses set language', () => {
    const expectedLang = 'fr';
    chrome.readingMode.setLanguageForTesting(expectedLang);

    emitEvent(app, ToolbarEvent.PLAY_PAUSE);

    assertEquals(1, speech.getCallCount('speak'));
    assertEquals(expectedLang, speech.getArgs('speak')[0].lang);
  });

  suite('with text selected', () => {
    let mockTimer: MockTimer;

    function selectAndPlay(
        baseTree: any, anchorId: number, anchorOffset: number, focusId: number,
        focusOffset: number, isBackward: boolean = false): void {
      select(
          baseTree, anchorId, anchorOffset, focusId, focusOffset, isBackward);
      playFromSelection();
    }

    function select(
        baseTree: any, anchorId: number, anchorOffset: number, focusId: number,
        focusOffset: number, isBackward: boolean = false): void {
      mockTimer.install();
      stubAnimationFrame();
      const selectedTree = Object.assign(
          {
            selection: {
              anchor_object_id: anchorId,
              focus_object_id: focusId,
              anchor_offset: anchorOffset,
              focus_offset: focusOffset,
              is_backward: isBackward,
            },
          },
          baseTree);
      chrome.readingMode.setContentForTesting(selectedTree, leafIds);
      const selectionController = SelectionController.getInstance();
      selectionController.updateSelection(app.getSelection(), app.$.container);
      selectionController.onSelectionChange(app.getSelection());
    }

    function playFromSelection() {
      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      mockTimer.tick(playFromSelectionTimeout);
      mockTimer.uninstall();
    }

    setup(() => {
      mockTimer = new MockTimer();
      return microtasksFinished();
    });

    test('first play starts from selected node', () => {
      selectAndPlay(axTree, 5, 0, 5, 7);
      assertEquals(paragraph2[0], getSpokenText());
    });

    test('selection is cleared after play', () => {
      selectAndPlay(axTree, 5, 0, 5, 10);
      const selection = app.getSelection();
      assertTrue(!!selection);
      assertEquals('None', selection.type);
    });

    test('in middle of node, play from beginning of node', async () => {
      selectAndPlay(axTree, 5, 10, 5, 20);
      await microtasksFinished();
      assertEquals(paragraph2[0], getSpokenText());
    });

    test('when selection crosses nodes, play from earlier node', () => {
      selectAndPlay(axTree, 3, 10, 5, 10);
      assertEquals(paragraph1[0], getSpokenText());
    });

    test('when selection is backward, play from earlier node', () => {
      selectAndPlay(axTree, 5, 10, 3, 10, /*isBackward=*/ true);
      assertEquals(paragraph1[0], getSpokenText());
    });

    test('after speech started, cancels and plays from selection', () => {
      select(axTree, 5, 0, 5, 10);
      const domNode = NodeStore.getInstance().getDomNode(1);
      speechController.initializeSpeechTree(domNode);
      speechController.setHasSpeechBeenTriggered(true);
      speech.reset();

      playFromSelection();

      assertEquals(1, speech.getCallCount('cancel'));
      assertEquals(paragraph2[0], getSpokenText());
    });

    test('after two selections, plays from most recent selection', () => {
      select(axTree, 5, 0, 5, 10);
      let domNode = NodeStore.getInstance().getDomNode(1);
      speechController.initializeSpeechTree(domNode);
      speechController.setHasSpeechBeenTriggered(true);
      speech.reset();

      playFromSelection();

      assertEquals(1, speech.getCallCount('cancel'));
      assertEquals(paragraph2[0], getSpokenText());

      select(axTree, 3, 10, 5, 10);
      domNode = NodeStore.getInstance().getDomNode(1);
      speechController.initializeSpeechTree(domNode);
      speechController.setHasSpeechBeenTriggered(true);
      speech.reset();

      playFromSelection();

      assertEquals(1, speech.getCallCount('cancel'));
      assertEquals(paragraph1[0], getSpokenText());
    });

    test('play from selection when node split across sentences', () => {
      const fragment1 = ' This is a sentence';
      const fragment2 = ' that ends in the next node. ';
      const fragment3 =
          'And a following sentence in the same node to be selected.';
      const splitNodeTree = {
        rootId: 1,
        nodes: [
          {
            id: 1,
            role: 'rootWebArea',
            htmlTag: '#document',
            childIds: [2],
          },
          {
            id: 2,
            role: 'paragraph',
            htmlTag: 'p',
            childIds: [3, 5],
          },
          {
            id: 3,
            role: 'link',
            htmlTag: 'a',
            url: 'http://www.google.com',
            childIds: [4],
          },
          {
            id: 4,
            role: 'staticText',
            name: fragment1,
          },
          {
            id: 5,
            role: 'staticText',
            name: fragment2 + fragment3,
          },
        ],
      };
      selectAndPlay(
          splitNodeTree, 5, fragment2.length + 1, 5,
          fragment2.length + fragment3.length);

      // We shouldn't speak fragment2 even though it's in the same node
      // because the selection only covers fragment 3.
      assertEquals(fragment3, getSpokenText());
    });
  });

  test('next granularity plays from there', async () => {
    await microtasksFinished();
    emitEvent(app, ToolbarEvent.NEXT_GRANULARITY);
    assertEquals(paragraph1[1], getSpokenText());
  });

  test('previous granularity plays from there', () => {
    chrome.readingMode.initAxPositionWithNode(2);
    emitEvent(app, ToolbarEvent.PLAY_PAUSE);
    speech.reset();

    emitEvent(app, ToolbarEvent.PREVIOUS_GRANULARITY);

    assertEquals(paragraph1[0], getSpokenText());
  });

  suite('while playing', () => {
    setup(() => {
      const domNode = NodeStore.getInstance().getDomNode(1);
      speechController.initializeSpeechTree(domNode);
      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
    });

    test('voice change cancels and restarts speech', () => {
      createAndSetVoices(speech, [
        {lang: 'en', name: 'Google Sheldon'},
        {lang: 'en', name: 'Google Mary'},
      ]);
      speech.reset();

      emitEvent(
          app, ToolbarEvent.VOICE,
          {detail: {selectedVoice: speech.getVoices()[1]}});

      assertEquals(2, speech.getCallCount('cancel'));
      assertEquals(1, speech.getCallCount('speak'));
      assertEquals(0, speech.getCallCount('pause'));
    });

    test('rate change cancels and restarts speech', () => {
      speech.reset();

      emitEvent(app, ToolbarEvent.RATE);

      assertEquals(2, speech.getCallCount('cancel'));
      assertEquals(1, speech.getCallCount('speak'));
      assertEquals(0, speech.getCallCount('pause'));
    });

    test('before speech engine is loaded is not playable', async () => {
      await microtasksFinished();
      assertFalse(app.$.toolbar.isReadAloudPlayable);
    });

    test('after speech engine is loaded is playable', async () => {
      assertEquals(1, speech.getCallCount('speak'));

      speech.getArgs('speak')[0].onstart();
      await microtasksFinished();

      assertTrue(app.$.toolbar.isReadAloudPlayable);
    });
  });
});
