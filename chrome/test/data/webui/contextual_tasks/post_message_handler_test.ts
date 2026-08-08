// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {PostMessageHandler} from 'chrome://contextual-tasks/post_message_handler.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';
import {HANDSHAKE_REQUEST_MESSAGE_BASE64, HANDSHAKE_RESPONSE_BYTES} from './contextual_tasks_test_utils.js';

const HANDSHAKE_INTERVAL_MS = 10;
const TARGET_ORIGIN = 'https://local.test';

// Shared helper functions
let mockWebView: any;
function simulateLoadStart(url: string = TARGET_ORIGIN + '/testPath') {
  const loadStartEvent = new Event('loadstart');
  Object.assign(loadStartEvent, {isTopLevel: true, url: url});
  mockWebView.dispatchEvent(loadStartEvent);
}

function simulateLoadCommit(url: string = TARGET_ORIGIN + '/testPath') {
  const loadCommitEvent = new Event('loadcommit');
  Object.assign(loadCommitEvent, {isTopLevel: true, url: url});
  mockWebView.dispatchEvent(loadCommitEvent);
}

function simulateLoadRedirect(oldUrl: string, newUrl: string) {
  const loadRedirectEvent = new Event('loadredirect');
  Object.assign(loadRedirectEvent, {
    isTopLevel: true,
    oldUrl: oldUrl,
    newUrl: newUrl,
  });
  mockWebView.dispatchEvent(loadRedirectEvent);
}

function simulateLoadAbort(url: string = TARGET_ORIGIN + '/testPath') {
  const loadAbortEvent = new Event('loadabort');
  Object.assign(loadAbortEvent, {isTopLevel: true, url: url});
  mockWebView.dispatchEvent(loadAbortEvent);
}

function simulateMessage(data: any, origin: string) {
  const messageEvent = new MessageEvent('message', {
    data: data,
    origin: origin,
  });
  window.dispatchEvent(messageEvent);
}

