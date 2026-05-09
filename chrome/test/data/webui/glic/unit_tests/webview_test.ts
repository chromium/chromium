// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BrowserProxy} from 'chrome://glic/browser_proxy.js';
import {ZoomAction} from 'chrome://glic/glic.mojom-webui.js';
import type {PageHandlerInterface} from 'chrome://glic/glic.mojom-webui.js';
import type {ApiHostEmbedder} from 'chrome://glic/glic_api_impl/host/glic_api_host.js';
import {matcherForOrigin, urlMatchesAllowedOrigin, WebviewController, WebviewPersistentState} from 'chrome://glic/webview.js';
import type {PageType, WebviewDelegate} from 'chrome://glic/webview.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('WebviewTest', () => {
  setup(() => {
    loadTimeData.resetForTesting({
      glicAllowedOrigins: '',
      glicGuestURL: 'https://cat.fun/',
      devMode: false,
    });
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
    loadTimeData.resetForTesting({
      glicAllowedOrigins: '',
      glicGuestURL: 'https://cat.fun/',
      devMode: false,
      chromeVersion: '123.0.0.0',
      chromeChannel: 'stable',
      glicHeaderRequestTypes: '',
    });

    // Set up mock interfaces to enable creating a WebviewController for test
    // use.
    const container = document.createElement('div');

    class FakePageHandler implements Partial<PageHandlerInterface> {
      webviewCommitted(_url: string) {}
      onZoomLevelChange(_zoomFactor: number) {}
      prepareForClient() {
        return Promise.resolve({result: 0});
      }
    }

    class FakeBrowserProxy implements BrowserProxy {
      pageHandler = new FakePageHandler() as PageHandlerInterface;
    }

    class FakeWebviewDelegate implements WebviewDelegate {
      webviewError(_reason: string) {}
      webviewUnresponsive() {}
      webviewPageCommit(_pageType: PageType) {}
      webviewDeniedByAdmin() {}
    }

    class FakeApiHostEmbedder implements ApiHostEmbedder {
      onGuestResizeRequest(_size: {width: number, height: number}) {}
      enableDragResize(_enabled: boolean) {}
      webClientReady() {}
      webClientWarmed() {}
      getZoom() {
        return Promise.resolve(1.0);
      }
    }

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
    const webview = controller.webview as any;
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
    const webview = controller.webview as any;
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
    const webview = controller.webview as any;
    webview.setZoom = (currentZoom: number) => {
      lastSetZoom = currentZoom;
    };

    controller.zoom(ZoomAction.kReset);
    assertEquals(1.0, lastSetZoom);
  });

  test('ZoomBoundaryConditions', () => {
    let lastSetZoom = 2.0;
    let setZoomCalled = false;
    const webview = controller.webview as any;
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
});
