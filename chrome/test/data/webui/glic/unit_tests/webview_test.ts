// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {matcherForOrigin, urlMatchesAllowedOrigin, WebviewController, WebviewPersistentState, ZoomAction} from 'chrome://glic/glic.js';
import type {CrA11yAnnouncerMessagesSentEvent} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

// Mock the `zoomchange` event defined in the chrome webviewTag API.
interface WebViewZoomChangeEvent extends Event {
  newZoomFactor: number;
  oldZoomFactor?: number;
}

import {configureLoadTimeData, FakeApiHostEmbedder, FakeBrowserProxy, FakeWebviewDelegate} from './test_helpers.js';

suite('WebviewTest', () => {
  setup(() => {
    configureLoadTimeData();
  });

  function assertUrlMatchesAllowedOrigin(expectMatches: boolean, url: string) {
    assertEquals(
        expectMatches, urlMatchesAllowedOrigin(url),
        `urlMatchesAllowedOrigin("${url}")`);
  }

  test('matcherForOrigin works', () => {
    assertFalse(!!matcherForOrigin(''));
    assertFalse(!!matcherForOrigin('fun'));
    assertFalse(!!matcherForOrigin('cat.fun'));

    let result = matcherForOrigin('https://cat.fun');
    assertTrue(!!result);
    assertEquals('cat.fun', result?.hostname);
    assertEquals('https', result?.protocol);

    result = matcherForOrigin('http://cat.fun');
    assertTrue(!!result);
    assertEquals('cat.fun', result?.hostname);
    assertEquals('http', result?.protocol);

    result = matcherForOrigin('http://cat.fun:42');
    assertTrue(!!result);
    assertEquals('cat.fun', result?.hostname);
    assertEquals('http', result?.protocol);
    assertEquals('42', result?.port);

    result = matcherForOrigin('http://cat.fun:*');
    assertTrue(!!result);
    assertEquals('cat.fun', result?.hostname);
    assertEquals('http', result?.protocol);
    assertEquals('*', result?.port);

    result = matcherForOrigin('http://cat.fun/://foo://');
    assertTrue(!!result);
    assertEquals('cat.fun', result?.hostname);
    assertEquals('http', result?.protocol);
  });

  test('urlMatchesAllowedOrigin allows the primary url', () => {
    loadTimeData.overrideValues({
      glicAllowedOrigins: '',
      glicGuestURL: 'https://cat.fun/party',
    });
    assertUrlMatchesAllowedOrigin(true, 'https://cat.fun/party');
    assertUrlMatchesAllowedOrigin(true, 'https://cat.fun/disaster');
    assertUrlMatchesAllowedOrigin(true, 'https://cat.fun/');
    assertUrlMatchesAllowedOrigin(false, 'https://dog.fun/');
    assertUrlMatchesAllowedOrigin(false, 'http://cat.fun/');
  });

  test('urlMatchesAllowedOrigin allows allowed origins', () => {
    loadTimeData.overrideValues({
      glicAllowedOrigins: 'https://*.mouse.org https://dog.com',
      glicGuestURL: 'https://cat.fun/party',
    });

    assertUrlMatchesAllowedOrigin(true, 'https://sub.mouse.org/party');
    assertUrlMatchesAllowedOrigin(true, 'https://inner.sub.mouse.org/party');
    assertUrlMatchesAllowedOrigin(false, 'https://mouse.org');
    assertUrlMatchesAllowedOrigin(false, 'https://amouse.org');

    assertUrlMatchesAllowedOrigin(true, 'https://dog.com/party');
    assertUrlMatchesAllowedOrigin(true, 'https://dog.com:99/party');
    assertUrlMatchesAllowedOrigin(false, 'http://dog.com/party');
  });

  test('urlMatchesAllowedOrigin allows http', () => {
    loadTimeData.overrideValues({
      glicAllowedOrigins: '',
      glicGuestURL: 'http://test.com',
    });

    assertUrlMatchesAllowedOrigin(true, 'http://test.com');
    assertUrlMatchesAllowedOrigin(false, 'https://test.com');
    assertUrlMatchesAllowedOrigin(false, 'http://other.com');
  });
});

suite('WebviewZoomTest', () => {
  let controller: WebviewController;

  setup(() => {
    configureLoadTimeData();

    // Set up mock interfaces to enable creating a WebviewController for test
    // use.
    const container = document.createElement('div');

    controller = new WebviewController(
        container,
        new FakeBrowserProxy(),
        new FakeWebviewDelegate(),
        new FakeApiHostEmbedder(),
        new WebviewPersistentState(),
    );
  });

  test('ZoomInReturnsNextZoomFactor', () => {
    let lastSetZoom = 1.0;
    const webview = controller.webview;
    webview.getZoom = (cb: (z: number) => void) => cb(lastSetZoom);
    webview.setZoom = (z: number) => {
      lastSetZoom = z;
    };

    controller.zoom(ZoomAction.kZoomIn);
    assertEquals(1.1, lastSetZoom);

    controller.zoom(ZoomAction.kZoomIn);
    assertEquals(1.25, lastSetZoom);
  });

  test('ZoomOutReturnsPreviousZoomFactor', () => {
    let lastSetZoom = 1.25;
    const webview = controller.webview;
    webview.getZoom = (cb: (z: number) => void) => cb(lastSetZoom);
    webview.setZoom = (currentZoom: number) => {
      lastSetZoom = currentZoom;
    };

    controller.zoom(ZoomAction.kZoomOut);
    assertEquals(1.1, lastSetZoom);

    controller.zoom(ZoomAction.kZoomOut);
    assertEquals(1.0, lastSetZoom);
  });

  test('ZoomResetReturnsOne', () => {
    let lastSetZoom = 1.5;
    const webview = controller.webview;
    webview.setZoom = (currentZoom: number) => {
      lastSetZoom = currentZoom;
    };

    controller.zoom(ZoomAction.kReset);
    assertEquals(1.0, lastSetZoom);
  });

  test('ZoomBoundaryConditions', () => {
    let lastSetZoom = 2.0;
    let setZoomCalled = false;
    const webview = controller.webview;
    webview.getZoom = (cb: (z: number) => void) => cb(lastSetZoom);
    webview.setZoom = (currentZoom: number) => {
      lastSetZoom = currentZoom;
      setZoomCalled = true;
    };

    // At 2.0, ZoomIn action should not result in a call to setZoom.
    controller.zoom(ZoomAction.kZoomIn);
    assertFalse(setZoomCalled);

    // At 1.0, ZoomOut action should not result in a call to setZoom.
    lastSetZoom = 1.0;
    setZoomCalled = false;
    controller.zoom(ZoomAction.kZoomOut);
    assertFalse(setZoomCalled);
  });

  test('ZoomAnnouncementMade', async () => {
    const announcementPromise =
        eventToPromise<CrA11yAnnouncerMessagesSentEvent>(
            'cr-a11y-announcer-messages-sent', document.body);

    // Simulate a zoom change to 125%
    const zoomEvent = new Event('zoomchange') as WebViewZoomChangeEvent;
    zoomEvent.newZoomFactor = 1.25;
    controller.webview.dispatchEvent(zoomEvent);

    const event = await announcementPromise;
    assertDeepEquals(event.detail.messages, ['Zoom: 125%']);
  });
});
