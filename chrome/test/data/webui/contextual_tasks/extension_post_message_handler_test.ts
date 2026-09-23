// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ExtensionBrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {ExtensionPostMessageHandler, resetExtensionPostMessagingForTesting, urlMatchesAllowList} from 'chrome://contextual-tasks/contextual_tasks_extension/post_message_handler.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestExtensionBrowserProxy} from './test_contextual_tasks_browser_proxy.js';

suite('ExtensionPostMessageHandlerTest', () => {
  let handler: ExtensionPostMessageHandler;
  let testProxy: TestExtensionBrowserProxy;
  let postedMessages: Array<{message: Uint8Array, targetOrigin: string}>;
  let originalPostMessage: typeof window.parent.postMessage;
  let messageResolver: PromiseResolver<void>;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    resetExtensionPostMessagingForTesting();

    Object.defineProperty(document, 'referrer', {
      value: 'https://www.google.com',
      configurable: true,
    });

    postedMessages = [];
    messageResolver = new PromiseResolver<void>();
    originalPostMessage = window.parent.postMessage;
    window.parent.postMessage =
        (message: unknown,
         targetOriginOrOptions?: string|WindowPostMessageOptions) => {
          const targetOrigin = typeof targetOriginOrOptions === 'string' ?
              targetOriginOrOptions :
              (targetOriginOrOptions?.targetOrigin || '*');
          postedMessages.push({message: message as Uint8Array, targetOrigin});
          messageResolver.resolve();
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
    await messageResolver.promise;
    assertTrue(postedMessages.length > 0);
    const firstMessage = postedMessages[0]!;
    assertEquals('https://www.google.com', firstMessage.targetOrigin);
    assertTrue(firstMessage.message instanceof Uint8Array);
    assertDeepEquals([1, 2, 3], Array.from(firstMessage.message));
  });

  test('Completes handshake on onHandshakeComplete mojo event', async () => {
    assertFalse(handler.handshakeCompleted);

    testProxy.callbackRouterRemote.onHandshakeComplete();
    await microtasksFinished();

    assertTrue(handler.handshakeCompleted);
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

  test('Ignores messages from non-allowlisted Google subdomains', async () => {
    const testData = new Uint8Array([40, 50]);
    const messageEvent = new MessageEvent('message', {
      data: testData.buffer,
      origin: 'https://sites.google.com',
      source: window.parent,
    });

    window.dispatchEvent(messageEvent);
    await microtasksFinished();

    assertEquals(0, testProxy.handler.getCallCount('onWebviewMessage'));
  });

  test('Rejects oversized webview messages exceeding limit', async () => {
    const oversizedData = new Uint8Array(1024 * 1024 + 1);
    const messageEvent = new MessageEvent('message', {
      data: oversizedData.buffer,
      origin: 'https://www.google.com',
      source: window.parent,
    });

    window.dispatchEvent(messageEvent);
    await microtasksFinished();

    assertEquals(0, testProxy.handler.getCallCount('onWebviewMessage'));
  });

  test('Validates origins with urlMatchesAllowList', () => {
    assertTrue(urlMatchesAllowList('https://google.com'));
    assertTrue(urlMatchesAllowList('https://www.google.com'));
    assertTrue(urlMatchesAllowList('https://search.corp.google.com'));
    assertTrue(urlMatchesAllowList('https://aim.prod.google.com'));
    assertTrue(urlMatchesAllowList('https://aim.borg.google.com'));

    assertFalse(urlMatchesAllowList('http://google.com'));
    assertFalse(urlMatchesAllowList('https://drive.google.com'));
    assertFalse(urlMatchesAllowList('https://sites.google.com'));
    assertFalse(urlMatchesAllowList('https://example.com'));
    assertFalse(urlMatchesAllowList('null'));
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
    testProxy.callbackRouterRemote.onHandshakeComplete();
    await microtasksFinished();

    postedMessages = [];
    messageResolver = new PromiseResolver<void>();

    testProxy.callbackRouterRemote.postAimMessage([9, 8, 7]);
    await messageResolver.promise;

    assertEquals(1, postedMessages.length);
    const aimMessage = postedMessages[0]!;
    assertEquals('https://www.google.com', aimMessage.targetOrigin);
    assertTrue(aimMessage.message instanceof Uint8Array);
    assertDeepEquals([9, 8, 7], Array.from(aimMessage.message));
  });

  test(
      'postSearchMessage forwards message to parent window when target ' +
          'origin is set',
      async () => {
        postedMessages = [];
        messageResolver = new PromiseResolver<void>();

        const testBytes = [10, 4, 1, 2, 3, 4];
        testProxy.callbackRouterRemote.postSearchMessage({
          protoName: 'lens.chrome.ClientToSearchMessage',
          smuggled: {bytes: testBytes},
        });
        await messageResolver.promise;

        assertEquals(1, postedMessages.length);
        const searchMessage = postedMessages[0]!;
        assertEquals('https://www.google.com', searchMessage.targetOrigin);
        assertTrue(searchMessage.message instanceof Uint8Array);
        assertDeepEquals(testBytes, Array.from(searchMessage.message));
      });

  test(
      'postSearchMessage queues message until target origin is set',
      async () => {
        handler.setTargetOriginForTesting(null);
        postedMessages = [];

        const testBytes = [10, 4, 1, 2, 3, 4];
        testProxy.callbackRouterRemote.postSearchMessage({
          protoName: 'lens.chrome.ClientToSearchMessage',
          smuggled: {bytes: testBytes},
        });
        await microtasksFinished();

        // Message should be queued, not dispatched to wildcard.
        assertEquals(0, postedMessages.length);

        // Setting a valid allowlisted origin flushes the queued message.
        messageResolver = new PromiseResolver<void>();
        handler.setTargetOriginForTesting('https://www.google.com');
        await messageResolver.promise;

        assertEquals(1, postedMessages.length);
        const searchMessage = postedMessages[0]!;
        assertEquals('https://www.google.com', searchMessage.targetOrigin);
        assertDeepEquals(testBytes, Array.from(searchMessage.message));
      });

  test('postSearchMessage rejects untrusted target origin', async () => {
    handler.setTargetOriginForTesting(null);
    postedMessages = [];

    const testBytes = [10, 4, 1, 2, 3, 4];
    testProxy.callbackRouterRemote.postSearchMessage({
      protoName: 'lens.chrome.ClientToSearchMessage',
      smuggled: {bytes: testBytes},
    });
    await microtasksFinished();

    // Setting an untrusted origin should be rejected by setTargetOrigin_.
    handler.setTargetOriginForTesting('https://evil.com');
    await microtasksFinished();

    assertEquals(0, postedMessages.length);
  });

  test('Cleans up postSearchMessage listener on destroy', async () => {
    handler.destroy();
    postedMessages = [];

    testProxy.callbackRouterRemote.postSearchMessage({
      protoName: 'lens.chrome.ClientToSearchMessage',
      smuggled: {bytes: [1, 2, 3]},
    });
    await microtasksFinished();

    assertEquals(0, postedMessages.length);
  });

  test('Sets isDestroyed on destroy', () => {
    assertFalse(handler.isDestroyed);
    handler.destroy();
    assertTrue(handler.isDestroyed);
  });

  test(
      'Does not start interval or post pings if destroyed during handshake',
      async () => {
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

        secondHandler.destroy();
        assertTrue(secondHandler.isDestroyed);

        resolveHandshake!({
          message: {
            protoName: '',
            smuggled: {
              bytes: [4, 5, 6],
            },
          },
        });
        await microtasksFinished();

        assertEquals(0, postedMessages.length);
      });
});
