// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {BrowserProxy, ContentController, NodeStore, ReadAloudHighlighter, setInstance, SpeechBrowserProxyImpl, SpeechController, VoiceLanguageController, WordBoundaries} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createApp, createSpeechSynthesisVoice, setSimpleAxTreeWithText, setSimpleNodeStoreWithTextAndModel, stubAnimationFrame} from './common.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';
import {TestReadAloudModelBrowserProxy} from './test_read_aloud_browser_proxy.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';


suite('SpeechController', () => {
  let speech: TestSpeechBrowserProxy;
  let speechController: SpeechController;
  let wordBoundaries: WordBoundaries;
  let nodeStore: NodeStore;
  let highlighter: ReadAloudHighlighter;
  let voiceLanguageController: VoiceLanguageController;
  let readAloudModel: TestReadAloudModelBrowserProxy;
  let app: AppElement;
  let isSpeechActiveChanged: boolean;

  function onPlayPauseToggle(text: string) {
    const element = document.createElement('p');
    element.textContent = text;
    speechController.onPlayPauseToggle(element);
  }

  function setDomNodes(axNodeIds: number[]): Node[] {
    const nodes: Node[] = [];
    axNodeIds.forEach(id => {
      const element = document.createElement('p');
      nodeStore.setDomNode(element, id);
      nodes.push(element);
    });
    return nodes;
  }

  const speechListener = {
    onIsSpeechActiveChange() {
      isSpeechActiveChanged = true;
    },

    onIsAudioCurrentlyPlayingChange() {},

    onEngineStateChange() {},

    onPreviewVoicePlaying() {},

    onPlayingFromSelection() {

    },
  };

  setup(async () => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    chrome.readingMode.onConnected = () => {};
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    speech = new TestSpeechBrowserProxy();
    SpeechBrowserProxyImpl.setInstance(speech);
    readAloudModel = new TestReadAloudModelBrowserProxy();
    setInstance(readAloudModel);

    voiceLanguageController = new VoiceLanguageController();
    voiceLanguageController.setUserPreferredVoice(
        createSpeechSynthesisVoice({lang: 'en', name: 'Google Rumi'}));
    VoiceLanguageController.setInstance(voiceLanguageController);
    nodeStore = new NodeStore();
    NodeStore.setInstance(nodeStore);
    wordBoundaries = new WordBoundaries();
    WordBoundaries.setInstance(wordBoundaries);
    highlighter = new ReadAloudHighlighter();
    ReadAloudHighlighter.setInstance(highlighter);
    speechController = new SpeechController();
    SpeechController.setInstance(speechController);
    ContentController.setInstance(new ContentController());
    speechController.addListener(speechListener);
    speech.reset();

    app = await createApp();
  });

  suite('initializeSpeechTree', () => {
    stubAnimationFrame();
    let initializedNode: Node|undefined;
    function mockInit() {
      readAloudModel.init = (node) => {
        initializedNode = node.domNode();
        readAloudModel.setInitialized(true);
      };
    }

    test('with no node does nothing', () => {
      mockInit();
      speechController.initializeSpeechTree();

      assertFalse(!!initializedNode);
      assertFalse(speechController.isSpeechTreeInitialized());
    });

    test('when already initialized does nothing', () => {
      mockInit();
      const id1 = 10;
      const id2 = 12;
      const nodes = setDomNodes([id1, id2]);
      assertEquals(2, nodes.length);
      speechController.initializeSpeechTree(nodes[0]);
      speechController.initializeSpeechTree(nodes[1]);
      assertEquals(nodes[0], initializedNode);
    });

    test('initializes speech tree after content is set', () => {
      speechController.initializeSpeechTree();
      // Before any content has been set, init is not called.
      assertEquals(0, readAloudModel.getCallCount('init'));

      setSimpleAxTreeWithText('hello');
      assertEquals(1, readAloudModel.getCallCount('init'));

      // After the tree has already been initialized, init is not called again.
      speechController.initializeSpeechTree();
      assertEquals(1, readAloudModel.getCallCount('init'));
    });

    test('updateContent initializes speech', () => {
      setSimpleAxTreeWithText('hello');
      readAloudModel.setInitialized(false);
      assertEquals(1, readAloudModel.getCallCount('init'));

      app.updateContent();
      assertEquals(2, readAloudModel.getCallCount('init'));
    });

    test('updateContent resets the read aloud model', () => {
      // resetModel should not be called when the TS segmentation flag is
      // disabled.
      const expectedCalls =
          chrome.readingMode.isTsTextSegmentationEnabled ? 1 : 0;
      assertEquals(
          1 * expectedCalls, readAloudModel.getCallCount('resetModel'));

      setSimpleAxTreeWithText('hello');
      // setSimpleAxTreeWithText results in showLoading being called once and
      // updateContent being called twice.
      assertEquals(
          4 * expectedCalls, readAloudModel.getCallCount('resetModel'));

      setSimpleAxTreeWithText('hello, it\'s me');
      assertEquals(
          7 * expectedCalls, readAloudModel.getCallCount('resetModel'));
    });

    test('showLoading resets the read aloud model', () => {
      // resetModel should not be called when the TS segmentation flag is
      // disabled.
      const expectedCalls =
          chrome.readingMode.isTsTextSegmentationEnabled ? 1 : 0;
      assertEquals(
          1 * expectedCalls, readAloudModel.getCallCount('resetModel'));

      app.showLoading();
      assertEquals(
          2 * expectedCalls, readAloudModel.getCallCount('resetModel'));

      app.showLoading();
      assertEquals(
          3 * expectedCalls, readAloudModel.getCallCount('resetModel'));
    });
  });

  test('clearReadAloudState', () => {
    const text = 'And I am a massive deal';
    const node: Node = setSimpleNodeStoreWithTextAndModel(text, readAloudModel);
    wordBoundaries.updateBoundary(4);
    speechController.onHighlightGranularityChange(
        chrome.readingMode.sentenceHighlighting);
    speechController.onPlayPauseToggle(node as HTMLElement);
    assertTrue(speechController.isSpeechActive());
    assertTrue(wordBoundaries.hasBoundaries());
    assertTrue(highlighter.hasCurrentGranularity());

    speech.reset();
    isSpeechActiveChanged = false;

    speechController.clearReadAloudState();

    assertTrue(isSpeechActiveChanged);
    assertEquals(1, speech.getCallCount('cancel'));
    assertFalse(speechController.isSpeechActive());
    assertFalse(speechController.isPausedFromButton());
    assertFalse(speechController.isTemporaryPause());
    assertFalse(wordBoundaries.hasBoundaries());
    assertFalse(highlighter.hasCurrentGranularity());
  });

  test('onPlayPauseToggle propagates state', async () => {
    let propagatedSpeechActive = false;
    let propagatedAudioPlaying = false;
    chrome.readingMode.onIsSpeechActiveChanged = () => {
      propagatedSpeechActive = true;
    };
    chrome.readingMode.onIsAudioCurrentlyPlayingChanged = () => {
      propagatedAudioPlaying = true;
    };
    const text = 'You bring the corsets';
    readAloudModel.setInitialized(true);
    const node = setSimpleNodeStoreWithTextAndModel(text, readAloudModel);

    speechController.onPlayPauseToggle(node as HTMLElement);
    const spoken = await speech.whenCalled('speak');
    assertTrue(!!spoken.onstart);
    spoken.onstart(new SpeechSynthesisEvent('type', {utterance: spoken}));

    assertTrue(propagatedSpeechActive);
    assertTrue(propagatedAudioPlaying);
  });

  test(
      'onPlayPauseToggle resume with word boundaries cancels and re-speaks',
      () => {
        const textContent = 'And our fame and our faces';
        const node =
            setSimpleNodeStoreWithTextAndModel(textContent, readAloudModel);
        speechController.onPlayPauseToggle(node as HTMLElement);
        speechController.onPlayPauseToggle(node as HTMLElement);
        wordBoundaries.updateBoundary(10);
        speech.reset();

        speechController.onPlayPauseToggle(node as HTMLElement);

        assertEquals(1, speech.getCallCount('speak'));
        assertEquals(1, speech.getCallCount('cancel'));
      });

  test('onNextGranularityClick updates state', () => {
    setSimpleNodeStoreWithTextAndModel(
        'Know all about the glories', readAloudModel);
    wordBoundaries.updateBoundary(5);
    assertEquals(1, speech.getCallCount('cancel'));

    speechController.onNextGranularityClick();

    assertTrue(speechController.isSpeechBeingRepositioned());
    assertFalse(wordBoundaries.hasBoundaries());
    assertEquals(2, speech.getCallCount('cancel'));
  });

  test('onPreviousGranularityClick updates state', () => {
    setSimpleNodeStoreWithTextAndModel('And the disgraces', readAloudModel);
    wordBoundaries.updateBoundary(5);
    assertEquals(1, speech.getCallCount('cancel'));

    speechController.onPreviousGranularityClick();

    assertTrue(speechController.isSpeechBeingRepositioned());
    assertFalse(wordBoundaries.hasBoundaries());
    assertEquals(2, speech.getCallCount('cancel'));
  });

  test('onVoiceMenuClose resume speech only if it was active before', () => {
    const text = 'You must agree that baby';
    setSimpleNodeStoreWithTextAndModel(text, readAloudModel);
    speechController.onVoiceMenuOpen();

    speechController.onVoiceMenuClose();

    assertEquals(1, speech.getCallCount('cancel'));
    assertEquals(0, speech.getCallCount('pause'));
    assertEquals(0, speech.getCallCount('speak'));

    onPlayPauseToggle(text);
    speechController.onVoiceMenuOpen();
    onPlayPauseToggle(text);
    speech.reset();

    speechController.onVoiceMenuClose();

    assertEquals(1, speech.getCallCount('resume'));
    assertEquals(0, speech.getCallCount('cancel'));
    assertEquals(0, speech.getCallCount('speak'));
  });

  test('set previous reading position without saved state does nothing', () => {
    const text = 'But I took your hand';
    setSimpleNodeStoreWithTextAndModel(text, readAloudModel);
    wordBoundaries.updateBoundary(4);
    speechController.onHighlightGranularityChange(
        chrome.readingMode.sentenceHighlighting);
    onPlayPauseToggle(text);
    onPlayPauseToggle(text);
    assertTrue(speechController.hasSpeechBeenTriggered());
    assertTrue(wordBoundaries.hasBoundaries());
    assertTrue(highlighter.hasCurrentGranularity());

    speechController.clearReadAloudState();
    speechController.setPreviousReadingPositionIfExists();

    assertFalse(speechController.hasSpeechBeenTriggered());
    assertFalse(wordBoundaries.hasBoundaries());
    assertFalse(highlighter.hasCurrentGranularity());
  });

  test('set previous reading position restores saved state', () => {
    const text = 'And promised I\'d withstand';
    setSimpleNodeStoreWithTextAndModel(text, readAloudModel);
    wordBoundaries.updateBoundary(4);
    speechController.onHighlightGranularityChange(
        chrome.readingMode.sentenceHighlighting);
    onPlayPauseToggle(text);
    onPlayPauseToggle(text);
    assertTrue(speechController.hasSpeechBeenTriggered());
    assertTrue(wordBoundaries.hasBoundaries());
    assertTrue(highlighter.hasCurrentGranularity());

    speechController.saveReadAloudState();
    speechController.clearReadAloudState();
    speechController.setPreviousReadingPositionIfExists();

    assertTrue(speechController.hasSpeechBeenTriggered());
    assertTrue(wordBoundaries.hasBoundaries());
    assertTrue(highlighter.hasCurrentGranularity());
  });

  test('onTabMuteStateChange updates speech volume', async () => {
    const text = 'We\'ll bring the cinches';
    readAloudModel.setInitialized(true);
    setSimpleNodeStoreWithTextAndModel(text, readAloudModel);

    speechController.onTabMuteStateChange(true);
    onPlayPauseToggle(text);

    let spoken = await speech.whenCalled('speak');
    assertEquals(0, spoken.volume);

    speech.reset();
    speechController.onTabMuteStateChange(false);
    onPlayPauseToggle(text);

    spoken = await speech.whenCalled('speak');
    assertEquals(1, spoken.volume);
  });

});
