// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ExtensionBrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {ExtensionPostMessageHandler, resetExtensionPostMessagingForTesting} from 'chrome://contextual-tasks/contextual_tasks_extension/post_message_handler.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestExtensionBrowserProxy} from './test_contextual_tasks_browser_proxy.js';

suite('ExtensionPostMessageHandlerTest', () => {
  let handler: ExtensionPostMessageHandler;
  let testProxy: TestExtensionBrowserProxy;
  let postedMessages: Array<{message: Uint8Array, targetOrigin: string}>;
  let originalPostMessage: typeof window.parent.postMessage;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    resetExtensionPostMessagingForTesting();

    postedMessages = [];
    originalPostMessage = window.parent.postMessage;
    window.parent.postMessage =
        (message: unknown,
         targetOriginOrOptions?: string|WindowPostMessageOptions) => {
          const targetOrigin = typeof targetOriginOrOptions === 'string' ?
              targetOriginOrOptions :
              (targetOriginOrOptions?.targetOrigin || '*');
          postedMessages.push({message: message as Uint8Array, targetOrigin});
        };

    testProxy = new TestExtensionBrowserProxy();
    ExtensionBrowserProxyImpl.setInstance(testProxy);

    handler = new ExtensionPostMessageHandler(testProxy);
    await testProxy.handler.whenCalled('getHandshakeMessage');
    await microtasksFinished();
  });

  teardown(() => {
    handler.destroy();
    resetExtensionPostMessagingForTesting();
    window.parent.postMessage = originalPostMessage;
  });

  test('Initializes handshake and sends ping to parent', async () => {
    while (postedMessages.length === 0) {
      await new Promise(resolve => setTimeout(resolve, 10));
    }
    assertTrue(postedMessages.length > 0);
    const firstMessage = postedMessages[0]!;
    assertTrue(firstMessage.message instanceof Uint8Array);
    assertDeepEquals([1, 2, 3], Array.from(firstMessage.message));
  });

  test('Completes handshake on onHandshakeComplete mojo event', async () => {
    assertFalse(handler.handshakeCompleted);
    assertEquals(false, window.stateForTesting?.handshakeCompleted);

    testProxy.callbackRouterRemote.onHandshakeComplete();
    await microtasksFinished();

    assertTrue(handler.handshakeCompleted);
    assertEquals(true, window.stateForTesting?.handshakeCompleted);
  });

  test('Forwards valid webview messages to browser handler', async () => {
    const testData = new Uint8Array([10, 20, 30]);
    const messageEvent = new MessageEvent('message', {
      data: testData.buffer,
      origin: 'https://www.google.com',
      source: window.parent,
    });

    window.dispatchEvent(messageEvent);
    await microtasksFinished();

    const receivedMessage =
        await testProxy.handler.whenCalled('onWebviewMessage');
    assertDeepEquals([10, 20, 30], receivedMessage);
    assertEquals('https://www.google.com', handler.targetOrigin);
    assertEquals(
        'https://www.google.com', window.stateForTesting?.targetOrigin);
  });

  test('Ignores messages from untrusted origins', async () => {
    const testData = new Uint8Array([40, 50]);
    const messageEvent = new MessageEvent('message', {
      data: testData.buffer,
      origin: 'https://untrusted-site.com',
      source: window.parent,
    });

    window.dispatchEvent(messageEvent);
    await microtasksFinished();

    assertEquals(0, testProxy.handler.getCallCount('onWebviewMessage'));
  });

  test('Ignores messages not from parent frame', async () => {
    const testData = new Uint8Array([60, 70]);
    const channel = new MessageChannel();
    const messageEvent = new MessageEvent('message', {
      data: testData.buffer,
      origin: 'https://www.google.com',
      source: channel.port1,
    });

    window.dispatchEvent(messageEvent);
    await microtasksFinished();

    assertEquals(0, testProxy.handler.getCallCount('onWebviewMessage'));
  });

  test('Posts AIM messages to parent when target origin is set', async () => {
    // Complete handshake first to stop periodic handshake pings.
    testProxy.callbackRouterRemote.onHandshakeComplete();
    await microtasksFinished();

    // Establish target origin through incoming message.
    const handshakeEvent = new MessageEvent('message', {
      data: new Uint8Array([1]).buffer,
      origin: 'https://www.google.com',
      source: window.parent,
    });
    window.dispatchEvent(handshakeEvent);
    await microtasksFinished();

    postedMessages = [];

    // Trigger postAimMessage mojo event.
    testProxy.callbackRouterRemote.postAimMessage([9, 8, 7]);
    await microtasksFinished();

    assertEquals(1, postedMessages.length);
    const aimMessage = postedMessages[0]!;
    assertEquals('https://www.google.com', aimMessage.targetOrigin);
    assertTrue(aimMessage.message instanceof Uint8Array);
    assertDeepEquals([9, 8, 7], Array.from(aimMessage.message));
  });

  test(
      'Cleans up window.stateForTesting and sets isDestroyed on destroy',
      () => {
        assertTrue(window.stateForTesting !== undefined);
        assertFalse(handler.isDestroyed);

        handler.destroy();

        assertTrue(handler.isDestroyed);
        assertEquals(undefined, window.stateForTesting);
      });

  test(
      'Does not start interval or post pings if destroyed during handshake',
      async () => {
        // Destroy the default test handler to avoid background pings.
        handler.destroy();

        let resolveHandshake: (value: {
          message: {protoName: string, smuggled: {bytes: number[]}},
        }) => void;
        const pendingPromise = new Promise<{
          message: {protoName: string, smuggled: {bytes: number[]}},
        }>(resolve => {
          resolveHandshake = resolve;
        });

        const secondProxy = new TestExtensionBrowserProxy();
        secondProxy.handler.getHandshakeMessage = () => {
          secondProxy.handler.methodCalled('getHandshakeMessage');
          return pendingPromise;
        };

        postedMessages = [];
        const secondHandler = new ExtensionPostMessageHandler(secondProxy);
        await secondProxy.handler.whenCalled('getHandshakeMessage');

        // Destroy before handshake promise resolves.
        secondHandler.destroy();
        assertTrue(secondHandler.isDestroyed);

        // Resolve handshake promise after destruction.
        resolveHandshake!({
          message: {
            protoName: '',
            smuggled: {
              bytes: [4, 5, 6],
            },
          },
        });
        await microtasksFinished();

        // Wait to verify no ping was posted.
        await new Promise(resolve => setTimeout(resolve, 50));
        assertEquals(0, postedMessages.length);
      });
});
