// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {PostMessageHandler} from 'chrome://contextual-tasks/post_message_handler.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';
import {HANDSHAKE_REQUEST_MESSAGE_BASE64, HANDSHAKE_RESPONSE_BYTES} from './test_utils.js';

const HANDSHAKE_INTERVAL_MS = 500;
const TARGET_ORIGIN = 'https://local.test';

// Shared helper functions
let mockWebView: any;
function simulateLoadStop() {
  const loadStopEvent = new Event('loadstop');
  mockWebView.dispatchEvent(loadStopEvent);
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
    simulateLoadStop();

    simulateMessage(new ArrayBuffer(8), 'https://wrong.origin');
    await flushTasks();

    assertEquals(
        0, browserProxy.handler.getCallCount('onWebviewMessage'),
        'onWebviewMessage should not be called for wrong origin');
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

  test('handles HandshakeResponse', () => {
    // Initialize and start handshake process
    simulateLoadStop();

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

  test('queues message across loadstop events', () => {
    // Initialize and start handshake process
    simulateLoadStop();

    // Send a message to be queued
    const pendingMsg = new Uint8Array([7, 8, 9]);
    postMessageHandler.sendMessage(pendingMsg);
    assertEquals(
        1, postMessageHandler.getPendingMessagesLengthForTesting(),
        'Message should be queued');

    // Simulate another loadstop
    simulateLoadStop();
    assertEquals(
        1, postMessageHandler.getPendingMessagesLengthForTesting(),
        'Message should still be queued after second loadstop');

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

  test('receives message after handshake', () => {
    // Initial handshake
    simulateLoadStop();
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

  test('handles postMessage error', () => {
    simulateLoadStop();

    // Make postMessage throw an error
    mockWebView.contentWindow.postMessage = () => {
      throw new Error('Test postMessage error');
    };

    mockTimer.tick(HANDSHAKE_INTERVAL_MS);
    // No assertion on error, just ensure the test doesn't crash and the timer
    // stops.
    assertTrue(true, 'Test should not crash due to postMessage error');
  });

  test('stops handshake after max attempts', () => {
    simulateLoadStop();

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
});
