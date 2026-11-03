// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/side_panel/post_message_communication.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import type {BrowserProxy, LensSidePanelPageRemote} from 'chrome-untrusted://lens/lens_side_panel.mojom-webui.js';
import {browserProxyFactory} from 'chrome-untrusted://lens/lens_side_panel.mojom-webui.js';
import {MessageType, ParamType, PostMessageReceiver} from 'chrome-untrusted://lens/side_panel/post_message_communication.js';
import {assertDeepEquals, assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
import {MockTimer} from 'chrome-untrusted://webui-test/mock_timer.js';
import {flushTasks} from 'chrome-untrusted://webui-test/polymer_test_util.js';

import {TestLensSidePanelPageHandler} from './test_side_panel_browser_proxy.js';

const RESULTS_SEARCH_URL = 'https://www.google.com/search';
const RESULTS_SEARCH_URL_ORIGIN = 'https://www.google.com';

const SERIALIZED_HANDSHAKE_PING_MESSAGE = Uint8Array.from([10, 3, 10, 1, 0]);

suite('PostMessageCommunication', () => {
  let handler: TestLensSidePanelPageHandler;
  let browserProxy: BrowserProxy;
  let pageRemote: LensSidePanelPageRemote;
  let postMessageReceiver: PostMessageReceiver;
  let resultsFrame: HTMLIFrameElement;

  function dispatchMessageEvent(data: string, origin: string) {
    // Dispatch a MessageEvent with the data and origin.
    window.dispatchEvent(new MessageEvent('message', {
      data: data,
      origin: origin,
    }));
  }

  setup(() => {
    handler = new TestLensSidePanelPageHandler();
    const {instance, remote} = browserProxyFactory.createForTest(handler);
    browserProxy = instance;
    pageRemote = remote;
    browserProxyFactory.setInstance(browserProxy);

    loadTimeData.overrideValues({
      'resultsSearchURL': RESULTS_SEARCH_URL,
      'enableAimSearchbox': true,
    });

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    resultsFrame = document.createElement('iframe');
    document.body.appendChild(resultsFrame);
    postMessageReceiver = new PostMessageReceiver(browserProxy, resultsFrame);
  });

  test('ListenCallsCorrectProxyFunctions', async () => {
    const textFragments = ['hello', 'world'];
    const pdfPageNumber = 0;
    const data = JSON.stringify({
      [ParamType.MESSAGE_TYPE]: MessageType.SCROLL_TO,
      [ParamType.TEXT_FRAGMENTS]: textFragments,
      [ParamType.PDF_PAGE_NUMBER]: pdfPageNumber,
    });

    dispatchMessageEvent(data, RESULTS_SEARCH_URL_ORIGIN);
    await handler.whenCalled('onScrollToMessage');
    const [fragment, page] = handler.getArgs('onScrollToMessage')[0];
    assertDeepEquals(textFragments, fragment);
    assertEquals(pdfPageNumber, page);
  });

  test('ListenCallsCorrectProxyFunctionsAimDisabled', async () => {
    loadTimeData.overrideValues({
      'enableAimSearchbox': false,
    });
    // Recreate post message receiver to use new load time data.
    postMessageReceiver.detach();
    postMessageReceiver = new PostMessageReceiver(browserProxy, resultsFrame);

    const textFragments = ['hello', 'world'];
    const pdfPageNumber = 0;
    const data = JSON.stringify({
      [ParamType.MESSAGE_TYPE]: MessageType.SCROLL_TO,
      [ParamType.TEXT_FRAGMENTS]: textFragments,
      [ParamType.PDF_PAGE_NUMBER]: pdfPageNumber,
    });

    dispatchMessageEvent(data, RESULTS_SEARCH_URL_ORIGIN);
    await handler.whenCalled('onScrollToMessage');
    const [fragment, page] = handler.getArgs('onScrollToMessage')[0];
    assertDeepEquals(textFragments, fragment);
    assertEquals(pdfPageNumber, page);
  });

  test('ListenOnlyWorksForExpectedOrigin', () => {
    const textFragments = ['hello', 'world'];
    const pdfPageNumber = 0;
    const data = JSON.stringify({
      [ParamType.MESSAGE_TYPE]: MessageType.SCROLL_TO,
      [ParamType.TEXT_FRAGMENTS]: textFragments,
      [ParamType.PDF_PAGE_NUMBER]: pdfPageNumber,
    });

    dispatchMessageEvent(data, 'https://www.example.com');
    assertEquals(0, handler.getCallCount('onScrollToMessage'));
  });

  test('DetachRemovesAllListeners', () => {
    postMessageReceiver.detach();

    const textFragments = ['hello', 'world'];
    const pdfPageNumber = 0;
    const data = JSON.stringify({
      [ParamType.MESSAGE_TYPE]: MessageType.SCROLL_TO,
      [ParamType.TEXT_FRAGMENTS]: textFragments,
      [ParamType.PDF_PAGE_NUMBER]: pdfPageNumber,
    });

    dispatchMessageEvent(data, RESULTS_SEARCH_URL_ORIGIN);
    assertEquals(0, handler.getCallCount('onScrollToMessage'));
  });

  test('SendMessagePostMessagesToIframe', async () => {
    const message = [1, 2, 3, 4];

    // Setup postMessage listener on the iframe.
    let receivedMessage: unknown;
    let receivedOrigin: string = '';

    resultsFrame.contentWindow!.postMessage = (message, origin) => {
      receivedMessage = message;
      receivedOrigin = origin as string;
    };

    // Call onSendMessage to simulate a message being sent from the browser.
    pageRemote.sendClientMessageToAim(message);
    await flushTasks();

    // Verify that the message was sent to the iframe.
    assertDeepEquals(message, receivedMessage);
    assertEquals(RESULTS_SEARCH_URL_ORIGIN, receivedOrigin);
  });
});

// Mock timer effects the testBrowserProxy, so create a separate suite for it.
suite('PostMessageCommunicationWithMockTimer', () => {
  let pageRemote: LensSidePanelPageRemote;
  let postMessageReceiver: PostMessageReceiver;
  let resultsFrame: HTMLIFrameElement;
  const mockTimer = new MockTimer();

  let postMessageCallCount: number = 0;
  let lastReceivedPostMessageData: unknown;
  let lastReceivedPostMessageOrigin: string = '';

  setup(() => {
    const {instance, remote} =
        browserProxyFactory.createForTest(new TestLensSidePanelPageHandler());
    pageRemote = remote;
    browserProxyFactory.setInstance(instance);

    loadTimeData.overrideValues({
      'resultsSearchURL': RESULTS_SEARCH_URL,
    });
    loadTimeData.overrideValues({
      'handshakeMessage':
          String.fromCodePoint(...SERIALIZED_HANDSHAKE_PING_MESSAGE),
    });

    // Install the mock timer before the component that uses it is created.
    mockTimer.install();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    resultsFrame = document.createElement('iframe');
    document.body.appendChild(resultsFrame);
    postMessageReceiver = new PostMessageReceiver(instance, resultsFrame);

    // Setup a mock post message method to spy on the post message calls.
    postMessageCallCount = 0;
    lastReceivedPostMessageData = null;
    lastReceivedPostMessageOrigin = '';
    resultsFrame.contentWindow!.postMessage = (message, origin) => {
      postMessageCallCount++;
      lastReceivedPostMessageData = message;
      lastReceivedPostMessageOrigin = origin as string;
    };
  });

  teardown(() => {
    mockTimer.uninstall();
    postMessageReceiver.detach();
  });

  test('AimHandshakeIsSentUntilReceived', async () => {
    // Mock sending the load event to the results frame to start the handshake.
    resultsFrame.dispatchEvent(new Event('load'));

    // Let one interval pass.
    mockTimer.tick(500);
    assertEquals(1, postMessageCallCount);
    assertDeepEquals(
        SERIALIZED_HANDSHAKE_PING_MESSAGE, lastReceivedPostMessageData);
    assertEquals(RESULTS_SEARCH_URL_ORIGIN, lastReceivedPostMessageOrigin);

    // Let another interval pass.
    mockTimer.tick(500);
    assertEquals(2, postMessageCallCount);

    // Simulate handshake received.
    pageRemote.aimHandshakeReceived();
    await pageRemote.$.flushForTesting();

    // Let a few more intervals pass and make sure post message is not called.
    mockTimer.tick(1000);
    assertEquals(2, postMessageCallCount);
  });

  test('LoadEventRestartsHandshake', async () => {
    // Mock sending the load event to the results frame to start the handshake.
    resultsFrame.dispatchEvent(new Event('load'));

    // Let one interval pass.
    mockTimer.tick(500);
    assertEquals(1, postMessageCallCount);
    assertDeepEquals(
        SERIALIZED_HANDSHAKE_PING_MESSAGE, lastReceivedPostMessageData);
    assertEquals(RESULTS_SEARCH_URL_ORIGIN, lastReceivedPostMessageOrigin);

    // Simulate handshake received.
    pageRemote.aimHandshakeReceived();
    await pageRemote.$.flushForTesting();

    // Let another interval pass. Handhsake should not be sent again.
    mockTimer.tick(500);
    assertEquals(1, postMessageCallCount);

    // Mock sending the load event to the results frame to restart the
    // handshake.
    resultsFrame.dispatchEvent(new Event('load'));

    // Let a few more intervals pass and make sure post message is not called.
    mockTimer.tick(1000);
    assertEquals(3, postMessageCallCount);
  });
});
