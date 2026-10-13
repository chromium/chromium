// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ExtensionBrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {ExtensionPostMessageHandler, resetExtensionPostMessagingForTesting, urlMatchesAllowList} from 'chrome://contextual-tasks/contextual_tasks_extension/post_message_handler.js';
import {HANDSHAKE_INTERVAL_MS, HANDSHAKE_TIMEOUT_MS} from 'chrome://contextual-tasks/utils.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestExtensionBrowserProxy} from './test_contextual_tasks_browser_proxy.js';

// Cache the real setTimeout before any MockTimer overrides it.
const realSetTimeout = window.setTimeout;

// microtasksFinished() is backed by setTimeout, which never resolves while
// MockTimer is installed. This flushes pending promises using the real timer
// instead.
function flushTasks(): Promise<void> {
  return new Promise(resolve => realSetTimeout(resolve, 0));
}

suite('ExtensionPostMessageHandlerTest', () => {
  let handler: ExtensionPostMessageHandler;
  let testProxy: TestExtensionBrowserProxy;
  let postedMessages: Array<{message: Uint8Array, targetOrigin: string}>;
  let originalPostMessage: typeof window.parent.postMessage;
  let messageResolver: PromiseResolver<void>;
  let mockTimer: MockTimer|null = null;
  // MockTimer drives setInterval but not performance.now(), so handlers that
  // need a controllable clock are constructed with `fakeClock`.
  let fakeNow = 0;
  const fakeClock = () => fakeNow;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    resetExtensionPostMessagingForTesting();
    fakeNow = 0;

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
    // Uninstall first and unconditionally: if an assertion fails before a test
    // can clean up its own timer, leaving setTimeout mocked would hang every
    // subsequent test in the suite.
    if (mockTimer) {
      mockTimer.uninstall();
      mockTimer = null;
    }
    handler.destroy();
    resetExtensionPostMessagingForTesting();
    window.parent.postMessage = originalPostMessage;
  });

  // Replaces the setup() handler with one driven by MockTimer and fakeClock.
  async function createClockedHandler(): Promise<ExtensionPostMessageHandler> {
    handler.destroy();
    mockTimer = new MockTimer();
    mockTimer.install();
    const proxy = new TestExtensionBrowserProxy();
    const clockedHandler = new ExtensionPostMessageHandler(proxy, fakeClock);
    await proxy.handler.whenCalled('getHandshakeMessage');
    await flushTasks();
    postedMessages = [];
    return clockedHandler;
  }

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

  test(
      'Caches default ExtensionBrowserProxyImpl on window across bundles',
      () => {
        delete window.__extensionBrowserProxyInstance;

        const mojoGlobal = Mojo as {bindInterface: typeof Mojo.bindInterface};
        const originalBindInterface = mojoGlobal.bindInterface;
        mojoGlobal.bindInterface = () => {};
        try {
          const defaultHandler = new ExtensionPostMessageHandler();
          defaultHandler.destroy();

          assertTrue(!!window.__extensionBrowserProxyInstance);
          assertEquals(
              window.__extensionBrowserProxyInstance,
              ExtensionBrowserProxyImpl.getInstance());
        } finally {
          mojoGlobal.bindInterface = originalBindInterface;
        }
      });

  test('Stops pinging at the handshake deadline', async () => {
    const clockedHandler = await createClockedHandler();

    mockTimer!.tick(HANDSHAKE_INTERVAL_MS * 3);
    assertEquals(3, postedMessages.length);

    fakeNow = HANDSHAKE_TIMEOUT_MS;
    mockTimer!.tick(HANDSHAKE_INTERVAL_MS * 5);
    assertEquals(3, postedMessages.length);

    clockedHandler.destroy();
  });

  test(
      'Stops the handshake at the deadline even if no origin resolves',
      async () => {
        // With no usable referrer no ping is ever sent, which is the case a
        // ping-count cap never terminated.
        Object.defineProperty(document, 'referrer', {
          value: 'https://not-allowed.example',
          configurable: true,
        });
        const clockedHandler = await createClockedHandler();

        fakeNow = HANDSHAKE_TIMEOUT_MS;
        mockTimer!.tick(HANDSHAKE_INTERVAL_MS);

        // An origin resolving after the deadline must not restart pinging,
        // which proves the interval was actually stopped.
        clockedHandler.setTargetOriginForTesting('https://www.google.com');
        mockTimer!.tick(HANDSHAKE_INTERVAL_MS * 5);
        assertEquals(0, postedMessages.length);

        clockedHandler.destroy();
      });

  test('Times out by elapsed time, not by number of ticks', async () => {
    const clockedHandler = await createClockedHandler();

    // Far more ticks than fit in the timeout at the nominal interval, as
    // happens when a hidden frame's timers are throttled, but the clock has
    // not reached the deadline, so pinging continues.
    fakeNow = HANDSHAKE_TIMEOUT_MS - 1;
    const ticks = HANDSHAKE_TIMEOUT_MS / HANDSHAKE_INTERVAL_MS + 10;
    mockTimer!.tick(HANDSHAKE_INTERVAL_MS * ticks);
    assertEquals(ticks, postedMessages.length);

    fakeNow = HANDSHAKE_TIMEOUT_MS;
    mockTimer!.tick(HANDSHAKE_INTERVAL_MS * 5);
    assertEquals(ticks, postedMessages.length);

    clockedHandler.destroy();
  });
});
