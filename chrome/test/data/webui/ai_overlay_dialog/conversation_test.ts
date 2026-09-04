// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageCallbackRouter} from 'chrome-untrusted://ai-overlay-dialog/ai_overlay_dialog.mojom-webui.js';
import {Conversation, State} from 'chrome-untrusted://ai-overlay-dialog/internal/conversation.js';
type ConversationMessage =|{
  type: 'inputTranscription',
  text: string,
}|{
  type: 'outputTranscription',
  text: string,
}|{type: 'clearTranscription'};
import type {AiOverlayToolsRemote} from 'chrome-untrusted://ai-overlay-dialog/tools.mojom-webui.js';
import {assertDeepEquals, assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';

class MockToolsRemote extends TestBrowserProxy {
  constructor() {
    super([
      'openUrl',
      'followLink',
      'performSearch',
      'switchTab',
      'closeCurrentTab',
      'goBack',
      'goForward',
      'reloadPage',
      'findAndHighlight',
      'scroll',
      'playVideo',
      'pauseVideo',
      'seekToTimestamp',
      'translatePage',
      'openGeminiPanel',
    ]);
  }
  openUrl() {
    return Promise.resolve();
  }
  followLink() {
    return Promise.resolve();
  }
  performSearch() {
    return Promise.resolve();
  }
  switchTab() {
    return Promise.resolve({title: '', url: {url: ''}, tabId: 0});
  }
  closeCurrentTab() {
    return Promise.resolve();
  }
  goBack() {
    return Promise.resolve();
  }
  goForward() {
    return Promise.resolve();
  }
  reloadPage() {
    return Promise.resolve();
  }
  findAndHighlight() {
    return Promise.resolve();
  }
  scroll() {
    return Promise.resolve();
  }
  playVideo() {
    return Promise.resolve();
  }
  pauseVideo() {
    return Promise.resolve();
  }
  seekToTimestamp() {
    return Promise.resolve();
  }
  translatePage() {
    return Promise.resolve();
  }
  openGeminiPanel() {
    return Promise.resolve('');
  }
}

suite('ConversationTest', () => {
  let sentMessages: ConversationMessage[];
  let stateChanges: Array<{newState: State, oldState: State}>;
  let audioResponses: string[];

  const mockUiDelegate = {
    sendToUI: (msg: ConversationMessage) => {
      sentMessages.push(msg);
    },
    onStateChange: (newState: State, oldState: State) => {
      stateChanges.push({newState, oldState});
    },
    onResponse: (audioData: string) => {
      audioResponses.push(audioData);
    },
  };

  const mockConfig = {
    api_config: {
      endpointUrl: 'ws://localhost/test',
      model: 'test-model',
      apiKey: 'test-key',
    },
    persona: {
      id: '1',
      name: 'Test Persona',
      nicknames: [],
      persona: 'Test',
      voice: 'Aoede',
    },
    system_instruction: 'test instruction',
  };

  const mockToolsRemote =
      new MockToolsRemote() as unknown as AiOverlayToolsRemote;
  const mockRouter = {
    didChangePage: {addListener: () => {}},
    updateCurrentPageContext: {addListener: () => {}},
  } as unknown as PageCallbackRouter;

  setup(() => {
    sentMessages = [];
    stateChanges = [];
    audioResponses = [];
  });

  test('InputTranscriptionAssignment', () => {
    const conversation = new Conversation(
        mockConfig, mockUiDelegate, mockToolsRemote, mockRouter);
    conversation.onConnectionChanged(true);

    // First chunk of user speech
    conversation.onTranscription('Hello', /*isInput=*/ true);
    assertEquals(1, sentMessages.length);
    assertDeepEquals(
        {type: 'inputTranscription', text: 'Hello'}, sentMessages[0]);

    // Second chunk of user speech (should assign '=' rather than append '+=')
    conversation.onTranscription('Hello world', /*isInput=*/ true);
    assertEquals(2, sentMessages.length);
    assertDeepEquals(
        {type: 'inputTranscription', text: 'Hello world'}, sentMessages[1]);
  });

  test('OutputTranscriptionDispatch', () => {
    const conversation = new Conversation(
        mockConfig, mockUiDelegate, mockToolsRemote, mockRouter);
    conversation.onConnectionChanged(true);

    conversation.onTranscription('Hi there', /*isInput=*/ false);
    assertEquals(1, sentMessages.length);
    assertDeepEquals(
        {type: 'outputTranscription', text: 'Hi there'}, sentMessages[0]);
  });

  test('OutputTranscriptionMultipleChunks', () => {
    const conversation = new Conversation(
        mockConfig, mockUiDelegate, mockToolsRemote, mockRouter);
    conversation.onConnectionChanged(true);

    // First chunk of output speech
    conversation.onTranscription('Hi', /*isInput=*/ false);
    assertEquals(1, sentMessages.length);
    assertDeepEquals(
        {type: 'outputTranscription', text: 'Hi'}, sentMessages[0]);

    // Second chunk of output speech (output accumulates across chunks unlike
    // input assignment)
    conversation.onTranscription(' there', /*isInput=*/ false);
    assertEquals(2, sentMessages.length);
    assertDeepEquals(
        {type: 'outputTranscription', text: 'Hi there'}, sentMessages[1]);
  });

  test('TurnCompleteDispatchesClearTranscription', () => {
    const conversation = new Conversation(
        mockConfig, mockUiDelegate, mockToolsRemote, mockRouter);
    conversation.onConnectionChanged(true);

    conversation.onTranscription('Test input', /*isInput=*/ true);
    assertEquals(1, sentMessages.length);

    conversation.onTurnComplete();
    assertEquals(2, sentMessages.length);
    assertDeepEquals({type: 'clearTranscription'}, sentMessages[1]);
  });

  test('InterruptDispatchesClearAndListeningState', () => {
    const conversation = new Conversation(
        mockConfig, mockUiDelegate, mockToolsRemote, mockRouter);
    conversation.onConnectionChanged(true);

    // Simulate active session state before interrupt
    (conversation as unknown as {state: State}).state = State.TALKING;
    conversation.onTranscription('Speaking...', /*isInput=*/ false);
    conversation.interrupt();

    assertDeepEquals(
        {type: 'clearTranscription'}, sentMessages[sentMessages.length - 1]);
    assertDeepEquals(
        {newState: State.LISTENING, oldState: State.TALKING},
        stateChanges[stateChanges.length - 1]);
  });
});