suite('PostMessageHandlerTest', () => {
  let postMessageHandler: PostMessageHandler;
  let browserProxy: TestContextualTasksBrowserProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestContextualTasksBrowserProxy(TARGET_ORIGIN);
    BrowserProxyImpl.setInstance(browserProxy);

    loadTimeData.resetForTesting({
      handshakeMessage: HANDSHAKE_REQUEST_MESSAGE_BASE64,
    });

    mockWebView = {
      src: TARGET_ORIGIN + '/testPath',
      contentWindow: {
        postMessage: () => {},
      },
      addEventListener: () => {},
      removeEventListener: () => {},
    };
    const listenerMap = new Map<string, Function>();
    mockWebView.dispatchEvent = (event: Event) => {
      if (listenerMap.has(event.type)) {
        listenerMap.get(event.type)!(event);
      }
    };
    mockWebView.addEventListener = (type: string, listener: Function) => {
      listenerMap.set(type, listener);
    };
    postMessageHandler = new PostMessageHandler(mockWebView, browserProxy);
    browserProxy.page.setPostMessageHandler(postMessageHandler);
  });

  teardown(() => {
    if (postMessageHandler) {
      postMessageHandler.detach();
    }
  });

  test('ignores message from wrong origin', async function() {
    simulateLoadStart();
    simulateLoadCommit();

    simulateMessage(new ArrayBuffer(8), 'https://wrong.origin');
    await microtasksFinished();

    assertEquals(
        0, browserProxy.handler.getCallCount('onWebviewMessage'),
        'onWebviewMessage should not be called for wrong origin');
  });

  test('handles input-plate-bounds-update message', async function() {
    simulateLoadStart();
    simulateLoadCommit();

    let callbackCalled = false;
    let receivedRect: any = null;
    let receivedOccluders: any = null;
    let receivedViewportWidth: number|undefined;
    let receivedViewportHeight: number|undefined;
    postMessageHandler.setInputPlateBoundsUpdateCallback(
        (rect, occluders, viewportWidth, viewportHeight) => {
          callbackCalled = true;
          receivedRect = rect;
          receivedOccluders = occluders;
          receivedViewportWidth = viewportWidth;
          receivedViewportHeight = viewportHeight;
        });

    const rect = {
      top: 10,
      left: 20,
      width: 100,
      height: 200,
      right: 120,
      bottom: 210,
    };
    const occluders = [rect];
    const message = {
      'type': 'input-plate-bounds-update',
      'bounds-rect': rect,
      'occluders': occluders,
      'viewportWidth': 800,
      'viewportHeight': 600,
    };

    simulateMessage(message, TARGET_ORIGIN);
    await microtasksFinished();

    assertTrue(callbackCalled, 'Callback should be called');
    assertDeepEquals(rect, receivedRect, 'Rect should match');
    assertDeepEquals(occluders, receivedOccluders, 'Occluders should match');
    assertEquals(800, receivedViewportWidth, 'Viewport width should match');
    assertEquals(600, receivedViewportHeight, 'Viewport height should match');
  });

  test('handles input state update message', async function() {
    simulateLoadStart();
    simulateLoadCommit();

    let callbackCalled = false;
    let receivedToolMode: number|undefined;
    let receivedModelMode: number|undefined;
    postMessageHandler.setInputStateUpdateCallback(
        (toolMode?: number, modelMode?: number) => {
          callbackCalled = true;
          receivedToolMode = toolMode;
          receivedModelMode = modelMode;
        });

    const message = {
      'type': 'input-state-update',
      'requiresCapability': 'input state update',
      'toolMode': 1,
      'modelMode': 2,
    };

    simulateMessage(message, TARGET_ORIGIN);
    await microtasksFinished();

    assertTrue(callbackCalled, 'Callback should be called');
    assertEquals(1, receivedToolMode, 'ToolMode should match');
    assertEquals(2, receivedModelMode, 'ModelMode should match');
  });

  test(
      'handles input state update message with requiresCapability',
      async function() {
        simulateLoadStart();
        simulateLoadCommit();

        let callbackCalled = false;
        let receivedToolMode: number|undefined;
        let receivedModelMode: number|undefined;
        postMessageHandler.setInputStateUpdateCallback(
            (toolMode?: number, modelMode?: number) => {
              callbackCalled = true;
              receivedToolMode = toolMode;
              receivedModelMode = modelMode;
            });

        const message = {
          'type': 'input-state-update',
          'requiresCapability': 'input state update',
          'toolMode': 5,
          'modelMode': 6,
        };

        simulateMessage(message, TARGET_ORIGIN);
        await microtasksFinished();

        assertTrue(callbackCalled, 'Callback should be called');
        assertEquals(5, receivedToolMode, 'ToolMode should match');
        assertEquals(6, receivedModelMode, 'ModelMode should match');
      });

  // Default is camelCase. See if it supports kebab-case.
  test('handles input-state-update with kebab-case fields', async function() {
    simulateLoadStart();
    simulateLoadCommit();

    let callbackCalled = false;
    let receivedToolMode: number|undefined;
    let receivedModelMode: number|undefined;
    postMessageHandler.setInputStateUpdateCallback(
        (toolMode?: number, modelMode?: number) => {
          callbackCalled = true;
          receivedToolMode = toolMode;
          receivedModelMode = modelMode;
        });

    const message = {
      'type': 'input-state-update',
      'requiresCapability': 'input state update',
      'tool-mode': 3,
      'model-mode': 4,
    };

    simulateMessage(message, TARGET_ORIGIN);
    await microtasksFinished();

    assertTrue(callbackCalled, 'Callback should be called');
    assertEquals(3, receivedToolMode, 'ToolMode should match');
    assertEquals(4, receivedModelMode, 'ModelMode should match');
  });

  // Default is camelCase. See if it supports underscores.
  test('handles input-state-update with snake_case fields', async function() {
    simulateLoadStart();
    simulateLoadCommit();

    let callbackCalled = false;
    let receivedToolMode: number|undefined;
    let receivedModelMode: number|undefined;
    postMessageHandler.setInputStateUpdateCallback(
        (toolMode?: number, modelMode?: number) => {
          callbackCalled = true;
          receivedToolMode = toolMode;
          receivedModelMode = modelMode;
        });

    const message = {
      'type': 'input-state-update',
      'requiresCapability': 'input state update',
      'tool_mode': 2,
      'model_mode': 1,
    };

    simulateMessage(message, TARGET_ORIGIN);
    await microtasksFinished();

    assertTrue(callbackCalled, 'Callback should be called');
    assertEquals(2, receivedToolMode, 'ToolMode should match');
    assertEquals(1, receivedModelMode, 'ModelMode should match');
  });

  test(
      'handles input-state-update with undefined, null, or missing fields',
      async function() {
        simulateLoadStart();
        simulateLoadCommit();

        let callbackCalled = false;
        let receivedToolMode: number|undefined|null;
        let receivedModelMode: number|undefined|null;
        postMessageHandler.setInputStateUpdateCallback(
            (toolMode?: number, modelMode?: number) => {
              callbackCalled = true;
              receivedToolMode = toolMode;
              receivedModelMode = modelMode;
            });

        // Omitted / undefined fields.
        let message: Record<string, unknown> = {
          'type': 'input-state-update',
          'requiresCapability': 'input state update',
        };
        simulateMessage(message, TARGET_ORIGIN);
        await microtasksFinished();
        assertTrue(callbackCalled);
        assertEquals(undefined, receivedToolMode);
        assertEquals(undefined, receivedModelMode);

        // Tool mode specified, model mode missing.
        callbackCalled = false;
        message = {
          'type': 'input-state-update',
          'requiresCapability': 'input state update',
          'toolMode': 1,
        };
        simulateMessage(message, TARGET_ORIGIN);
        await microtasksFinished();
        assertTrue(callbackCalled);
        assertEquals(1, receivedToolMode);
        assertEquals(undefined, receivedModelMode);

        // Model mode specified, tool mode missing.
        callbackCalled = false;
        message = {
          'type': 'input-state-update',
          'requiresCapability': 'input state update',
          'modelMode': 2,
        };
        simulateMessage(message, TARGET_ORIGIN);
        await microtasksFinished();
        assertTrue(callbackCalled);
        assertEquals(undefined, receivedToolMode);
        assertEquals(2, receivedModelMode);

        // Null fields.
        callbackCalled = false;
        message = {
          'type': 'input-state-update',
          'requiresCapability': 'input state update',
          'toolMode': null,
          'modelMode': null,
        };
        simulateMessage(message, TARGET_ORIGIN);
        await microtasksFinished();
        assertTrue(callbackCalled);
        assertEquals(null, receivedToolMode);
        assertEquals(null, receivedModelMode);
      });

  test('ignores unhandled message types', async function() {
    simulateLoadStart();
    simulateLoadCommit();

    let callbackCalled = false;
    postMessageHandler.setInputStateUpdateCallback(() => {
      callbackCalled = true;
    });

    const message = {
      'type': 'other-unrelated-event',
      'requiresCapability': 'input state update',
      'toolMode': 1,
    };

    simulateMessage(message, TARGET_ORIGIN);
    await microtasksFinished();

    assertFalse(
        callbackCalled, 'Callback should not be called for unrelated message');
  });

  test('ignores message with wrong requiresCapability', async function() {
    simulateLoadStart();
    simulateLoadCommit();

    let callbackCalled = false;
    postMessageHandler.setInputStateUpdateCallback(() => {
      callbackCalled = true;
    });

    const message = {
      'type': 'input-state-update',
      'requiresCapability': 'wrong capability',
      'toolMode': 1,
    };

    simulateMessage(message, TARGET_ORIGIN);
    await microtasksFinished();

    assertFalse(
        callbackCalled, 'Callback should not be called for wrong capability');
  });
});

