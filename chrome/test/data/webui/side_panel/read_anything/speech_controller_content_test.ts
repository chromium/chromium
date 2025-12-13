// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {BrowserProxy, ContentController, NodeStore, playFromSelectionTimeout, ReadAloudHighlighter, ReadAloudNode, SelectionController, setInstance, SpeechBrowserProxyImpl, SpeechController, VoiceLanguageController, WordBoundaries} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {AppElement, Segment} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {MockTimer} from 'chrome-untrusted://webui-test/mock_timer.js';

import {createApp, createSpeechSynthesisVoice, setContent, stubAnimationFrame} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
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
  let selectionController: SelectionController;
  let readAloudModel: TestReadAloudModelBrowserProxy;
  let app: AppElement;
  let isSpeechActiveChanged: boolean;
  let onPlayingFromSelection: boolean;

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
      onPlayingFromSelection = true;
    },
  };

  setup(async () => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
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
    selectionController = new SelectionController();
    SelectionController.setInstance(selectionController);
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

      setContent('hello', readAloudModel);
      app.updateContent();
      assertEquals(1, readAloudModel.getCallCount('init'));

      // After the tree has already been initialized, init is not called again.
      speechController.initializeSpeechTree();
      assertEquals(1, readAloudModel.getCallCount('init'));
    });

    test('updateContent resets the read aloud model with ts flag', async () => {
      chrome.readingMode.isTsTextSegmentationEnabled = true;
      await createApp();
      assertEquals(1, readAloudModel.getCallCount('resetModel'));

      setContent('hello', readAloudModel);
      app.updateContent();
      assertEquals(2, readAloudModel.getCallCount('resetModel'));

      setContent('hello, it\'s me', readAloudModel);
      app.updateContent();
      assertEquals(3, readAloudModel.getCallCount('resetModel'));
    });

    test('updateContent does not reset the model without ts flag', async () => {
      chrome.readingMode.isTsTextSegmentationEnabled = false;
      await createApp();
      assertEquals(0, readAloudModel.getCallCount('resetModel'));

      setContent('hello', readAloudModel);
      app.updateContent();
      assertEquals(0, readAloudModel.getCallCount('resetModel'));

      setContent('hello, it\'s me', readAloudModel);
      app.updateContent();
      assertEquals(0, readAloudModel.getCallCount('resetModel'));
    });

    test('showLoading resets the read aloud model with ts flag', () => {
      chrome.readingMode.isTsTextSegmentationEnabled = true;
      app.showLoading();
      assertEquals(1, readAloudModel.getCallCount('resetModel'));
    });

    test('showLoading does not reset the model without ts flag', () => {
      chrome.readingMode.isTsTextSegmentationEnabled = false;
      app.showLoading();
      assertEquals(0, readAloudModel.getCallCount('resetModel'));
    });
  });

  test('clearReadAloudState', () => {
    const text = 'And I am a massive deal';
    const node: Node = setContent(text, readAloudModel);
    wordBoundaries.updateBoundary(4);
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.sentenceHighlighting);
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
    const node = setContent(text, readAloudModel);

    speechController.onPlayPauseToggle(node as HTMLElement);
    const spoken = await speech.whenCalled('speak');
    assertTrue(!!spoken.onstart);
    spoken.onstart(new SpeechSynthesisEvent('type', {utterance: spoken}));

    assertTrue(propagatedSpeechActive);
    assertTrue(propagatedAudioPlaying);
  });

  test('onPlayPauseToggle ignores hidden nodes', () => {
    const text = 'I\'m just tryna have some fun';
    const id = 2;
    readAloudModel.setInitialized(true);
    const parent = document.createElement('p');
    const node = document.createTextNode(text);
    parent.appendChild(node);
    nodeStore.setDomNode(parent, id);
    const segments: Segment[] =
        [{node: ReadAloudNode.create(parent)!, start: 0, length: text.length}];
    readAloudModel.setCurrentTextContent(text);
    nodeStore.hideImageNode(2);
    let calls = 0;
    readAloudModel.getCurrentTextSegments = () => {
      calls++;
      if (calls === 1) {
        return segments;
      } else {
        return [];
      }
    };

    speechController.onPlayPauseToggle(parent);

    assertEquals(0, speech.getCallCount('speak'));
  });

  test(
      'onPlayPauseToggle resume with word boundaries cancels and re-speaks',
      () => {
        const textContent = 'And our fame and our faces';
        const node = setContent(textContent, readAloudModel);
        speechController.onPlayPauseToggle(node as HTMLElement);
        speechController.onPlayPauseToggle(node as HTMLElement);
        wordBoundaries.updateBoundary(10);
        speech.reset();

        speechController.onPlayPauseToggle(node as HTMLElement);

        assertEquals(1, speech.getCallCount('speak'));
        assertEquals(1, speech.getCallCount('cancel'));
      });

  test('onPlayPauseToggle with selection reads from there', async () => {
    const id = 35;
    const p = document.createElement('p');
    const text1 = 'And our fame. ';
    const text2 = 'And our faces. ';
    const text3 = 'Know all about the glories.';
    const textNode = document.createTextNode(text1 + text2 + text3);
    p.appendChild(textNode);
    document.body.appendChild(p);
    chrome.readingMode.startNodeId = id;
    chrome.readingMode.startOffset = text1.length + text2.length + 3;
    chrome.readingMode.endNodeId = id;
    chrome.readingMode.endOffset = text1.length + text2.length + 8;
    nodeStore.setDomNode(textNode, id);
    const selection = document.getSelection();
    assertTrue(!!selection);
    const range = new Range();
    range.setStart(textNode, chrome.readingMode.startOffset);
    range.setEnd(textNode, chrome.readingMode.endOffset);
    selection.addRange(range);
    selectionController.onSelectionChange(selection);
    readAloudModel.setInitialized(true);
    readAloudModel.setCurrentTextContent(text3);
    const node = ReadAloudNode.create(textNode);
    assertTrue(!!node);
    readAloudModel.setCurrentTextSegments(
        [{node, start: 0, length: text1.length}]);
    let calls = 0;
    readAloudModel.moveSpeechForward = () => {
      readAloudModel.methodCalled('moveSpeechForward');
      calls++;
      if (calls === 1) {
        readAloudModel.setCurrentTextSegments(
            [{node, start: text1.length, length: text2.length}]);
      } else {
        readAloudModel.setCurrentTextSegments([
          {node, start: text1.length + text2.length, length: text3.length},
        ]);
      }
    };

    speechController.onPlayPauseToggle(p);
    const mockTimer = new MockTimer();
    mockTimer.install();
    mockTimer.tick(playFromSelectionTimeout);
    mockTimer.uninstall();
    await speech.whenCalled('speak');

    assertTrue(onPlayingFromSelection);
    assertEquals(2, readAloudModel.getCallCount('moveSpeechForward'));
  });

  test('onPlayPauseToggle with selection resets word boundaries', async () => {
    const id = 35;
    const p = document.createElement('p');
    const text1 = 'And the disgraces. ';
    const text2 = 'I\'m done. ';
    const text3 = 'Cause all this time.';
    const textNode = document.createTextNode(text1 + text2 + text3);
    p.appendChild(textNode);
    document.body.appendChild(p);
    // Start playing and then pause
    speechController.onPlayPauseToggle(p);
    wordBoundaries.updateBoundary(2);
    speechController.onPlayPauseToggle(p);
    assertTrue(wordBoundaries.hasBoundaries());
    // Now select text and play from there.
    chrome.readingMode.startNodeId = id;
    chrome.readingMode.startOffset = text1.length + text2.length + 3;
    chrome.readingMode.endNodeId = id;
    chrome.readingMode.endOffset = text1.length + text2.length + 8;
    nodeStore.setDomNode(textNode, id);
    const selection = document.getSelection();
    assertTrue(!!selection);
    const range = new Range();
    range.setStart(textNode, chrome.readingMode.startOffset);
    range.setEnd(textNode, chrome.readingMode.endOffset);
    selection.addRange(range);
    selectionController.onSelectionChange(selection);
    readAloudModel.setInitialized(true);
    readAloudModel.setCurrentTextContent(text3);
    const node = ReadAloudNode.create(textNode);
    assertTrue(!!node);
    readAloudModel.setCurrentTextSegments(
        [{node, start: 0, length: text1.length}]);
    let calls = 0;
    readAloudModel.moveSpeechForward = () => {
      readAloudModel.methodCalled('moveSpeechForward');
      calls++;
      if (calls === 1) {
        readAloudModel.setCurrentTextSegments(
            [{node, start: text1.length, length: text2.length}]);
      } else {
        readAloudModel.setCurrentTextSegments([
          {node, start: text1.length + text2.length, length: text3.length},
        ]);
      }
    };

    speechController.onPlayPauseToggle(p);
    const mockTimer = new MockTimer();
    mockTimer.install();
    mockTimer.tick(playFromSelectionTimeout);
    mockTimer.uninstall();
    await speech.whenCalled('speak');

    assertTrue(onPlayingFromSelection);
    assertFalse(wordBoundaries.hasBoundaries());
  });

  test('onNextGranularityClick updates state', () => {
    setContent('Know all about the glories', readAloudModel);
    wordBoundaries.updateBoundary(5);
    assertEquals(1, speech.getCallCount('cancel'));

    speechController.onNextGranularityClick();

    assertTrue(speechController.isSpeechBeingRepositioned());
    assertFalse(wordBoundaries.hasBoundaries());
    assertEquals(2, speech.getCallCount('cancel'));
  });

  test('onPreviousGranularityClick updates state', () => {
    setContent('And the disgraces', readAloudModel);
    wordBoundaries.updateBoundary(5);
    assertEquals(1, speech.getCallCount('cancel'));

    speechController.onPreviousGranularityClick();

    assertTrue(speechController.isSpeechBeingRepositioned());
    assertFalse(wordBoundaries.hasBoundaries());
    assertEquals(2, speech.getCallCount('cancel'));
  });

  test('onVoiceMenuClose resume speech only if it was active before', () => {
    const text = 'You must agree that baby';
    setContent(text, readAloudModel);
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
    setContent(text, readAloudModel);
    wordBoundaries.updateBoundary(4);
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.sentenceHighlighting);
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
    setContent(text, readAloudModel);
    wordBoundaries.updateBoundary(4);
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.sentenceHighlighting);
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
    setContent(text, readAloudModel);

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
