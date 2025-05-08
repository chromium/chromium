// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/side_panel/post_message_communication.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {MessageType, ParamType, PostMessageReceiver} from 'chrome-untrusted://lens/side_panel/post_message_communication.js';
import {SidePanelBrowserProxyImpl} from 'chrome-untrusted://lens/side_panel/side_panel_browser_proxy.js';
import {assertDeepEquals, assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';

import {TestLensSidePanelBrowserProxy} from './test_side_panel_browser_proxy.js';

const RESULTS_SEARCH_URL = 'https://www.google.com/search';
const RESULTS_SEARCH_URL_ORIGIN = 'https://www.google.com';

suite('PostMessageCommunication', () => {
  let testBrowserProxy: TestLensSidePanelBrowserProxy;
  let postMessageReceiver: PostMessageReceiver;

  function dispatchMessageEvent(data: string, origin: string) {
    // Dispatch a MessageEvent with the data and origin.
    window.dispatchEvent(new MessageEvent('message', {
      data: data,
      origin: origin,
    }));
  }

  setup(() => {
    testBrowserProxy = new TestLensSidePanelBrowserProxy();
    SidePanelBrowserProxyImpl.setInstance(testBrowserProxy);

    loadTimeData.overrideValues({
      'scrollToEnabled': true,
      'resultsSearchURL': RESULTS_SEARCH_URL,
    });

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    postMessageReceiver = new PostMessageReceiver(testBrowserProxy);
  });

  test('ListenCallsCorrectProxyFunctions', async () => {
    postMessageReceiver.listen();

    const textFragments = ['hello', 'world'];
    const pdfPageNumber = 0;
    const data = JSON.stringify({
      [ParamType.MESSAGE_TYPE]: MessageType.SCROLL_TO,
      [ParamType.TEXT_FRAGMENTS]: textFragments,
      [ParamType.PDF_PAGE_NUMBER]: pdfPageNumber,
    });

    dispatchMessageEvent(data, RESULTS_SEARCH_URL_ORIGIN);
    await testBrowserProxy.handler.whenCalled('onScrollToMessage');
    const [fragment, page] =
        testBrowserProxy.handler.getArgs('onScrollToMessage')[0];
    assertDeepEquals(textFragments, fragment);
    assertEquals(pdfPageNumber, page);
  });

  test('ListenOnlyWorksIfScrollToEnabled', () => {
    loadTimeData.overrideValues({'scrollToEnabled': false});
    postMessageReceiver.listen();

    const textFragments = ['hello', 'world'];
    const pdfPageNumber = 0;
    const data = JSON.stringify({
      [ParamType.MESSAGE_TYPE]: MessageType.SCROLL_TO,
      [ParamType.TEXT_FRAGMENTS]: textFragments,
      [ParamType.PDF_PAGE_NUMBER]: pdfPageNumber,
    });

    dispatchMessageEvent(data, RESULTS_SEARCH_URL_ORIGIN);
    assertEquals(0, testBrowserProxy.handler.getCallCount('onScrollToMessage'));
  });

  test('ListenOnlyWorksForExpectedOrigin', () => {
    postMessageReceiver.listen();

    const textFragments = ['hello', 'world'];
    const pdfPageNumber = 0;
    const data = JSON.stringify({
      [ParamType.MESSAGE_TYPE]: MessageType.SCROLL_TO,
      [ParamType.TEXT_FRAGMENTS]: textFragments,
      [ParamType.PDF_PAGE_NUMBER]: pdfPageNumber,
    });

    dispatchMessageEvent(data, 'https://www.example.com');
    assertEquals(0, testBrowserProxy.handler.getCallCount('onScrollToMessage'));
  });

  test('DetachRemovesAllListeners', () => {
    postMessageReceiver.listen();
    postMessageReceiver.detach();

    const textFragments = ['hello', 'world'];
    const pdfPageNumber = 0;
    const data = JSON.stringify({
      [ParamType.MESSAGE_TYPE]: MessageType.SCROLL_TO,
      [ParamType.TEXT_FRAGMENTS]: textFragments,
      [ParamType.PDF_PAGE_NUMBER]: pdfPageNumber,
    });

    dispatchMessageEvent(data, RESULTS_SEARCH_URL_ORIGIN);
    assertEquals(0, testBrowserProxy.handler.getCallCount('onScrollToMessage'));
  });
});