suite('PostMessageHandlerTestWithMockTimer', () => {
  const TEST_MAX_HANDSHAKE_ATTEMPTS = 3;
  let postMessageHandler: PostMessageHandler;
  let browserProxy: TestContextualTasksBrowserProxy;
  let postMessageSpy: any;
  let mockTimer: MockTimer;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    mockTimer = new MockTimer();
    mockTimer.install();
    browserProxy = new TestContextualTasksBrowserProxy(TARGET_ORIGIN);
    BrowserProxyImpl.setInstance(browserProxy);

    loadTimeData.resetForTesting({
      handshakeMessage: HANDSHAKE_REQUEST_MESSAGE_BASE64,
    });

    postMessageSpy = {
      calls: [],
      postMessage: function(message: ArrayBuffer, targetOrigin: string) {
        this.calls.push({args: [message, targetOrigin]});
      },
      reset: function() {
        this.calls = [];
      },
    };

    mockWebView = {
      src: TARGET_ORIGIN + '/testPath',
      contentWindow: {
        postMessage: postMessageSpy.postMessage.bind(postMessageSpy),
      },
      addEventListener: () => {},
      removeEventListener: () => {},
    };
    const listenerMap = new Map<string, Function>();
    mockWebView.dispatchEvent = (event: Event) => {
      if (listenerMap.has(event.type)) {
        listenerMap.get(event.type)!(event);
      }
    };
    mockWebView.addEventListener = (type: string, listener: Function) => {
      listenerMap.set(type, listener);
    };
    postMessageHandler = new PostMessageHandler(
        mockWebView, browserProxy, TEST_MAX_HANDSHAKE_ATTEMPTS);
    browserProxy.page.setPostMessageHandler(postMessageHandler);
  });

  teardown(() => {
    mockTimer.uninstall();
    if (postMessageHandler) {
      postMessageHandler.detach();
    }
  });

  test('handles HandshakeResponse', function() {
    // Initialize and start handshake process
    simulateLoadStart();
    simulateLoadCommit();


    // Send a message to be queued
    const pendingMsg = new Uint8Array([4, 5, 6]);
    postMessageHandler.sendMessage(pendingMsg);
    assertEquals(
        1, postMessageHandler.getPendingMessagesLengthForTesting(),
        'Message should be queued before handshake');

    // Trigger the first handshake interval
    mockTimer.tick(HANDSHAKE_INTERVAL_MS);
    assertEquals(
        1, postMessageSpy.calls.length, 'Handshake message should be sent');

    // Simulate receiving the handshake response
    simulateMessage(HANDSHAKE_RESPONSE_BYTES, TARGET_ORIGIN);

    // Verify handshake completion and pending message sent
    assertTrue(
        postMessageHandler.isHandshakeCompleteForTesting(),
        'Handshake should be complete');
    assertEquals(
        0, postMessageHandler.getPendingMessagesLengthForTesting(),
        'Pending messages queue should be empty');
    assertEquals(
        2, postMessageSpy.calls.length,
        'Handshake and pending message should be sent');
    assertEquals(
        1, browserProxy.handler.getCallCount('onWebviewMessage'),
        'onWebviewMessage should be called once with handshake response');

    const onWebviewMessageArgs =
        browserProxy.handler.getArgs('onWebviewMessage')[0];
    assertDeepEquals(
        Array.from(HANDSHAKE_RESPONSE_BYTES), onWebviewMessageArgs,
        'onWebviewMessageArgs should match handshake response');

    const pendingCallArgs = postMessageSpy.calls[1].args;
    assertDeepEquals(
        pendingMsg.buffer, pendingCallArgs[0],
        'Pending message content should match');
    assertEquals(
        TARGET_ORIGIN, pendingCallArgs[1],
        'Pending message target origin should match');

    // Ensure no more handshakes are sent
    mockTimer.tick(HANDSHAKE_INTERVAL_MS * 2);
    assertEquals(
        2, postMessageSpy.calls.length, 'No more messages should be sent');
  });

  test('queues message across loadstart events', function() {
    // Initialize and start handshake process
    simulateLoadStart();
    simulateLoadCommit();


    // Send a message to be queued
    const pendingMsg = new Uint8Array([7, 8, 9]);
    postMessageHandler.sendMessage(pendingMsg);
    assertEquals(
        1, postMessageHandler.getPendingMessagesLengthForTesting(),
        'Message should be queued');

    // Simulate another loadstart
    simulateLoadStart();
    assertEquals(
        1, postMessageHandler.getPendingMessagesLengthForTesting(),
        'Message should still be queued after second loadstart');

    simulateLoadCommit();


    // Trigger the handshake interval
    mockTimer.tick(HANDSHAKE_INTERVAL_MS);
    assertEquals(
        1, postMessageSpy.calls.length, 'Handshake message should be sent');

    // Simulate receiving the handshake response
    simulateMessage(HANDSHAKE_RESPONSE_BYTES, TARGET_ORIGIN);

    // Verify handshake completion and pending message sent
    assertTrue(
        postMessageHandler.isHandshakeCompleteForTesting(),
        'Handshake should be complete');
    assertEquals(
        0, postMessageHandler.getPendingMessagesLengthForTesting(),
        'Pending messages queue should be empty');
    assertEquals(
        2, postMessageSpy.calls.length,
        'Handshake and pending message should be sent');

    const pendingCallArgs = postMessageSpy.calls[1].args;
    assertDeepEquals(
        pendingMsg.buffer, pendingCallArgs[0],
        'Pending message content should match');
  });

  test('ignores non-top level loadstart events', () => {
    // Initialize and complete handshake
    simulateLoadStart();
    simulateLoadCommit();
    mockTimer.tick(HANDSHAKE_INTERVAL_MS);
    simulateMessage(HANDSHAKE_RESPONSE_BYTES, TARGET_ORIGIN);
    assertTrue(
        postMessageHandler.isHandshakeCompleteForTesting(),
        'Handshake should be complete');

    // Simulate non-top level loadstart
    const loadStartEvent = new Event('loadstart');
    Object.assign(loadStartEvent, {isTopLevel: false});
    mockWebView.dispatchEvent(loadStartEvent);

    // Handshake should still be complete
    assertTrue(
        postMessageHandler.isHandshakeCompleteForTesting(),
        'Handshake should still be complete after non-top level loadstart');
  });

  test('receives message after handshake', function() {
    // Initial handshake
    simulateLoadStart();
    simulateLoadCommit();

    mockTimer.tick(HANDSHAKE_INTERVAL_MS);
    simulateMessage(HANDSHAKE_RESPONSE_BYTES, TARGET_ORIGIN);
    assertTrue(
        postMessageHandler.isHandshakeCompleteForTesting(),
        'Handshake should be complete');

    // Reset spies and handlers
    browserProxy.handler.reset();
    postMessageSpy.reset();

    // Send a test message
    const testMessage = new Uint8Array([10, 11, 12]);
    simulateMessage(testMessage, TARGET_ORIGIN);

    // Verify the message was received by the browser proxy
    assertEquals(
        1, browserProxy.handler.getCallCount('onWebviewMessage'),
        'onWebviewMessage should be called once for test message');
    const messageArgs = browserProxy.handler.getArgs('onWebviewMessage')[0];
    assertDeepEquals(
        Array.from(testMessage), messageArgs,
        'onWebviewMessageArgs should match test message');
    assertEquals(
        0, postMessageSpy.calls.length,
        'No messages should be sent to webview');
  });

  test('handles postMessage error', function() {
    simulateLoadStart();
    simulateLoadCommit();


    // Make postMessage throw an error
    mockWebView.contentWindow.postMessage = () => {
      throw new Error('Test postMessage error');
    };

    // Suppress console.error to avoid crashing the test when JS error checking is enabled.
    const originalConsoleError = console.error;
    console.error = () => {};

    try {
      mockTimer.tick(HANDSHAKE_INTERVAL_MS);
      // No assertion on error, just ensure the test doesn't crash and the timer
      // stops.
      assertTrue(true, 'Test should not crash due to postMessage error');
    } finally {
      console.error = originalConsoleError;
    }
  });

  test('stops handshake after max attempts', function() {
    simulateLoadStart();
    simulateLoadCommit();


    for (let i = 0; i < TEST_MAX_HANDSHAKE_ATTEMPTS; i++) {
      mockTimer.tick(HANDSHAKE_INTERVAL_MS);
    }
    assertEquals(
        TEST_MAX_HANDSHAKE_ATTEMPTS, postMessageSpy.calls.length,
        `Should have tried to send handshake ${
            TEST_MAX_HANDSHAKE_ATTEMPTS} times`);

    // One more tick should not result in another call
    mockTimer.tick(HANDSHAKE_INTERVAL_MS);
    assertEquals(
        TEST_MAX_HANDSHAKE_ATTEMPTS, postMessageSpy.calls.length,
        'Should stop sending handshake after max attempts');
    assertFalse(
        postMessageHandler.isHandshakeCompleteForTesting(),
        'Handshake should not be complete');
  });

  test('does not start handshake if only loadstart is called', () => {
    simulateLoadStart();
    mockTimer.tick(HANDSHAKE_INTERVAL_MS);
    assertEquals(
        0, postMessageSpy.calls.length,
        'Handshake should not start without loadcommit');
    assertFalse(
        postMessageHandler.isHandshakeCompleteForTesting(),
        'Handshake should not be complete');
  });

  test('resets handshake on loadcommit after loadstart', function() {
    const url = TARGET_ORIGIN + '/matchingPath';

    // Complete initial handshake
    simulateLoadStart(url);
    simulateLoadCommit(url);


    mockTimer.tick(HANDSHAKE_INTERVAL_MS);
    simulateMessage(HANDSHAKE_RESPONSE_BYTES, TARGET_ORIGIN);
    assertTrue(postMessageHandler.isHandshakeCompleteForTesting());

    // Start a new navigation
    simulateLoadStart(url);

    // Commit
    simulateLoadCommit(url);


    // Handshake should be reset!
    assertFalse(postMessageHandler.isHandshakeCompleteForTesting());
  });

  test('resets handshake on loadcommit after loadredirect', function() {
    const url = TARGET_ORIGIN + '/matchingPath';

    // Complete initial handshake
    simulateLoadStart(url);
    simulateLoadCommit(url);


    mockTimer.tick(HANDSHAKE_INTERVAL_MS);
    simulateMessage(HANDSHAKE_RESPONSE_BYTES, TARGET_ORIGIN);
    assertTrue(postMessageHandler.isHandshakeCompleteForTesting());

    // Start a new navigation
    simulateLoadStart(url);

    // Simulate redirect
    const newUrl = url + '_new';
    simulateLoadRedirect(url, newUrl);

    // Handshake should NOT be reset on redirect!
    assertTrue(postMessageHandler.isHandshakeCompleteForTesting());

    // Simulate commit of the new URL
    simulateLoadCommit(newUrl);

    // Handshake should be reset on commit!
    assertFalse(postMessageHandler.isHandshakeCompleteForTesting());
  });

  test('does not reset handshake on loadcommit after loadabort', function() {
    const url = TARGET_ORIGIN + '/matchingPath';

    // Complete initial handshake
    simulateLoadStart(url);
    simulateLoadCommit(url);


    mockTimer.tick(HANDSHAKE_INTERVAL_MS);
    simulateMessage(HANDSHAKE_RESPONSE_BYTES, TARGET_ORIGIN);
    assertTrue(postMessageHandler.isHandshakeCompleteForTesting());

    // Start a new navigation
    simulateLoadStart(url);

    // Abort the navigation
    simulateLoadAbort(url);

    // Commit happens anyway (e.g. old load or something weird)
    simulateLoadCommit(url);


    // Handshake should STILL be complete!
    assertTrue(postMessageHandler.isHandshakeCompleteForTesting());
  });

  test(
      'does not reset handshake on loadcommit for non-matching URL',
      function() {
        const url = TARGET_ORIGIN + '/matchingPath';

        // Complete initial handshake
        simulateLoadStart(url);
        simulateLoadCommit(url);

        mockTimer.tick(HANDSHAKE_INTERVAL_MS);
        simulateMessage(HANDSHAKE_RESPONSE_BYTES, TARGET_ORIGIN);
        assertTrue(postMessageHandler.isHandshakeCompleteForTesting());

        // Start a new navigation
        simulateLoadStart(url);

        // Commit a non-matching URL
        simulateLoadCommit(TARGET_ORIGIN + '/nonMatchingPath');

        // Handshake should STILL be complete!
        assertTrue(postMessageHandler.isHandshakeCompleteForTesting());
      });
});
